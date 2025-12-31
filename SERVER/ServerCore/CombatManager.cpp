#include "pch.h"
#include "CombatManager.h"

void CombatManager::HandlePlayerMove(const std::shared_ptr<GameSession>& player)
{
	HandleDamage_PlayerMoved(player);
}

void CombatManager::HandleMonsterMove(const std::shared_ptr<Monster>& monster)
{
	HandleDamage_MonsterMoved(monster);
}

void CombatManager::HandlePlayerAttack(const std::shared_ptr<GameSession>& player)
{
	HandleDamage_PlayerAttacked(player);
}

bool CombatManager::CanAttack(int attackerId, float cooldownSec)
{
	using namespace std::chrono;

	auto service = _service.lock();
	if (nullptr == service) {
		return false;
	}

	auto attacker = service->FindObject(attackerId);
	auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	auto lastAttackTime = attacker->_lastAttackTime;

	// 한 번도 공격하지 않았으면
	if (lastAttackTime == 0) {
		return true;
	}

	auto elapsed = now - lastAttackTime;
	return elapsed >= static_cast<long long>(cooldownSec * 1000.0f);
}

void CombatManager::RegisterAttackTime(int attackerId)
{
	using namespace std::chrono;

	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	auto attacker = service->FindObject(attackerId);
	attacker->_lastAttackTime = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void CombatManager::HandleDamage_PlayerMoved(const std::shared_ptr<GameSession>& player)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	short px = player->GetX();
	short py = player->GetY();

	auto monsters = service->GetMonstersInTile(px, py);

	for (auto& monsterId : monsters) {
		auto monster = service->FindObject(monsterId);

		if (not monster->IsAlive()) {
			continue;
		}

		float monsterCooltime = 1.0f;
		if (not CanAttack(monster->GetId(), monsterCooltime)) {
			continue;
		}

		int damage = monster->GetDamage();

		DamageToPlayer(player, damage, monster->GetId());
		RegisterAttackTime(monster->GetId());
	}
}

void CombatManager::HandleDamage_PlayerAttacked(const std::shared_ptr<GameSession>& player)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	if (not player->IsAlive()) {
		return;
	}

	const float playerCooldown = 1.0f;
	if (not CanAttack(player->GetId(), playerCooldown)) {
		return;
	}

	std::vector<std::shared_ptr<Monster>> targets;

	const std::array<std::pair<int, int>, 4> directions{ {
		{ 0, -1 },
		{ 0,  1 },
		{ -1, 0 },
		{ 1,  0 }
	} };

	auto visible = service->CollectViewList(player);
	for (const auto& [dx, dy] : directions) {
		short nx = player->GetX() + dx;
		short ny = player->GetY() + dy;

		for (int id : visible) {
			auto object = service->FindObject(id);
			if (object->GetType() == ObjectType::MONSTER) {
				if (nullptr == object) continue;
				if ((nx == object->GetX()) and (ny == object->GetY()) and (object->IsAlive())) {
					auto snapShot = player->GetViewList();
					if (snapShot->contains(object->GetId())) {
						targets.push_back(static_pointer_cast<Monster>(object));
					}
				}
			}
		}
	}

	for (auto& target : targets) {
		if (not target->IsAlive()) {
			continue;
		}

		int damage = player->GetDamage();

		DamageToMonster(target, damage, player->GetId());
		RegisterAttackTime(player->GetId());
	}
}

void CombatManager::HandleDamage_MonsterMoved(const std::shared_ptr<Monster>& monster)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	short px = monster->GetX();
	short py = monster->GetY();

	auto players = service->GetPlayersInTile(px, py);

	for (auto& playerId : players) {
		auto player = service->FindObject(playerId);

		if (not player->IsAlive()) {
			continue;
		}

		float monsterCooltime = 1.0f;
		if (not CanAttack(monster->GetId(), monsterCooltime)) {
			continue;
		}

		int damage = monster->GetDamage();

		DamageToPlayer(static_pointer_cast<GameSession>(player), damage, monster->GetId());
		RegisterAttackTime(monster->GetId());
	}
}

void CombatManager::DamageToPlayer(const std::shared_ptr<GameSession>& player, int damage, int attackerId)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	player->TakeDamage(damage);

	if (not player->IsAlive()) {
		HandlePlayerDeath(player);
	}
}

void CombatManager::DamageToMonster(const std::shared_ptr<Monster>& monster, int damage, int attackerId)
{
	monster->TakeDamage(damage);

	if (not monster->IsAlive()) {
		if (auto service = _service.lock()) {
			auto object = service->FindObject(attackerId);
			if ((nullptr != object) and (object->GetType() == ObjectType::PLAYER)) {
				auto killer = static_pointer_cast<GameSession>(object);
				HandleMonsterDeath(monster, killer);
			}
		}
	}
}

void CombatManager::HandleMonsterDeath(const std::shared_ptr<Monster>& monster, const std::shared_ptr<GameSession>& killer)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	if (nullptr != killer) {
		// View, Quest 처리
		service->OnNpcDeath(monster, killer);

		// Temp : 30% 확률로 HpPotion Get
		if (rand() % 100 < 30) {
			service->UserGetItem(killer, HpPotion, 1);
			killer->GetInventory()->AddItem(HpPotion, 1);
			killer->Send(PacketFactory::BuildAddItemPacket(HpPotion, 1));
		}

		// Exp 배분
		int exp = monster->GetExp();

		if (auto party = killer->GetParty()) {
			party->ShareExp(exp);
		}

		else {
			killer->AddExp(exp);
		}

		killer->Send(PacketFactory::BuildStatChangePacket(*killer));
	}
}

void CombatManager::HandlePlayerDeath(const std::shared_ptr<GameSession>& player)
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	service->OnPlayerDeath(player);
}
