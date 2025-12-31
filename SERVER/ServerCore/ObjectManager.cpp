#include "pch.h"
#include "ObjectManager.h"

int ObjectManager::AddObject(const std::shared_ptr<GameObject>& object)
{
	int id = AllocateId(object);
	if (id < 0) {
		LOG_WRN("Too Many IDs");
		return -1;
	}

	object->SetId(id);
	{
		std::unique_lock lock{ _mutex };
		_objects.insert(std::make_pair(id, object));
	}

	return id;
}

int ObjectManager::RemoveObject(const std::shared_ptr<GameObject>& object)
{
	std::unique_lock lock{ _mutex };
	_objects.unsafe_erase(object->GetId());
	_freePlayerIds.push(object->GetId());

	return object->GetId();
}

std::shared_ptr<GameObject> ObjectManager::FindObject(int id, bool player) const
{
	if (player) {
		for (const auto& [key, obj] : _objects) {
			auto object = obj.load();
			if ((nullptr != object) and (object->GetType() == ObjectType::PLAYER)) {
				auto player = static_pointer_cast<GameSession>(object);
				if ((nullptr != player) and (player->GetUserID() == id)) {
					return object;
				}
			}
		}

		return nullptr;
	}

	auto it = _objects.find(id);
	if (it == _objects.end()) {
		return nullptr;
	}

	auto object = it->second.load();
	return object;
}

void ObjectManager::ForEachObject(const std::function<void(int, const std::shared_ptr<GameObject>&)>& f) const
{
	for (auto& [id, object] : _objects) {
		int copyId = id;
		if (auto copyObject = object.load()) {
			f(copyId, copyObject);
		}
	}
}

void ObjectManager::ForEachPlayer(const std::function<void(const std::shared_ptr<GameSession>&)>& f) const
{
	ForEachObject(
		[&](int, const std::shared_ptr<GameObject>& object)
		{
			if (object->GetType() == ObjectType::PLAYER) {
				f(static_pointer_cast<GameSession>(object));
			}
		}
	);
}

const std::vector<int> ObjectManager::GetPlayerInTile(short x, short y, const std::shared_ptr<Service>& service) const
{
	std::vector<int> result;
	result.reserve(1);

	std::shared_ptr<Monster> temp = std::make_shared<Monster>(-1, x, y, "-1");

	auto viewList = service->CollectViewList(temp);

	for (int id : viewList) {
		auto object = service->FindObject(id);
		if ((nullptr == object) or (not object->IsAlive())) {
			continue;
		}

		if ((object->GetType() == ObjectType::PLAYER) and (object->GetX() == x) and (object->GetY() == y)) {
			result.push_back(object->GetId());
		}
	}

	return result;
}

const std::vector<int> ObjectManager::GetMonsterInTile(short x, short y, const std::shared_ptr<Service>& service) const
{
	std::vector<int> result;
	result.reserve(1);

	std::shared_ptr<Monster> temp = std::make_shared<Monster>(-1, x, y, "-1");

	auto viewList = service->CollectViewList(temp);

	for (int id : viewList) {
		auto object = service->FindObject(id);
		if ((nullptr == object) or (not object->IsAlive())) {
			continue;
		}

		if ((object->GetType() == ObjectType::MONSTER) and (object->GetX() == x) and (object->GetY() == y)) {
			result.push_back(object->GetId());
		}
	}

	return result;
}

int ObjectManager::AllocateId(const std::shared_ptr<GameObject>& object)
{
	int id;

	switch (object->GetType()) {
	case ObjectType::PLAYER: {
		if (not _freePlayerIds.empty()) {
			if (_freePlayerIds.try_pop(id)) {
				return id;
			}
		}

		if (_nextPlayerId < MAX_USER) {
			return _nextPlayerId++;
		}

		break;
	}
	case ObjectType::MONSTER: {
		if (not _freeNpcIds.empty()) {
			if (_freeNpcIds.try_pop(id)) {
				return id;
			}
		}

		if (_nextNpcId < MAX_USER + MAX_NPC) {
			return _nextNpcId++;
		}

		break;
	}
	case ObjectType::NPC: {
		return MAX_USER + MAX_NPC;
	}
	}

	return -1;
}