#pragma once

constexpr int MAP_SIZE = 2000;
constexpr int SECTOR_SIZE = 20;
constexpr int SECTOR_COUNT = MAP_SIZE / SECTOR_SIZE;

class Sector
{
public:
	void AddObject(int id);
	void RemoveObject(int id);
	void CollectObject(std::unordered_set<int>& out) const;

	bool Contains(int id) const;

public:
	static std::pair<int, int> GetSector(int x, int y);
	static std::pair<std::pair<int, int>, std::pair<int, int>> GetSectorRange(int x, int y);

private:
	std::unordered_set<int> _objects;
	mutable std::shared_mutex _mutex;
};

