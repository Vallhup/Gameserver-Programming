#pragma once

#include <sqlext.h>

struct UserData {
	int id;
	int level;
	long long exp;
	short hp;
	short maxHp;
	short x;
	short y;
	int partyId;
};

struct QuestData {
	int questId;
	int progress;
};

class DBManager
{
public:
	DBManager() = default;
	~DBManager();

	bool Init(const std::wstring& database);
	void Shutdown();

	bool CheckUserID(const std::wstring& userID);

	bool GetUserInfo(const std::wstring& userID, UserData& outData);
	bool UpdateUserInfo(const UserData& userData);

	std::vector<std::pair<int, int>> GetUserItems(const std::wstring& userID);
	bool UserGetItem(int userId, int itemId, int count);
	bool UserUseItem(int userId, int itemId, int count);

	std::vector<QuestData> GetUserQuests(const std::wstring& userID);
	bool UpdateUserQuests(int userId, const std::vector<QuestData>& quests);

private:
	void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);
	void EnsureThreadConnection(const std::wstring& database);

	//void ThreadLoop();
	
private:
	static SQLHENV _hEnv;
	static thread_local SQLHDBC _hDbc;

	std::wstring _database;
};

