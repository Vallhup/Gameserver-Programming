#include "pch.h"
#include "ItemManager.h"

bool ItemManager::GetUserItem(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service)
{
	if (session->GetState() != ST_INGAME) {
		return false;
	}

	auto& inventory = session->GetInventory();
	auto userItems = service->GetUserItems(session);

	for (auto& [itemId, count] : userItems) {
		inventory->AddItem(itemId, count);
		session->Send(PacketFactory::BuildAddItemPacket(itemId, count));
	}

	return true;
}

bool ItemManager::UseItem(const std::shared_ptr<GameSession>& session, int itemId, const std::shared_ptr<Service>& service)
{
	// 1. Session State Check
	if ((session->GetState() != ST_INGAME) or (not session->IsAlive())) {
		return false;
	}

	// 2. 인벤토리에 아이템 있는지 확인
	if (not session->GetInventory()->HasItem(itemId)) {
		return false;
	}

	// 3. 아이템별 분기 처리
	switch (itemId) {
	case HpPotion:
		return HandleHpPotion(session, service);

	default:
		LOG_ERR("Item[%d] is not Item", itemId);
		return false;
	}
}

bool ItemManager::HandleHpPotion(const std::shared_ptr<GameSession>& session, const std::shared_ptr<Service>& service)
{
	// 1. Hp 회복
	short hp = session->GetHp();
	session->IncreaseHp(5);

	if (session->GetHp() == hp) {
		return false;
	}

	service->UserUseItem(session, HpPotion, 1);

	// 2. 인벤토리에서 제거
	session->GetInventory()->RemoveItem(HpPotion, 1);

	// 3. 성공 Packet Send
	session->Send(PacketFactory::BuildUseItemOkPacket(HpPotion));
	session->Send(PacketFactory::BuildStatChangePacket(*session));

	return true;
}