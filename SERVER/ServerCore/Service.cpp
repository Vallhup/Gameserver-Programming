#include "pch.h"
#include "Service.h"
#include "Listener.h"
#include "Party.h"
#include "ObjectManager.h"

Service::Service(std::shared_ptr<IocpCore> core, int maxSessionCount)
	: _iocpCore(core)
{
	for (auto& row : _navigationMap) {
		row.fill(true);
	}
}

bool Service::Start(std::string_view database, const std::string& map)
{
	LOG_INF("Start Service");

	// temp : rand() Seed Setting
	srand(static_cast<unsigned int>(time(nullptr)));

	// 0. running Flag 설정
	_running.store(true);

	// 1. WinSock 초기화
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) {
		LOG_ERR("WSAStartup Error");
		return false;
	}

	// 2. Monster Initialize / Monster Timer Thread Start
	InitNpcs(MAX_NPC);
	StartNpcTimerThread();

	LoadMap(map);

	// 3. QuestManager, DBManager Init
	_questManager->Init();
	_dbManager->Init(std::wstring().assign(database.begin(), database.end()));

	// 4. Listener에서 Accept 시작
	if (_listener == nullptr) {
		LOG_ERR("Listener allocation failed");
		return false;
	}

	if (_listener->StartAccept() == false) {
		LOG_ERR("Listener StartAccept failed");
		return false;
	}

	// 5. Worker Thread Start
	unsigned int threadCount = std::thread::hardware_concurrency();
	_workers.reserve(threadCount);
	for (unsigned int i = 0; i < threadCount; ++i) {
		_workers.emplace_back([this]()
			{
				while (_running.load()) {
					if (not _iocpCore->Dispatch()) {
						int error = WSAGetLastError();

						if (_running.load()) {
							if (error == WSAECONNRESET || error == WSAENOTCONN || error == ERROR_NETNAME_DELETED || error == WSA_OPERATION_ABORTED) {
								LOG_WRN("IOCP Dispatch expected error: %d", error);
							}
							else {
								LOG_ERR("IOCP Dispatch critical error: %d", error);
							}
						}

						break;
					}
				}
			});
	}

	return true;
}

void Service::CloseService()
{
	// 0) _running Flag 설정
	_running.store(false);

	_dbManager->Shutdown();

	// 1) Accept 종료
	_listener->StopAccept();

	// 2) Worker Thread join
	for (size_t i = 0; i < _workers.size(); ++i) {
		PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, 0, nullptr);
	}

	for (std::thread& worker : _workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	_workers.clear();

	// 3) Timer Thread join
	if (_npcTimerThread.joinable()) {
		_npcTimerThread.join();
	}

	// 4) Session 정리
	_objectManager->ForEachPlayer(
		[&](const std::shared_ptr<GameSession>& session)
		{
			CancelIoEx(session->GetHandle(), nullptr);
		}
	);
	_objectManager.reset();

	// 5) Listener 해제
	_listener.reset();

	// 6) IOCP Handle 해제
	_iocpCore.reset();

	// 7) WinSock 정리
	WSACleanup();
}

std::shared_ptr<GameObject> Service::FindObject(int id, bool player) const
{
	return _objectManager->FindObject(id, player);
}

int Service::AddObject(const std::shared_ptr<GameObject> object)
{
	return _objectManager->AddObject(object);
}

void Service::ReleaseSession(const std::shared_ptr<GameSession>& session)
{
	int id = session->GetId();

	session->Close();

	// 1. viewList 동기화
	std::unordered_set<int> viewList = *(session->GetViewList());

	for (int objId : viewList) {
		auto object = FindObject(objId);
		if (object->GetType() == ObjectType::PLAYER) {
			auto target = static_pointer_cast<GameSession>(object);
			if (nullptr == target) continue;

			auto packetForTarget = PacketFactory::BuildRemovePacket(*session);
			target->Send(packetForTarget);
		}
	}

	// 2. Sector에서 session 제거
	_viewManager->LeaveSector(session);

	// 3. 파티있으면 파티 상태 Update
	if (auto party = session->GetParty()) {
		party->Update();
	}

	// 4. State Setting
	session->SetState(ST_FREE);

	// 5. QuestManager에서 삭제
	_questManager->UnregisterPlayer(session->GetId());

	//_objectManager->RemoveObject(session);

	LOG_INF("Session %d finalized and erased", id);
}

void Service::InitNpcs(int npcCount)
{
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);

	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, Lua_SpawnMonster_Wrapper, 1);
	lua_setglobal(L, "spawnMonster");

	if (luaL_dofile(L, "monster_spawn.lua") != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		LOG_ERR("[Lua Error] %s", err);
		return;
	}

	lua_close(L);

	std::string NPCname = "NPC" + std::to_string(1);
	auto npc = std::make_shared<Npc>(-1, 1000, 1000, NPCname);
	AddObject(npc);
	EnterSector(npc);
}

void Service::StartNpcTimerThread()
{
	using namespace std::chrono;

	_npcTimerThread = std::thread([this]()
		{
			while (_running.load()) {
				Event event;
				while (_timerQueue.try_pop(event)) {
					auto now = high_resolution_clock::now();
					if (event.wakeupTime > now) {
						_timerQueue.push(event);
						break;
					}

					OperationType opType;
					switch (event.eventId) {
					case EV_MOVE:			opType = NpcMove; break;
					case EV_HEAL:			opType = NpcHeal; break;
					case EV_ATTACK:			opType = NpcAttack; break;
					case EV_PLAYER_HEAL:	opType = Heal; break;
					case EV_PLAYER_RESPAWN:	opType = Respawn; break;
					default: continue;
					}

					EventOver* eventOver = new EventOver(opType, event.objId);
					
					auto object = FindObject(event.objId);
					if (object->GetType() == ObjectType::PLAYER) {
						if (auto player = static_pointer_cast<GameSession>(FindObject(event.objId))) {
							eventOver->_owner = player;
						}

						else {
							delete eventOver;
							continue;
						}
					}
					
					else {
						if (auto npc = static_pointer_cast<Monster>(FindObject(event.objId))) {
							eventOver->_owner = npc;
						}

						else {
							delete eventOver;
							continue;
						}
					}
			
					PostQueuedCompletionStatus(_iocpCore->GetHandle(), 0, event.objId, reinterpret_cast<LPOVERLAPPED>(eventOver));
				}
				std::this_thread::sleep_for(1ms);
			}
		});
}

void Service::LoadMap(const std::string& filename)
{
	std::ifstream in{ filename };
	if (not in) {
		LOG_ERR("Map Loding failed");
		return;
	}

	std::string line;
	int y{ 0 };

	while (std::getline(in, line)) {
		if (line.length() < MAP_SIZE) {
			LOG_ERR("[%d] line is short");
			return;
		}

		for (int x = 0; x < MAP_SIZE; ++x) {
			if (line[x] == '1') {
				_navigationMap[y][x] = true;
			}

			else {
				_navigationMap[y][x] = false;
			}
		}
		++y;

		if (y >= MAP_SIZE) {
			break;
		}
	}

	if (y != MAP_SIZE) {
		LOG_ERR("Line is not Max");
		return;
	}

	LOG_INF("Map Loading Success");
}

void Service::OnPlayerLogin(const std::shared_ptr<GameSession>& session)
{
	_viewManager->HandlePlayerLoginNotify(session);
}

void Service::OnPlayerMove(const std::shared_ptr<GameSession>& session)
{
	_viewManager->HandlePlayerMoveNotify(session);
	_combatManager->HandlePlayerMove(session);
}

void Service::OnPlayerDeath(const std::shared_ptr<GameSession>& session)
{
	_viewManager->HandlePlayerDeathNotify(session);
}

void Service::OnPlayerRevive(const std::shared_ptr<GameSession>& session)
{
	// temp : 성능 테스트 할 때 부활 장소가 고정되어 있으니까 너무 빡셈
	while (true) {
		short x = rand() % 2000;
		short y = rand() % 2000;

		if (_navigationMap[y][x]) {
			session->SetPos(x, y);
			session->Send(PacketFactory::BuildMovePacket(*session));
			break;
		}
	}

	_viewManager->HandlePlayerReviveNotify(session);
}

void Service::OnNpcMove(const std::shared_ptr<Monster>& npc, int oldX, int oldY)
{
	_viewManager->HandleNpcMove(npc, oldX, oldY);
	_combatManager->HandleMonsterMove(npc);
}

void Service::OnNpcDeath(const std::shared_ptr<Monster>& monster, const std::shared_ptr<GameSession>& killer)
{
	_viewManager->HandleNpcDeath(monster);
	_questManager->HandleEvent(killer, KillMonster, monster->GetTypeId());
}

void Service::OnNpcRevive(const std::shared_ptr<Monster>& npc)
{
	_viewManager->HandleNpcRevive(npc);
}

void Service::EnterSector(const std::shared_ptr<GameObject>& object)
{
	_viewManager->EnterSector(object);
}

void Service::LeaveSector(const std::shared_ptr<GameObject>& object)
{
	_viewManager->LeaveSector(object);
}

void Service::EnterSector(const std::shared_ptr<GameObject>& object, int sx, int sy)
{
	_viewManager->EnterSector(object, sx, sy);
}

void Service::LeaveSector(const std::shared_ptr<GameObject>& object, int sx, int sy)
{
	_viewManager->LeaveSector(object, sx, sy);
}

std::unordered_set<int> Service::CollectVisibleObjects(const std::shared_ptr<GameObject>& object) const
{
	return _viewManager->CollectVisibleObjects(object);
}

std::unordered_set<int> Service::CollectViewList(const std::shared_ptr<GameObject>& object) const
{
	return _viewManager->CollectViewList(object);
}

void Service::OnChatRequest(int senderId, const char* msg, int targetId)
{
	_chatManager->HandleMessage(shared_from_this(), senderId, msg, targetId);
}

void Service::Broadcast(const std::vector<char>& buf)
{
	_objectManager->ForEachPlayer(
		[&](const std::shared_ptr<GameSession>& session) { session->Send(buf); });
}

bool Service::OnPacket(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	char packetType = packet[1];

	switch (packetType) {
	case CS_LOGIN:			return OnLogin(session, packet);
	case CS_LOGOUT:			return OnLogout(session, packet);
	case CS_MOVE:			return OnMove(session, packet);
	case CS_TELEPORT:		return OnTeleport(session, packet);
	case CS_ATTACK:			return OnAttack(session, packet);
	case CS_CHAT:			return OnChat(session, packet);
	case CS_PARTY_REQUEST:	return OnPartyRequest(session, packet);
	case CS_PARTY_RESPONSE: return OnPartyResponse(session, packet);
	case CS_PARTY_LEAVE:	return OnPartyLeave(session);
	case CS_USE_ITEM:		return OnUseItem(session, packet);
	case CS_TALK_TO_NPC:	return OnTalkToNpc(session, packet);
	case CS_QUEST_ACCEPT:	return OnQuestAccept(session, packet);

	default:
		LOG_WRN("Packet Type Error");
		return false;
	}
}

bool Service::OnLogin(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 1. packet 파싱
	auto requestPacket = PacketFactory::Deserialize<CS_LOGIN_PACKET>(packet);
	requestPacket.name[NAME_SIZE - 1] = '\0';

	const std::string userId = std::to_string(requestPacket.id);
	const std::wstring id = std::wstring().assign(userId.begin(), userId.end());

	// 중복 로그인 검사
	{
		std::shared_lock lock{ _inGameUsersMutex };
		if (_inGameUsers.contains(requestPacket.id)) {
			LOG_DBG("User[%d] is already in game now", requestPacket.id);

			session->Send(PacketFactory::BuildLoginFailPacket(*session));
			return false;
		}
	}

	{
		std::unique_lock lock{ _inGameUsersMutex };
		_inGameUsers.try_emplace(requestPacket.id, session);
	}

	// 2. ALLOC 상태를 INGAME으로 변경
	if (not session->TryExchangeState(ST_ALLOC, ST_INGAME)) {
		LOG_WRN("Session state is not Alloc");
		return false;
	}

	if (nullptr == _dbManager) {
		LOG_ERR("[Service] DBManager is not Init");
		
		session->Send(PacketFactory::BuildLoginFailPacket(*session));
		session->Close();

		return false;
	}

	if (not _dbManager->CheckUserID(id)) {
		LOG_WRN("ID[%s] is not DB ", userId.c_str());

		session->Send(PacketFactory::BuildLoginFailPacket(*session));
		session->Close();

		return false;
	}

	session->SetUserID(requestPacket.id);

	// 3. Session name 설정
	session->SetName(requestPacket.name);

	UserData userData;

	// Session Position 설정
	if (_dbManager->GetUserInfo(id, userData)) {
		session->SetUserInfo(userData);
		//session->SetPos(rand() % 2000, rand() % 2000);
		
		if (userData.partyId != -1) {
			auto party = _partyManager->GetParty(userData.partyId);
			if (nullptr != party) {
				party->RemoveMember(session->GetUserID());
				party->AddMember(session);
				party->Update();
			}
		}
	}

	else {
		LOG_ERR("[Service] DBManager GetPosition failed");
		session->Close();
		return false;
	}

	// Session Item List Select
	_itemManager->GetUserItem(session, shared_from_this());

	// 4. Session Container에 등록
	AddObject(session);

	// 5. Login / Stat Packet Send
	auto loginPacket = PacketFactory::BuildLoginOkPacket(*session);
	auto statPacket = PacketFactory::BuildStatChangePacket(*session);

	session->Send(loginPacket);
	session->Send(statPacket);

	// 6. Sector에 등록
	_viewManager->EnterSector(session);

	// 7. OnPlayerLogin 호출
	OnPlayerLogin(session);

	// 8. 자동 회복 Event Push
	_timerQueue.push(Event{ session->GetId(),
		std::chrono::high_resolution_clock::now() + std::chrono::seconds(5),
		EV_PLAYER_HEAL, 0 });

	// 9. QuestManager 등록
	auto quests = _dbManager->GetUserQuests(std::to_wstring(session->GetUserID()));
	_questManager->RegisterPlayer(session->GetId());
	_questManager->SetUserQuests(session, quests);


	LOG_DBG("Process Login Packet Success");
	return true;
}

bool Service::OnLogout(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	{
		std::unique_lock lock{ _inGameUsersMutex };
		if (_inGameUsers.contains(session->GetUserID())) {
			_inGameUsers.erase(session->GetUserID());
		}
	}

	UserData userData = session->GetUserInfo();
	auto quests = _questManager->GetUserQuestData(session->GetId());

	_dbManager->UpdateUserInfo(userData);
	_dbManager->UpdateUserQuests(userData.id, quests);

	session->Close();
	return true;
}

bool Service::OnMove(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	if (not session->IsAlive()) {
		return false;
	}

	// 1. packet 파싱
	auto requestPacket = PacketFactory::Deserialize<CS_MOVE_PACKET>(packet);
	//session->_lastMoveTime = requestPacket.move_time;

	// 2. lastMoveTime과 현재 시각 계산해서 0.5초에 1번씩 움직이도록 제한
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if ((now - session->_lastMoveTime) < 500) {
		LOG_INF("Move Cooldown");
		return false;
	}
	session->_lastMoveTime = now;

	// 2. Pos 업데이트
	short x = session->GetX();
	short y = session->GetY();

	short nextX = x;
	short nextY = y;

	switch (requestPacket.direction) {
	case UP:	if (y > 0)				nextY--; break;
	case DOWN:	if (y < W_HEIGHT - 1)	nextY++; break;
	case LEFT:	if (x > 0)				nextX--; break;
	case RIGHT: if (x < W_WIDTH - 1)	nextX++; break;
	}

	if (not _navigationMap[nextY][nextX]) {
		return false;
	}

	auto oldSector = Sector::GetSector(x, y);
	auto newSector = Sector::GetSector(nextX, nextY);

	session->SetPos(nextX, nextY);

	// 3. Sector 동기화
	if (oldSector != newSector) {
		_viewManager->LeaveSector(session, oldSector.first, oldSector.second);
		_viewManager->EnterSector(session, newSector.first, newSector.second);
	}

	// 4. OnPlayerMove 호출
	OnPlayerMove(session);

	LOG_DBG("Process Move Packet Success");
	return true;
}

bool Service::OnTeleport(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	if (not session->IsAlive()) {
		return false;
	}


	while (true) {
		short x = rand() % 2000;
		short y = rand() % 2000;

		if (_navigationMap[y][x]) {
			session->SetPos(x, y);
			session->Send(PacketFactory::BuildMovePacket(*session));
			return true;
		}
	}
}

bool Service::OnAttack(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	if (not session->IsAlive()) {
		return false;
	}

	_combatManager->HandlePlayerAttack(session);

	SC_ATTACK_NOTIFY_PACKET p;
	p.size = sizeof(p);
	p.type = SC_ATTACK_NOTIFY;
	p.attackerId = session->GetId();
	p.x[0] = session->GetX() - 1; p.y[0] = session->GetY();
	p.x[1] = session->GetX() + 1; p.y[1] = session->GetY();
	p.x[2] = session->GetX();     p.y[2] = session->GetY() - 1;
	p.x[3] = session->GetX();     p.y[3] = session->GetY() + 1;

	auto visible = _viewManager->CollectViewList(session);
	for (int id : visible) {
		auto object = FindObject(id);
		if (object->GetType() == ObjectType::PLAYER) {
			if (auto target = std::static_pointer_cast<GameSession>(object)) {
				target->Send(PacketFactory::Serialize(p));
			}
		}
	}

	return true;
}

bool Service::OnChat(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱 / null termination
	auto requestPacket = PacketFactory::Deserialize<CS_CHAT_PACKET>(packet);
	requestPacket.message[CHAT_SIZE - 1] = '\0';

	// 2. OnChatRequest 호출
	OnChatRequest(session->GetUserID(), requestPacket.message, session->GetUserID());

	LOG_DBG("Process Chat Packet Success");
	return true;
}

bool Service::OnPartyRequest(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱
	auto requestPacket = PacketFactory::Deserialize<CS_PARTY_REQUEST_PACKET>(packet);

	LOG_ERR("[%d]", requestPacket.targetId);

	// 2. Target Session Find
	auto object = _objectManager->FindObject(requestPacket.targetId, true);
	if ((nullptr != object) and (object->GetType() == ObjectType::PLAYER)) {
		auto target = static_pointer_cast<GameSession>(object);
		if ((nullptr == target) or (target->GetState() != ST_INGAME)) {
			LOG_WRN("Party Request Target Session Error %d", requestPacket.targetId);
			return false;
		}

		LOG_ERR("[%d] = [%d]", object->GetId(), target->GetId());
		LOG_ERR("[%d] -> [%d]", session->GetUserID(), target->GetUserID());

		return _partyManager->HandleRequest(session, target);
	}

	else {
		LOG_ERR("Party Request Target Session Error %d", requestPacket.targetId);
		return false;
	}
}

bool Service::OnPartyResponse(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱
	auto responsePacket = PacketFactory::Deserialize<CS_PARTY_RESPONSE_PACKET>(packet);

	return _partyManager->HandleResponse(session, responsePacket.acceptFlag);
}

bool Service::OnPartyLeave(const std::shared_ptr<GameSession>& session)
{
	return _partyManager->HandleLeave(session);
}

void Service::OnPartyDisband(int partyId)
{
	return _partyManager->DisbandParty(partyId);
}

bool Service::OnUseItem(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱
	auto responsePacket = PacketFactory::Deserialize<CS_USE_ITEM_PACKET>(packet);

	// 2. ItemManager, QuestManager 호출
	bool retVal{ false };

	retVal = (_itemManager->UseItem(session, responsePacket.itemId, shared_from_this())) and
		(_questManager->HandleEvent(session, QuestEventType::UseItem, responsePacket.itemId));

	return retVal;
}

bool Service::OnTalkToNpc(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱
	auto responsePacket = PacketFactory::Deserialize<CS_TALK_TO_NPC_PACKET>(packet);
	const int npcId = responsePacket.npcId;
	const int sid = session->GetId();

	// 2. NPC 존재 여부 확인
	auto object = FindObject(npcId);
	if ((nullptr == object) or (object->GetType() != ObjectType::NPC)) {
		LOG_ERR("NPC[%d] not found", npcId);
		return false;
	}

	if (_questManager->HandleEvent(session, QuestEventType::TalkToNpc, npcId)) {
		UpdateQuestSymbol(session, npcId);
	}

	return true;
}

bool Service::OnQuestAccept(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet)
{
	// 0. INGAME이 아니면 실행 X
	if (session->GetState() != ST_INGAME) {
		LOG_WRN("Session state is not Ingame");
		return false;
	}

	// 1. Packet 파싱
	auto responsePacket = PacketFactory::Deserialize<CS_QUEST_ACCEPT_PACKET>(packet);

	return _questManager->AcceptQuest(session, responsePacket.questId);
}

char Service::GetQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId)
{
	return _questManager->GetNpcQuestSymbol(session, npcId);
}

int Service::GetRandomInterval(int minMs, int maxMs)
{
	thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_int_distribution<int> dist{ minMs, maxMs };

	return dist(rng);
}

std::vector<std::pair<int, int>> Service::GetUserItems(const std::shared_ptr<GameSession>& session)
{
	return _dbManager->GetUserItems(std::to_wstring(session->GetUserID()));
}

bool Service::UserGetItem(const std::shared_ptr<GameSession>& session, int itemId, int count)
{
	return _dbManager->UserGetItem(session->GetUserID(), itemId, count);
}

bool Service::UserUseItem(const std::shared_ptr<GameSession>& session, int itemId, int count)
{
	return _dbManager->UserUseItem(session->GetUserID(), itemId, count);
}

const std::vector<int> Service::GetPlayersInTile(short x, short y)
{
	return _objectManager->GetPlayerInTile(x, y, shared_from_this());
}

const std::vector<int> Service::GetMonstersInTile(short x, short y)
{
	return _objectManager->GetMonsterInTile(x, y, shared_from_this());
}

void Service::UpdateQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId)
{
	char symbol = _questManager->GetNpcQuestSymbol(session, npcId);
	session->Send(PacketFactory::BuildQuestSymbolUpdatePacket(npcId, symbol));
}

std::shared_ptr<Service> Service::Create(std::shared_ptr<IocpCore> core, int maxSessionCount)
{
	std::shared_ptr<Service> service = std::make_shared<Service>(core, maxSessionCount);

	service->_iocpCore = std::make_shared<IocpCore>(service);
	service->_listener = std::make_shared<Listener>(service);
	service->_viewManager = std::make_shared<ViewManager>(service);
	service->_questManager = std::make_shared<QuestManager>(service);
	service->_combatManager = std::make_shared<CombatManager>(service);
	service->_dbManager = std::make_shared<DBManager>();
	service->_chatManager = std::make_shared<ChatManager>();
	service->_itemManager = std::make_shared<ItemManager>();
	service->_partyManager = std::make_shared<PartyManager>();
	service->_objectManager = std::make_shared<ObjectManager>();

	return service;
}

int Service::Lua_SpawnMonster(lua_State* L)
{
	int x = static_cast<int>(lua_tointeger(L, 1));
	int y = static_cast<int>(lua_tointeger(L, 2));
	const char* typeStr = lua_tostring(L, 3);
	const char* moveStr = lua_tostring(L, 4);
	int level = static_cast<int>(lua_tointeger(L, 5));
	
	MonsterType mType;
	MovementType mvType;

	std::string type;
	std::string move;

	if (strcmp(typeStr, "Peace") == 0) {
		mType = MonsterType::Peace; 
		type = "P";
	}

	else if (strcmp(typeStr, "Agro") == 0) {
		mType = MonsterType::Agro;
		type = "A";
	}

	else {
		return 0;
	}

	if (strcmp(moveStr, "Fixed") == 0) {
		mvType = MovementType::Fixed;
		move = "F";
	}

	else if (strcmp(moveStr, "Roaming") == 0) {
		mvType = MovementType::Roaming;
		move = "R";
	}

	else {
		return 0;
	}

	static int idCounter = 0;

	std::string name = "M" + type + move + std::to_string(idCounter++);

	auto monster = std::make_shared<Monster>(-1, x, y, name, mType, mvType);
	monster->SetLevel(level);
	monster->SetService(shared_from_this());
	AddObject(monster);
	EnterSector(monster);
	_iocpCore->Register(monster);

	return 0;
}

int Lua_SpawnMonster_Wrapper(lua_State* L)
{
	Service* raw = static_cast<Service*>(lua_touserdata(L, lua_upvalueindex(1)));
	auto service = raw->shared_from_this();
	return service->Lua_SpawnMonster(L);
}
