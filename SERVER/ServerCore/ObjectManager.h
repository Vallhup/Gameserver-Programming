#pragma once

class ObjectManager
{
public:
	ObjectManager() : _nextPlayerId(0), _nextNpcId(5000) {}

	int AddObject(const std::shared_ptr<GameObject>& object);
	int RemoveObject(const std::shared_ptr<GameObject>& object);
	std::shared_ptr<GameObject> FindObject(int id, bool player = false) const;

	void ForEachObject(const std::function<void(int, const std::shared_ptr<GameObject>&)>& f) const;
	void ForEachPlayer(const std::function<void(const std::shared_ptr<GameSession>&)>& f) const;

	const std::vector<int> GetPlayerInTile(short x, short y, const std::shared_ptr<Service>& service) const;
	const std::vector<int> GetMonsterInTile(short x, short y, const std::shared_ptr<Service>& service) const;

private:
	int AllocateId(const std::shared_ptr<GameObject>& object);

	mutable std::shared_mutex _mutex;

	std::atomic<int> _nextPlayerId;
	concurrency::concurrent_priority_queue<int> _freePlayerIds;

	std::atomic<int> _nextNpcId;
	concurrency::concurrent_priority_queue<int> _freeNpcIds;

	concurrency::concurrent_unordered_map<int, std::atomic<std::shared_ptr<GameObject>>> _objects;
};