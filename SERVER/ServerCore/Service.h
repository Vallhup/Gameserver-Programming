#pragma once

#include "include/lua.hpp"

class IocpCore;
class Listener;
class GameObject;
class Party;
class PartyManager;
class ChatManager;
class QuestManager;
class ItemManager;
class ObjectManager;
class ViewManager;
class CombatManager;
class Monster;
class DBManager;

enum EventType : char {
	EV_MOVE,
	EV_HEAL,
	EV_ATTACK,
	EV_PLAYER_HEAL,
	EV_PLAYER_RESPAWN
};

struct Event {
	int objId;
	std::chrono::high_resolution_clock::time_point wakeupTime;
	char eventId;
	int targetId;

	bool operator<(const Event& other) const {
		return wakeupTime > other.wakeupTime;
	}
};

class Service : public std::enable_shared_from_this<Service>
{
public:
	Service(std::shared_ptr<IocpCore> core, int maxSessionCount);

public:
	bool Start(std::string_view database, const std::string& map);
	void CloseService();

public:
	std::shared_ptr<GameObject> FindObject(int id, bool player = false) const;
	int AddObject(const std::shared_ptr<GameObject> object);
	void ReleaseSession(const std::shared_ptr<GameSession>& session);

public:
	void InitNpcs(int npcCount);
	void StartNpcTimerThread();

	void LoadMap(const std::string& filename);

	void OnPlayerLogin(const std::shared_ptr<GameSession>& session);
	void OnPlayerMove(const std::shared_ptr<GameSession>& session);
	void OnPlayerDeath(const std::shared_ptr<GameSession>& session);
	void OnPlayerRevive(const std::shared_ptr<GameSession>& session);

	void OnNpcMove(const std::shared_ptr<Monster>& npc, int oldX, int oldY);
	void OnNpcDeath(const std::shared_ptr<Monster>& monster, const std::shared_ptr<GameSession>& killer);
	void OnNpcRevive(const std::shared_ptr<Monster>& npc);

public:
	void EnterSector(const std::shared_ptr<GameObject>& object);
	void LeaveSector(const std::shared_ptr<GameObject>& object);
	void EnterSector(const std::shared_ptr<GameObject>& object, int sx, int sy);
	void LeaveSector(const std::shared_ptr<GameObject>& object, int sx, int sy);

	std::unordered_set<int> CollectVisibleObjects(const std::shared_ptr<GameObject>& object) const;
	std::unordered_set<int> CollectViewList(const std::shared_ptr<GameObject>& object) const;

public:
	void OnChatRequest(int senderId, const char* msg, int targetId = -1);
	void Broadcast(const std::vector<char>& buf);

public:
	bool OnPacket(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnLogin(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnLogout(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnMove(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnTeleport(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnAttack(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnChat(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnPartyRequest(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnPartyResponse(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnPartyLeave(const std::shared_ptr<GameSession>& session);
	void OnPartyDisband(int partyId);
	bool OnUseItem(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnTalkToNpc(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);
	bool OnQuestAccept(const std::shared_ptr<GameSession>& session, const std::vector<char>& packet);

public:
	char GetQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId);
	int GetRandomInterval(int minMs, int maxMs);
	std::shared_ptr<IocpCore>& GetIocpCore() { return _iocpCore; }

	std::vector<std::pair<int, int>> GetUserItems(const std::shared_ptr<GameSession>& session);
	bool UserGetItem(const std::shared_ptr<GameSession>& session, int itemId, int count);
	bool UserUseItem(const std::shared_ptr<GameSession>& session, int itemId, int count);

	const std::vector<int> GetPlayersInTile(short x, short y);
	const std::vector<int> GetMonstersInTile(short x, short y);

	void UpdateQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId);

public:
	int Lua_SpawnMonster(lua_State* L);

public:
	static std::shared_ptr<Service> Create(std::shared_ptr<IocpCore> core, int maxSessionCount = 10);

public:
	std::array<std::array<bool, 2000>, 2000> _navigationMap;
	std::vector<std::thread> _workers;
	std::thread _npcTimerThread;
	concurrency::concurrent_priority_queue<Event> _timerQueue;
	std::atomic<bool> _running{ false };

	std::unordered_map<int, std::weak_ptr<GameSession>> _inGameUsers;
	std::shared_mutex _inGameUsersMutex;

private:
	std::shared_ptr<IocpCore>	   _iocpCore;
	std::shared_ptr<Listener>      _listener;
	std::shared_ptr<DBManager>     _dbManager;
	std::shared_ptr<ChatManager>   _chatManager;
	std::shared_ptr<ItemManager>   _itemManager;
	std::shared_ptr<ViewManager>   _viewManager;
	std::shared_ptr<PartyManager>  _partyManager;
	std::shared_ptr<QuestManager>  _questManager;
	std::shared_ptr<ObjectManager> _objectManager;
	std::shared_ptr<CombatManager> _combatManager;
};

int Lua_SpawnMonster_Wrapper(struct lua_State* L);