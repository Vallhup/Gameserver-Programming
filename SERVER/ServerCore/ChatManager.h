#pragma once

class Service;

class ChatManager
{
public:
	void HandleMessage(const std::shared_ptr<Service>& service, int senderId, const char* msg, int targetId = -1);
};

