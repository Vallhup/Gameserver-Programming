#include "pch.h"
#include "MonsterBehavior.h"

void PeaceFixedBehavior::OnMove(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (not owner->IsAlive()) {
		owner->SetMovePending(false);
		return;
	}

	owner->SetState(NpcState::ST_Random);
	owner->SetActive(false);
	owner->SetMovePending(false);
}

void PeaceFixedBehavior::OnHeal(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (owner->IsAlive()) {
		owner->SetHealPending(false);
		return;
	}

	owner->SetAlive(true);
	owner->SetPos(owner->GetDefaultX(), owner->GetDefaultY());
	owner->SetState(NpcState::ST_Random);
	owner->SetHp(owner->GetMaxHp());

	service->OnNpcRevive(owner);

	owner->SetHealPending(false);
	owner->RegisterTimer();
}

void PeaceRoamingBehavior::OnMove(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (not owner->IsAlive()) {
		owner->SetMovePending(false);
		return;
	}

	owner->SetState(NpcState::ST_Random);
	owner->RandomMove(service);
	owner->RegisterTimer();
}

void PeaceRoamingBehavior::OnHeal(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (owner->IsAlive()) {
		owner->SetHealPending(false);
		return;
	}

	owner->SetAlive(true);
	owner->SetPos(owner->GetDefaultX(), owner->GetDefaultY());
	owner->SetState(NpcState::ST_Random);
	owner->SetHp(owner->GetMaxHp());

	service->OnNpcRevive(owner);

	owner->RegisterTimer();
	owner->SetHealPending(false);
}

void AgroFixedBehavior::OnMove(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (not owner->IsAlive()) {
		owner->SetMovePending(false);
		return;
	}

	APos npcPos{ owner->GetX(), owner->GetY() };

	// 1. 주변 Player 탐색
	bool agro{ false };
	auto visibleList = service->CollectViewList(owner);

	APos targetPos;

	// 2. 상태 결정
	for (int id : visibleList) {
		auto object = service->FindObject(id);
		if (object->GetType() == ObjectType::PLAYER) {
			auto player = static_pointer_cast<GameSession>(object);
			if ((nullptr != player) and (player->IsAlive())) {
				int dx = std::abs(player->GetX() - npcPos.x);
				int dy = std::abs(player->GetY() - npcPos.y);

				// Player가 11 x 11 안에 들어오면 Agro 상태
				if ((dx <= 5) and (dy <= 5)) {
					int dist = dx + dy;
					targetPos = { player->GetX(), player->GetY() };
					agro = true;
					break;
				}
			}
		}
	}

	std::unordered_set<int> newViewList;

	// 3-1. Agro 상태면 AStar로 Player 쫓아가기
	if (agro) {
		owner->SetActive(true);
		owner->SetState(NpcState::ST_Agro);
		newViewList = owner->AStarMove(service, npcPos, targetPos);
	}

	// 3-2. Agro 상태 아니면 Random Move
	else {
		owner->SetMovePending(false);
		owner->SetActive(false);
		return;
	}

	bool isActive{ false };
	for (int id : newViewList) {
		if (service->FindObject(id)->GetType() == ObjectType::PLAYER) {
			isActive = true;
			break;
		}
	}

	// 4. 움직인 후에도 Player가 근처에 있으면 다음 Event Push, 없으면 비활성화
	if (isActive) {
		owner->RegisterTimer();
	}

	else {
		owner->SetActive(false);
	}
}

void AgroFixedBehavior::OnHeal(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (owner->IsAlive()) {
		owner->SetHealPending(false);
		return;
	}

	owner->SetAlive(true);
	owner->SetPos(owner->GetDefaultX(), owner->GetDefaultY());
	owner->SetState(NpcState::ST_Random);
	owner->SetHp(owner->GetMaxHp());

	service->OnNpcRevive(owner);

	owner->SetHealPending(false);

	owner->RegisterTimer();
}

void AgroRoamingBehavior::OnMove(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (not owner->IsAlive()) {
		owner->SetMovePending(false);
		return;
	}

	APos npcPos{ owner->GetX(), owner->GetY() };
	APos defaultPos{ owner->GetDefaultX(), owner->GetDefaultY() };

	// 1. 주변 Player 탐색
	bool agro{ false };
	auto visibleList = service->CollectViewList(owner);

	APos targetPos;

	// 2. 상태 결정
	for (int id : visibleList) {
		auto object = service->FindObject(id);
		if (object->GetType() == ObjectType::PLAYER) {
			auto player = static_pointer_cast<GameSession>(object);
			if ((nullptr != player) and (player->IsAlive())) {
				int dx = std::abs(player->GetX() - npcPos.x);
				int dy = std::abs(player->GetY() - npcPos.y);

				// Player가 11 x 11 안에 들어오면 Agro 상태
				if ((dx <= 5) and (dy <= 5)) {
					int dist = dx + dy;
					targetPos = { player->GetX(), player->GetY() };
					agro = true;
					break;
				}
			}
		}
	}

	std::unordered_set<int> newViewList;

	// 3-1. Agro 상태면 AStar로 Player 쫓아가기
	if (agro) {
		owner->SetState(NpcState::ST_Agro);
		newViewList = owner->AStarMove(service, npcPos, targetPos);
	}

	// 3-2. Agro 상태 아니면 Random Move
	else {
		owner->SetState(NpcState::ST_Random);
		newViewList = owner->RandomMove(service);
	}

	bool isActive{ false };
	for (int id : newViewList) {
		if (service->FindObject(id)->GetType() == ObjectType::PLAYER) {
			isActive = true;
			break;
		}
	}

	// 4. 움직인 후에도 Player가 근처에 있으면 다음 Event Push, 없으면 비활성화
	if (isActive) {
		owner->RegisterTimer();
	}

	else {
		owner->SetActive(false);
	}
}

void AgroRoamingBehavior::OnHeal(const std::shared_ptr<Monster>& owner, const std::shared_ptr<Service>& service)
{
	if (owner->IsAlive()) {
		owner->SetHealPending(false);
		return;
	}

	owner->SetAlive(true);
	owner->SetPos(owner->GetDefaultX(), owner->GetDefaultY());
	owner->SetState(NpcState::ST_Random);
	owner->SetHp(owner->GetMaxHp());

	service->OnNpcRevive(owner);

	owner->RegisterTimer();
	owner->SetHealPending(false);
}
