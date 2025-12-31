#pragma once

class GameSession;

class ItemManager
{
public:
	bool GetUserItem(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service);
	bool UseItem(const std::shared_ptr<GameSession>& session, int itemId, const std::shared_ptr<Service>& service);

private:
	bool HandleHpPotion(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service);
};

