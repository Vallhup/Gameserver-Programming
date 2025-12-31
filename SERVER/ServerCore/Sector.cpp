#include "pch.h"
#include "Sector.h"

void Sector::AddObject(int id)
{
	std::unique_lock lock{ _mutex };
	_objects.insert(id);
}

void Sector::RemoveObject(int id)
{
	std::unique_lock lock{ _mutex };
	_objects.erase(id);
}

void Sector::CollectObject(std::unordered_set<int>& out) const
{
	std::shared_lock lock{ _mutex };
	out.insert(_objects.begin(), _objects.end());
}

bool Sector::Contains(int id) const
{
	std::shared_lock lock{ _mutex };
	return _objects.contains(id);
}

std::pair<int, int> Sector::GetSector(int x, int y)
{
	int sectorX = std::clamp<int>(x / SECTOR_SIZE, 0, SECTOR_COUNT - 1);
	int sectorY = std::clamp<int>(y / SECTOR_SIZE, 0, SECTOR_COUNT - 1);

	return { sectorX, sectorY };
}

std::pair<std::pair<int, int>, std::pair<int, int>> Sector::GetSectorRange(int x, int y)
{
	auto compute = [](int coord) -> std::pair<int, int>
		{
			int min = std::clamp((coord - VIEW_RANGE) / SECTOR_SIZE, 0, SECTOR_COUNT - 1);
			int max = std::clamp((coord + VIEW_RANGE) / SECTOR_SIZE, 0, SECTOR_COUNT - 1);

			return { min, max };
		};

	std::pair<int, int> xRange = compute(x);
	std::pair<int, int> yRange = compute(y);

	return { xRange, yRange };
}