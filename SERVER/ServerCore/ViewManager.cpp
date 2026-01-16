#include "pch.h"
#include "ViewManager.h"

ViewManager::ViewManager(const std::shared_ptr<Service>& service) : _service(service)
{
}

ViewListDiff ViewManager::SyncViewList(const std::shared_ptr<GameSession>& session) const
{
	ViewListDiff viewListDiff;

	// 1. newViewList Get
	std::unordered_set<int> newViewList = CollectViewList(session);

	// 2. Session의 기존 viewList Get
	const auto oldViewList = *(session->GetViewList());

	// 3. Add : newViewList - oldViewList
	for (int id : newViewList) {
		if (not oldViewList.contains(id)) {
			viewListDiff.addViewList.push_back(id);
		}
	}

	// 4. Remove : oldViewList - newViewList
	for (int id : oldViewList) {
		if (not newViewList.contains(id)) {
			viewListDiff.removeViewList.push_back(id);
		}

		// 5. Move : oldViewList and newViewList
		else {
			viewListDiff.moveViewList.push_back(id);
		}
	}

	// 6. Session의 viewList Update
	session->SetViewList(newViewList);
	
	return viewListDiff;
}

ViewListDiff ViewManager::SyncViewList(const std::unordered_set<int>& oldViewList, const std::unordered_set<int>& newViewList)
{
	ViewListDiff viewListDiff;

	for (int id : newViewList) {
		if (not oldViewList.contains(id)) {
			viewListDiff.addViewList.push_back(id);
		}
	}

	for (int id : oldViewList) {
		if (not newViewList.contains(id)) {
			viewListDiff.removeViewList.push_back(id);
		}

		else {
			viewListDiff.moveViewList.push_back(id);
		}
	}

	return viewListDiff;
}

void ViewManager::HandlePlayerLoginNotify(const std::shared_ptr<GameSession>& session)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	// 1. Login한 Player의 시야에 걸리는 Sector Range
	auto [xRange, yRange] = Sector::GetSectorRange(session->GetX(), session->GetY());

	// 2. 해당 Sector 내부의 NPC WakeUp
	for (int sx = xRange.first; sx <= xRange.second; ++sx) {
		for (int sy = yRange.first; sy <= yRange.second; ++sy) {
			// 3. 해당 Sector의 Object List Get
			std::unordered_set<int> sectorObjects;
			_sectors[sx][sy].CollectObject(sectorObjects);

			for (int id : sectorObjects) {
				auto object = service->FindObject(id);
				if ((nullptr != object) and (object->GetType() == ObjectType::MONSTER)) {
					if (auto monster = static_pointer_cast<Monster>(object)) {
						monster->WakeUp(true, session->GetId());
					}

					else {
						LOG_ERR("OnPlayerLogin FindObject failed");
					}
				}
			}
		}
	}

	ViewListDiff viewListDiff = SyncViewList(session);

	for (int id : viewListDiff.addViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		auto symbol = service->GetQuestSymbol(session, object->GetId());
		session->Send(PacketFactory::BuildAddPacket(*object, symbol));

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildAddPacket(*session));
		}
	}
}

void ViewManager::HandlePlayerMoveNotify(const std::shared_ptr<GameSession>& session)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	// 1. Move한 Player의 시야에 걸리는 Sector Range
	auto [xRange, yRange] = Sector::GetSectorRange(session->GetX(), session->GetY());

	// 2. 해당 Sector 내부의 NPC WakeUp
	for (int sx = xRange.first; sx <= xRange.second; ++sx) {
		for (int sy = yRange.first; sy <= yRange.second; ++sy) {
			// 3. 해당 Sector의 Object List Get
			std::unordered_set<int> sectorObjects;
			_sectors[sx][sy].CollectObject(sectorObjects);

			for (int id : sectorObjects) {
				auto object = service->FindObject(id);
				if (object->GetType() == ObjectType::MONSTER) {
					if (auto npc = static_pointer_cast<Monster>(object)) {
						npc->WakeUp(false, session->GetId());
					}

					else {
						LOG_ERR("OnPlayerLogin FindObject failed");
					}
				}
			}
		}
	}

	Multicast(session, service);
}

void ViewManager::HandlePlayerDeathNotify(const std::shared_ptr<GameSession>& session)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	auto sector = Sector::GetSector(session->GetX(), session->GetY());

	if (_sectors[sector.first][sector.second].Contains(session->GetId())) {
		LeaveSector(session);
	};

	std::unordered_set<int> candidates = CollectViewList(session);

	for (int id : candidates) {
		auto object = service->FindObject(id);

		if ((nullptr != object) and (object->GetType() == ObjectType::PLAYER)) {
			auto player = static_pointer_cast<GameSession>(object);
			player->Send(PacketFactory::BuildRemovePacket(*session));
			player->RemoveViewList(session->GetId());
		}
	}

	session->Send(PacketFactory::BuildRemovePacket(*session));

	auto party = session->GetParty();
	if (nullptr != party) {
		party->Broadcast(PacketFactory::BuildPartyUpdatePacket(*party));
	}
}

void ViewManager::HandlePlayerReviveNotify(const std::shared_ptr<GameSession>& session)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	if (nullptr == session) {
		return;
	}

	session->ClearViewList();

	// 1. Sector 갱신
	EnterSector(session);

	std::unordered_set<int> candidates = CollectViewList(session);

	for (int id : candidates) {
		auto object = service->FindObject(id);

		if ((nullptr != object) and (object->GetType() == ObjectType::PLAYER)) {
			auto player = static_pointer_cast<GameSession>(object);
			player->Send(PacketFactory::BuildAddPacket(*session));
			player->AddViewList(session->GetId());
		}

		char symbol = service->GetQuestSymbol(session, object->GetId());
		session->Send(PacketFactory::BuildAddPacket(*object, symbol));
		session->AddViewList(object->GetId());
	}

	session->Send(PacketFactory::BuildAddPacket(*session));
	session->Send(PacketFactory::BuildStatChangePacket(*session));

	// 1. Login한 Player의 시야에 걸리는 Sector Range
	auto [xRange, yRange] = Sector::GetSectorRange(session->GetX(), session->GetY());

	// 2. 해당 Sector 내부의 NPC WakeUp
	for (int sx = xRange.first; sx <= xRange.second; ++sx) {
		for (int sy = yRange.first; sy <= yRange.second; ++sy) {
			// 3. 해당 Sector의 Object List Get
			std::unordered_set<int> sectorObjects;
			_sectors[sx][sy].CollectObject(sectorObjects);

			for (int id : sectorObjects) {
				auto object = service->FindObject(id);
				if (object->GetType() == ObjectType::MONSTER) {
					if (auto npc = static_pointer_cast<Monster>(object)) {
						npc->WakeUp(true, session->GetId());
					}

					else {
						LOG_ERR("OnPlayerRevive FindObject failed");
					}
				}
			}
		}
	}

	auto party = session->GetParty();
	if (nullptr != party) {
		party->Broadcast(PacketFactory::BuildPartyUpdatePacket(*party));
	}
}

void ViewManager::HandleNpcMove(const std::shared_ptr<Monster>& npc, int oldX, int oldY)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	// 1. 이동 전후 viewList Get
	auto oldViewList = CollectViewList(npc, oldX, oldY);
	auto newViewList = CollectViewList(npc);

	// 2. Sector 갱신
	auto oldSector = Sector::GetSector(oldX, oldY);
	auto newSector = Sector::GetSector(npc->GetX(), npc->GetY());

	if (oldSector != newSector) {
		if (_sectors[oldSector.first][oldSector.second].Contains(npc->GetId())) {
			LeaveSector(npc, oldSector.first, oldSector.second);
		}
		EnterSector(npc);
	}
	
	// 3. Packet Send
	Multicast(oldViewList, newViewList, npc, service);
}

void ViewManager::HandleNpcDeath(const std::shared_ptr<Monster>& npc)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}
	
	auto sector = Sector::GetSector(npc->GetX(), npc->GetY());

	if (_sectors[sector.first][sector.second].Contains(npc->GetId())) {
		LeaveSector(npc);
	};

	std::unordered_set<int> candidates = CollectViewList(npc);

	for (int id : candidates) {
		auto object = service->FindObject(id);

		if (object->GetType() == ObjectType::PLAYER) {
			auto player = static_pointer_cast<GameSession>(object);
			player->Send(PacketFactory::BuildRemovePacket(*npc));
			player->RemoveViewList(npc->GetId());
		}
	}
}

void ViewManager::HandleNpcRevive(const std::shared_ptr<Monster>& npc)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	// 1. Sector 갱신
	EnterSector(npc);

	std::unordered_set<int> candidates = CollectViewList(npc);

	for (int id : candidates) {
		auto object = service->FindObject(id);

		if (object->GetType() == ObjectType::PLAYER) {
			auto player = static_pointer_cast<GameSession>(object);
			player->Send(PacketFactory::BuildAddPacket(*npc));
			player->AddViewList(npc->GetId());
		}
	}
}

void ViewManager::EnterSector(const std::shared_ptr<GameObject>& object)
{
	auto [sx, sy] = Sector::GetSector(object->GetX(), object->GetY());
	_sectors[sx][sy].AddObject(object->GetId());
}

void ViewManager::LeaveSector(const std::shared_ptr<GameObject>& object)
{
	auto [sx, sy] = Sector::GetSector(object->GetX(), object->GetY());
	_sectors[sx][sy].RemoveObject(object->GetId());
}

void ViewManager::EnterSector(const std::shared_ptr<GameObject>& object, int sx, int sy)
{
	_sectors[sx][sy].AddObject(object->GetId());
}

void ViewManager::LeaveSector(const std::shared_ptr<GameObject>& object, int sx, int sy)
{
	_sectors[sx][sy].RemoveObject(object->GetId());
}

std::unordered_set<int> ViewManager::CollectVisibleObjects(const std::shared_ptr<GameObject>& object) const
{
	auto [xRange, yRange] = Sector::GetSectorRange(object->GetX(), object->GetY());
	std::unordered_set<int> result;

	for (int sx = xRange.first; sx <= xRange.second; ++sx) {
		for (int sy = yRange.first; sy <= yRange.second; ++sy) {
			_sectors[sx][sy].CollectObject(result);
		}
	}

	return result;
}

std::unordered_set<int> ViewManager::CollectViewList(const std::shared_ptr<GameObject>& object) const
{
	auto service = _service.lock();
	if (nullptr == service) {
		return {};
	}

	// 1. self의 View Range가 걸치는 Sector에 있는 모든 client의 id 복사
	std::unordered_set<int> targetViewList = CollectVisibleObjects(object);
	std::unordered_set<int> result;

	// 2. Sector 순회하면서 실제 self의 View Range안에 있는 client의 id Search
	for (int id : targetViewList) {
		if (id == object->GetId()) continue;

		GameObjectPtr target = service->FindObject(id);
		if (nullptr == target) continue;
		if (not target->IsVisible()) continue;
		if (not target->IsAlive()) continue;
		if (CanSee(object, target)) {
			result.insert(id);
		}
	}

	return result;
}

std::unordered_set<int> ViewManager::CollectVisibleObjects(int x, int y) const
{
	auto [xRange, yRange] = Sector::GetSectorRange(x, y);
	std::unordered_set<int> result;

	for (int sx = xRange.first; sx <= xRange.second; ++sx) {
		for (int sy = yRange.first; sy <= yRange.second; ++sy) {
			_sectors[sx][sy].CollectObject(result);
		}
	}

	return result;
}

std::unordered_set<int> ViewManager::CollectViewList(const std::shared_ptr<GameObject>& object, int x, int y) const
{
	auto service = _service.lock();
	if (nullptr == service) {
		return {};
	}

	// 1. self의 View Range가 걸치는 Sector에 있는 모든 client의 id 복사
	std::unordered_set<int> targetViewList = CollectVisibleObjects(x, y);
	std::unordered_set<int> result;

	// 2. Sector 순회하면서 실제 self의 View Range안에 있는 client의 id Search
	for (int id : targetViewList) {
		if (id == object->GetId()) continue;

		GameObjectPtr target = service->FindObject(id);
		if (nullptr == target) continue;
		if (not target->IsVisible()) continue;
		if (not target->IsAlive()) continue;
		if (CanSee(object, target)) {
			result.insert(id);
		}
	}

	return result;
}

bool ViewManager::CanSee(const std::shared_ptr<GameObject>& self, const std::shared_ptr<GameObject>& target) const
{
	if (abs(self->GetX() - target->GetX()) > VIEW_RANGE) return false;
	return abs(self->GetY() - target->GetY()) <= VIEW_RANGE;
}

void ViewManager::Multicast(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service)
{
	ViewListDiff viewListDiff = SyncViewList(session);

	session->Send(PacketFactory::BuildMovePacket(*session));

	for (int id : viewListDiff.addViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		char symbol = service->GetQuestSymbol(session, object->GetId());
		session->Send(PacketFactory::BuildAddPacket(*object, symbol));

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildAddPacket(*session));
		}
	}

	for (int id : viewListDiff.moveViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		session->Send(PacketFactory::BuildMovePacket(*object));

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildMovePacket(*session));
		}
	}

	for (int id : viewListDiff.removeViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		session->Send(PacketFactory::BuildRemovePacket(*object));

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildRemovePacket(*session));
		}
	}
}

void ViewManager::Multicast(const std::unordered_set<int>& oldViewList, const std::unordered_set<int>& newViewList, const std::shared_ptr<GameObject>& npc, const std::shared_ptr<Service>& service)
{
	ViewListDiff viewListDiff = SyncViewList(oldViewList, newViewList);

	for (int id : viewListDiff.addViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;
		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			char symbol = service->GetQuestSymbol(target, npc->GetId());
			target->Send(PacketFactory::BuildAddPacket(*npc, symbol));
		}
	}

	for (int id : viewListDiff.moveViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildMovePacket(*npc));
		}
	}

	for (int id : viewListDiff.removeViewList) {
		auto object = service->FindObject(id);
		if (nullptr == object) continue;

		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			target->Send(PacketFactory::BuildRemovePacket(*npc));
		}
	}
}
