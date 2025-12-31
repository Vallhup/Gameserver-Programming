#pragma once

enum OperationType : char
{
	Accept,
	Recv,
	Send,
	Heal,
	Respawn,
	NpcMove,
	NpcHeal,
	NpcAttack
};

class GameSession;
class RecvBuffer;
class IocpObject;

class ExpOver : public OVERLAPPED
{
public:
	ExpOver(OperationType operationType);

	void Init(const std::shared_ptr<IocpObject>& owner = nullptr);

public:
	OperationType				_operationType;
	std::shared_ptr<IocpObject>	_owner;
};

class AcceptOver : public ExpOver
{
public:
	AcceptOver();

public:
	char	_buffer[128];	
	std::shared_ptr<GameSession> _session;
};

class RecvOver : public ExpOver
{
public:
	RecvOver();

	int SetBuffers();

public:
	RecvBuffer	_buffer;
	WSABUF		_wsaBuf[2];
};

class SendOver : public ExpOver
{
public:
	SendOver();

	void SetBuffers(const std::vector<std::shared_ptr<std::vector<char>>>&& buffers);

public:
	std::vector<WSABUF>	_wsaBufs;
	std::vector<std::shared_ptr<std::vector<char>>> _sendDataList;
};

class EventOver : public ExpOver
{
public:
	EventOver(OperationType op, int id = -1);

public:
	int _id;
};
