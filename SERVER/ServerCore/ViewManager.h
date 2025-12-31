#pragma once

struct ViewListDiff {
	std::vector<int> addViewList;
	std::vector<int> moveViewList;
	std::vector<int> removeViewList;
};

class Sector;
class Service;
class GameSession;
class Monster;

class ViewManager
{
public:
	ViewManager(const std::shared_ptr<Service>& service);
	~ViewManager() = default;

	ViewListDiff SyncViewList(const std::shared_ptr<GameSession>& session) const;
	ViewListDiff SyncViewList(const std::unordered_set<int>& oldViewList, const std::unordered_set<int>& newViewList);

	void HandlePlayerLoginNotify(const std::shared_ptr<GameSession>& session);
	void HandlePlayerMoveNotify(const std::shared_ptr<GameSession>& session);
	void HandlePlayerDeathNotify(const std::shared_ptr<GameSession>& session);
	void HandlePlayerReviveNotify(const std::shared_ptr<GameSession>& session);

	void HandleNpcMove(const std::shared_ptr<Monster>& npc, int oldX, int oldY);
	void HandleNpcDeath(const std::shared_ptr<Monster>& npc);
	void HandleNpcRevive(const std::shared_ptr<Monster>& npc);

	void EnterSector(const std::shared_ptr<GameObject>& object);
	void LeaveSector(const std::shared_ptr<GameObject>& object);
	void EnterSector(const std::shared_ptr<GameObject>& object, int sx, int sy);
	void LeaveSector(const std::shared_ptr<GameObject>& object, int sx, int sy);

	std::unordered_set<int> CollectVisibleObjects(const std::shared_ptr<GameObject>& object) const;
	std::unordered_set<int> CollectViewList(const std::shared_ptr<GameObject>& object) const;
	std::unordered_set<int> CollectVisibleObjects(int x, int y) const;
	std::unordered_set<int> CollectViewList(const std::shared_ptr<GameObject>& object, int x, int y) const;

private:
	std::array<std::array<Sector, SECTOR_COUNT>, SECTOR_COUNT> _sectors;
	std::weak_ptr<Service> _service;

	bool CanSee(const std::shared_ptr<GameObject>& self, const std::shared_ptr<GameObject>& target) const;
	void Multicast(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service);
	void Multicast(const std::unordered_set<int>& oldViewList, const std::unordered_set<int>& newViewList, const std::shared_ptr<GameObject>& npc, const std::shared_ptr<Service>& service);

};