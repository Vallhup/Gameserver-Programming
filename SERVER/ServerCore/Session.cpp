#include "pch.h"
#include "Session.h"

Session::~Session()
{ 
	LOG_DBG("Session %d Delete", _sessionId);
}

void Session::Send(const std::vector<char>& data)
{
	if (_state.load() == ST_FREE) {
		Close();
		return;
	}

	if (_socket == INVALID_SOCKET) {
		return;
	}

	auto buf = std::make_shared<std::vector<char>>(data);
	_sendQueue.push(buf);

	bool expected{ false };
	if (_isSending.compare_exchange_strong(expected, true)) {
		doSend();
	}
}

void Session::doRecv()
{
	if ((ST_FREE == _state.load()) or (_socket == INVALID_SOCKET)) {
		Close();
		return;
	}

	DWORD recvFlag = 0;

	auto sp = static_cast<GameSession*>(this);

	_recvOver.Init(sp->shared_from_this());
	int wsaBufCount = _recvOver.SetBuffers();

	_pendingIoCount.fetch_add(1);
	int result = WSARecv(_socket, _recvOver._wsaBuf, wsaBufCount, NULL, &recvFlag, reinterpret_cast<LPWSAOVERLAPPED>(&_recvOver), NULL);
	if (SOCKET_ERROR == result) {
		int error = WSAGetLastError();
		if (WSA_IO_PENDING != error) {
			if (error == WSAECONNRESET || error == WSAENOTCONN || error == WSAESHUTDOWN || error == WSA_OPERATION_ABORTED) {
				LOG_WRN("WSARecv disconnected/aborted: %d", error);
			}
			else {
				LOG_ERR("WSARecv failed: %d", error);
			}
		}
	}
}

void Session::doSend()
{
	if ((ST_FREE == _state.load()) or (_socket == INVALID_SOCKET)) {
		Close();
		return;
	}

	constexpr size_t MAX_PACKET = 32;
	std::vector<std::shared_ptr<std::vector<char>>> packets;
	packets.reserve(MAX_PACKET);

	std::shared_ptr<std::vector<char>> sendData;
	while ((packets.size() < MAX_PACKET) and (_sendQueue.try_pop(sendData))) {
		packets.push_back(std::move(sendData));
	}

	if (packets.empty()) {
		_isSending.store(false);
		return;
	}

	auto sp = static_cast<GameSession*>(this);
	auto sendOver = new SendOver;

	sendOver->Init(sp->shared_from_this());
	sendOver->SetBuffers(std::move(packets));

	DWORD bytesSent{ 0 };
	_pendingIoCount.fetch_add(1);
	if (SOCKET_ERROR == WSASend(_socket, sendOver->_wsaBufs.data(), static_cast<DWORD>(sendOver->_wsaBufs.size()), &bytesSent, 0, reinterpret_cast<LPWSAOVERLAPPED>(sendOver), NULL)) {
		int error = WSAGetLastError();
		if ((error == WSAECONNRESET) or (error == WSAENOTCONN) or (error == WSAESHUTDOWN)) {
			LOG_INF("Client %d disconnected", _sessionId);
		}

		else if (error != WSA_IO_PENDING) {
			LOG_ERR("WSASend failed: %d", error);
		}

		sendOver->_owner.reset();
		delete sendOver;

		_isSending.store(false);
		Close();
		return;
	}
}

void Session::RecvCallback(DWORD numBytes)
{
	if (numBytes == 0) {
		LOG_INF("Client %d disconnected", _sessionId);
		Close();
		return;
	}

	if (not _recvOver._buffer.Write(nullptr, numBytes)) {
		LOG_ERR("RecvBuffer overflow in session %d", _sessionId);
		Close();
		return;
	}

	std::vector<char> readBuffer(numBytes);
	_recvOver._buffer.Read(readBuffer.data(), numBytes);

	ProcessPacket(readBuffer);

	if (_pendingIoCount.fetch_sub(1) == 1) {
		if (_shouldRelease) {
			if (auto service = _service.lock()) {
				auto self = static_cast<GameSession*>(this);
				service->ReleaseSession(self->shared_from_this());
			}
		}
	}

	_recvOver._owner.reset();

	doRecv();
}

void Session::SendCallback()
{
	if (_pendingIoCount.fetch_sub(1) == 1) {
		if (_shouldRelease) {
			if (auto service = _service.lock()) {
				auto self = static_cast<GameSession*>(this);
				service->ReleaseSession(self->shared_from_this());
			}
		}
	}

	doSend();
}

void Session::Close()
{
	// 이미 닫힌 Session이면 return, 닫히지 않았으면 state를 Free로 변경
	if (_state.exchange(ST_FREE) == ST_FREE) {
		return;
	}

	shutdown(_socket, SD_BOTH);
	CancelIoEx(GetHandle(), nullptr);
	closesocket(_socket);

	_shouldRelease = true;
	_socket = INVALID_SOCKET;
	
	if (auto service = _service.lock()) {
		auto sp = static_cast<GameSession*>(this);
		service->ReleaseSession(sp->shared_from_this());
	}

	LOG_INF("Closing Session[%d]", _sessionId);
}

HANDLE Session::GetHandle()
{
	return reinterpret_cast<HANDLE>(_socket);
}

void GameSession::Dispatch(ExpOver* expOver, int numOfBytes)
{
	if (ST_FREE == _state.load()) {
		Close();
		return;
	}

	switch (expOver->_operationType) {
	case OperationType::Recv:
		RecvCallback(numOfBytes);
		break;

	case OperationType::Send:
		SendCallback();
		expOver->_owner.reset();
		delete reinterpret_cast<SendOver*>(expOver);
		break;

	case OperationType::Heal:
		OnHeal();
		delete reinterpret_cast<EventOver*>(expOver);
		break;

	case OperationType::Respawn:
		Revive();
		delete reinterpret_cast<EventOver*>(expOver);
		break;

	default:
		LOG_WRN("Unknown operation in Dispatch: op=%d", expOver->_operationType);
		break;
	}
}

GameSession::GameSession() : Session(), GameObject()
{
	_type = ObjectType::PLAYER;
	_viewList.store(std::make_shared<std::unordered_set<int>>());
	_inventory = nullptr;
}

GameSession::~GameSession()
{
	LOG_DBG("GameSession[%d] Delete", _id);
}

bool GameSession::ProcessPacket(const std::vector<char>& packet)
{
	if (ST_FREE == _state.load()) {
		LOG_WRN("Session state is Free");
		return false;
	}

	auto service = _service.lock();
	if (not service) {
		LOG_WRN("Service expired in CS_LOGIN");
		return false;
	}

	char packetType = packet[1];
	bool handled = false;

	switch (packetType) {
	case CS_LOGIN:
	case CS_LOGOUT:
	case CS_MOVE :
	case CS_TELEPORT:
	case CS_ATTACK:
	case CS_CHAT :
	case CS_PARTY_REQUEST:
	case CS_PARTY_RESPONSE:
	case CS_PARTY_LEAVE:
	case CS_USE_ITEM:
	case CS_TALK_TO_NPC:
	case CS_QUEST_ACCEPT:
		handled = service->OnPacket(shared_from_this(), packet);
		break;

	default:
		LOG_WRN("Packet Type Error");
		return false;
	}

	return handled;
}

void GameSession::TakeDamage(short damage)
{
	if (not _isAlive.load()) {
		return;
	}

	DecreaseHp(damage);
	Send(PacketFactory::BuildStatChangePacket(*this));

	std::string str = "Player[" + std::to_string(_userID) + "] suffered " + std::to_string(damage) + " damage.";
	Send(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));

	if (CheckDie()) {
		Die();
		return;
	}
}

void GameSession::Die()
{
	GameObject::Die();

	std::string str = "Player[" + std::to_string(_userID) + "] is die";
	Send(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));

	if (auto service = _service.lock()) {
		service->OnPlayerDeath(shared_from_this());
		service->_timerQueue.push(Event{ _id,
			std::chrono::high_resolution_clock::now() + std::chrono::seconds(3),
			EV_PLAYER_RESPAWN, 0 });
	}
}

void GameSession::Revive()
{
	GameObject::Revive();

	_exp /= 2;
	Send(PacketFactory::BuildStatChangePacket(*this));

	std::string str = "Player[" + std::to_string(_userID) + "] is revive";
	Send(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));

	if (auto service = _service.lock()) {
		service->OnPlayerRevive(shared_from_this());
		service->_timerQueue.push(Event{ _id,
				std::chrono::high_resolution_clock::now() + std::chrono::seconds(5),
				EV_PLAYER_HEAL, 0 });
	}
}

void GameSession::OnHeal()
{
	if ((not _isAlive) or (ST_INGAME != _state.load())) {
		return;
	}

	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	// 1. 이미 maxHp면 다음 Event Push하고 끝
	if (_hp == _maxHp) {
		service->_timerQueue.push(Event{ _id,
			std::chrono::high_resolution_clock::now() + std::chrono::seconds(5),
			EV_PLAYER_HEAL, 0 });
		return;
	}

	short hp = _hp;

	// 2. maxHp의 10%만큼 회복
	_hp = std::min<short>(_hp + (_maxHp / 10), _maxHp);

	short healHp = _hp - hp;

	// 3. Stat Update
	Send(PacketFactory::BuildStatChangePacket(*this));

	std::string str = "Player[" + std::to_string(_userID) + "] recovered " + std::to_string(healHp) + " hp";
	Send(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));
	
	// 4, 다음 Event Push
	service->_timerQueue.push(Event{ _id,
		std::chrono::high_resolution_clock::now() + std::chrono::seconds(5),
		EV_PLAYER_HEAL, 0 });
}

std::shared_ptr<GameSession> GameSession::ConsumePendingPartyRequester()
{
	auto temp = _pendingPartyRequester.load();
	_pendingPartyRequester.store(std::weak_ptr<GameSession>{});

	return temp.lock();
}

UserData GameSession::GetUserInfo() const
{
	UserData userData;

	userData.id = _userID;
	userData.level = _level;
	userData.exp = _exp;
	userData.hp = _hp;
	userData.maxHp = _maxHp;
	userData.x = _x;
	userData.y = _y;

	if (auto party = GetParty()) {
		userData.partyId = party->GetId();
	}

	else {
		userData.partyId = -1;
	}

	return userData;
}

void GameSession::AddViewList(int id)
{
	while (true) {
		auto oldViewList = _viewList.load();
		auto newViewList = std::make_shared<std::unordered_set<int>>(*oldViewList);
		newViewList->insert(id);

		if (_viewList.compare_exchange_strong(oldViewList, newViewList)) {
			break;
		}
	}
}

void GameSession::RemoveViewList(int id)
{
	while (true) {
		auto oldViewList = _viewList.load();
		auto newViewList = std::make_shared<std::unordered_set<int>>(*oldViewList);
		newViewList->erase(id);

		if (_viewList.compare_exchange_strong(oldViewList, newViewList)) {
			break;
		}
	}
}

void GameSession::ClearViewList()
{
	_viewList.store(std::make_shared<std::unordered_set<int>>());
}

void GameSession::SetUserInfo(const UserData& userData)
{
	_userID = userData.id;
	_level = userData.level;
	_exp = userData.exp;
	_hp = userData.hp;
	_maxHp = userData.maxHp;
	_x = userData.x;
	_y = userData.y;
	_damage = _level * 10;
}

void GameSession::AddExp(short exp)
{
	GameObject::AddExp(exp);

	std::string str = "Player[" + std::to_string(_userID) + "] has an experience value of " + std::to_string(exp);
	Send(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));
}