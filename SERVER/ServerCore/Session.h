#pragma once

#include "ExpOver.h"
#include "IocpCore.h"
#include "GameObject.h"

enum State : char {
	ST_ALLOC,
	ST_INGAME,
	ST_FREE
};

constexpr int VIEW_RANGE = 7;

class RecvOver;
class SendOver;
class Service;
class Party;
class NPC;
class Inventory;

class Session : public IocpObject
{
public:
	Session()
	{
		_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	}

	virtual ~Session();

public:
	void Send(const std::vector<char>& data);
	virtual bool ProcessPacket(const std::vector<char>& packet) abstract;

public:
	SOCKET GetSocket() const { return _socket; }
	State GetState() const { return _state.load(); }

	void SetState(State state) { _state.store(state); }
	void SetService(std::shared_ptr<Service> service) { _service = service; }

	bool TryExchangeState(State from, State to) { return _state.compare_exchange_strong(from, to); }

public:
	void doRecv();
	void doSend();

	void RecvCallback(DWORD numBytes);
	void SendCallback();

public:
	void Close();

public:
	// Interface ±¸Çö
	virtual HANDLE GetHandle() override;
	virtual void Dispatch(ExpOver* expOver, int numOfBytes = 0) abstract;

protected:
	std::atomic<State> _state{ ST_ALLOC };
	std::weak_ptr<Service> _service;
	SOCKET	_socket;

protected:
	std::atomic<int> _pendingIoCount{ 0 };
	std::atomic<bool> _shouldRelease{ false };

protected:
	concurrency::concurrent_queue<std::shared_ptr<std::vector<char>>> _sendQueue;
	std::atomic<bool> _isSending{ false };

protected:
	RecvOver	_recvOver;
	SendOver	_sendOver;
};

enum QuestStatus;
class QuestType;

class GameSession : 
	public Session, 
	public GameObject,
	public std::enable_shared_from_this<GameSession> {
public:
	GameSession();
	virtual ~GameSession() override;

public:
	virtual void Dispatch(ExpOver* expOver, int numOfBytes = 0) override;

public:
	virtual bool ProcessPacket(const std::vector<char>& packet) override;
	virtual bool IsVisible() const override { return _state == ST_INGAME; }
	virtual void TakeDamage(short damage) override;
	virtual void Die() override;
	virtual void Revive() override;

public:
	void OnHeal();

public:
	std::shared_ptr<GameSession> ConsumePendingPartyRequester();

public:
	std::shared_ptr<Inventory>& GetInventory() { return _inventory; }
	std::shared_ptr<Party> GetParty() const { return _party.lock(); }
	std::shared_ptr<const std::unordered_set<int>> GetViewList() const { return _viewList.load(); }
	std::shared_ptr<GameSession> GetPendingPartyRequester() const { return _pendingPartyRequester.load().lock(); }
	int GetUserID() const { return _userID; }
	struct UserData GetUserInfo() const;

	void SetParty(std::shared_ptr<Party> party) { _party = party; }
	void SetInventory(std::shared_ptr<Inventory> inventory) { _inventory = inventory; }
	void SetPendingPartyRequester(std::shared_ptr<GameSession> session) { _pendingPartyRequester.store(session); }
	void SetViewList(const std::unordered_set<int>& newViewList) { _viewList.store(std::make_shared<std::unordered_set<int>>(newViewList)); }
	void AddViewList(int id);
	void RemoveViewList(int id);
	void ClearViewList();
	void SetUserID(int userID) { _userID = userID; }
	void SetUserInfo(const UserData& userData);

	virtual void AddExp(short exp);

private:
	std::atomic<std::shared_ptr<std::unordered_set<int>>> _viewList;

private:
	std::weak_ptr<Party> _party;
	std::atomic<std::weak_ptr<GameSession>> _pendingPartyRequester;

private:
	std::shared_ptr<Inventory> _inventory;

private:
	int _userID{ -1 };
};