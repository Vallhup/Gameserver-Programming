#include "pch.h"
#include "ChatManager.h"

void ChatManager::HandleMessage(const std::shared_ptr<Service>& service, int senderId, const char* msg, int targetId)
{
	if (nullptr == service) {
		LOG_ERR("HandleMessage failed: service is nullptr");
		return;
	}

	std::string text(msg);

	// 1. 보내려는 message의 길이 검사 / 제한 (CHAT_SIZE)
	if (text.size() >= sizeof(CHAT_SIZE)) {
		text.resize(CHAT_SIZE - 1);
	}

	// 2. Packet Broadcast
	SC_CHAT_PACKET chat;
	chat.id = senderId;
	chat.size = sizeof(chat);
	chat.type = SC_CHAT;
	chat.targetId = targetId;
	strcpy_s(chat.message, CHAT_SIZE - 1, text.c_str());
	chat.message[ CHAT_SIZE - 1 ] = '\0';

	service->Broadcast(PacketFactory::Serialize(chat));
}