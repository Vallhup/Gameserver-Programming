#pragma once

class Service;
class Monster;
class GameSession;

class CombatManager
{
public:
	CombatManager(const std::shared_ptr<Service>& service) : _service(service) {}

	void HandlePlayerMove(const std::shared_ptr<GameSession>& player);
	void HandleMonsterMove(const std::shared_ptr<Monster>& monster);
	void HandlePlayerAttack(const std::shared_ptr<GameSession>& player);

private:
	std::weak_ptr<Service> _service;

	bool CanAttack(int attackerId, float cooldownSec);
	void RegisterAttackTime(int attackerId);

	void HandleDamage_PlayerMoved(const std::shared_ptr<GameSession>& player);
	void HandleDamage_PlayerAttacked(const std::shared_ptr<GameSession>& player);
	void HandleDamage_MonsterMoved(const std::shared_ptr<Monster>& monster);

	void DamageToPlayer(const std::shared_ptr<GameSession>& player, int damage, int attackerId);
	void DamageToMonster(const std::shared_ptr<Monster>& monster, int damage, int attackerId);

	void HandleMonsterDeath(const std::shared_ptr<Monster>& monster, const std::shared_ptr<GameSession>& killer);
	void HandlePlayerDeath(const std::shared_ptr<GameSession>& player);
};

