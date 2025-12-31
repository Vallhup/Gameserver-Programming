#include "pch.h"
#include "Inventory.h"

void Inventory::AddItem(char itemId, int count)
{
	if (count <= 0) {
		return;
	}

	std::unique_lock lock{ _mutex };
	_items[itemId] += count;
}

bool Inventory::RemoveItem(char itemId, int count)
{
	if (count <= 0) {
		return false;
	}

	std::unique_lock lock{ _mutex };

	if (not _items.contains(itemId)) {
		return false;
	}

	else {
		if (_items[itemId] <= count) {
			_items.erase(itemId);
		}

		else {
			_items[itemId] -= count;
		}
	}

	return true;
}

int Inventory::GetItemCount(char itemId) const
{
	std::shared_lock lock{ _mutex };

	if (_items.contains(itemId)) {
		return _items.at(itemId);
	}

	return 0;
}

bool Inventory::HasItem(char itemId) const
{
	std::shared_lock lock{ _mutex };
	return _items.contains(itemId);
}
