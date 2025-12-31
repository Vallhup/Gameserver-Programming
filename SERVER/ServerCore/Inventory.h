#pragma once

class Inventory
{
public:
	Inventory(const std::shared_ptr<GameSession>& owner) : _owner(owner) {}

public:
	void AddItem(char itemId, int count);
	bool RemoveItem(char itemId, int count);
	
public:
	int GetItemCount(char itemId) const;
	bool HasItem(char itemId) const;
	
private:
	std::unordered_map<int, int> _items;
	mutable std::shared_mutex _mutex;

	std::weak_ptr<GameSession> _owner;
};