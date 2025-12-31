#include "pch.h"
#include "DBManager.h"

SQLHENV DBManager::_hEnv = SQL_NULL_HENV;
thread_local SQLHDBC DBManager::_hDbc = SQL_NULL_HDBC;

DBManager::~DBManager()
{
    Shutdown();
}

bool DBManager::Init(const std::wstring& database)
{
    // 1. ODBC 환경 Handle Alloc
    SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_hEnv);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle ENV Failed");
        return false;
    }

    retcode = SQLSetEnvAttr(_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLSetEnvAttr Failed");
        return false;
    }

    _database = database;

    // 4. DB 접속 전용 Thread
    /*_running.store(true);
    _dbThread = std::thread(&DBManager::ThreadLoop, this);*/

    LOG_INF("[DBManager] MSSQL Connect Success");
    return true;
}

void DBManager::Shutdown()
{
    // 2. ODBC Handle 해제
    if (_hDbc != SQL_NULL_HDBC) {
        SQLDisconnect(_hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, _hDbc);
    }

    if (_hEnv != SQL_NULL_HDBC) {
        SQLFreeHandle(SQL_HANDLE_ENV, _hEnv);
    }

    LOG_INF("[DBManager] Shutdown Success");
}

bool DBManager::CheckUserID(const std::wstring& userID)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    SQLINTEGER userId{ -1 };
    SQLLEN cbUserId{ 0 };

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query = L"EXEC select_user_id " + userID;
    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLExecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    // 3. Column Binding
    retcode = SQLBindCol(hStmt, 1, SQL_C_LONG, &userId, 100, &cbUserId);

    retcode = SQLFetch(hStmt);
    if ((retcode == SQL_SUCCESS) or (retcode == SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("ID[%d] Success", userId);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return true;
    }

    else {
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }
}

bool DBManager::GetUserInfo(const std::wstring& userID, UserData& outData)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    UserData userData{ -1, };
    SQLLEN cbUserData[8]{};

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query = L"EXEC select_user_info " + userID;
    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    // 3. Column Binding
    retcode = SQLBindCol(hStmt, 1, SQL_C_LONG, &userData.id, 100, &cbUserData[0]);
    retcode = SQLBindCol(hStmt, 2, SQL_C_LONG, &userData.level, 100, &cbUserData[1]);
    retcode = SQLBindCol(hStmt, 3, SQL_C_LONG, &userData.exp, 100, &cbUserData[2]);
    retcode = SQLBindCol(hStmt, 4, SQL_C_SHORT, &userData.hp, 100, &cbUserData[3]);
    retcode = SQLBindCol(hStmt, 5, SQL_C_SHORT, &userData.maxHp, 100, &cbUserData[4]);
    retcode = SQLBindCol(hStmt, 6, SQL_C_SHORT, &userData.x, 100, &cbUserData[5]);
    retcode = SQLBindCol(hStmt, 7, SQL_C_SHORT, &userData.y, 100, &cbUserData[6]);
    retcode = SQLBindCol(hStmt, 8, SQL_C_LONG, &userData.partyId, 100, &cbUserData[7]);

    retcode = SQLFetch(hStmt);
    if ((retcode == SQL_SUCCESS) or (retcode == SQL_SUCCESS_WITH_INFO)) {
        outData = userData;
    }

    LOG_INF("[DBManager] GetUserInfo Success");
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

bool DBManager::UpdateUserInfo(const UserData& userData)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query =
        L"EXEC update_user_info " + std::to_wstring(userData.id) +
        L", " + std::to_wstring(userData.level) +
        L", " + std::to_wstring(userData.exp) +
        L", " + std::to_wstring(userData.hp) +
        L", " + std::to_wstring(userData.maxHp) +
        L", " + std::to_wstring(userData.x) +
        L", " + std::to_wstring(userData.y) +
        L", " + std::to_wstring(userData.partyId);

    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    LOG_INF("[DBManager] User[%d] UpdateUserInfo Success", userData.id);
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

std::vector<std::pair<int, int>> DBManager::GetUserItems(const std::wstring& userID)
{
    EnsureThreadConnection(_database);

    std::vector<std::pair<int, int>> result;

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return result;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query = L"EXEC select_user_item " + userID;
    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return result;
    }

    else {
        SQLINTEGER itemID{ 0 };
        SQLINTEGER count{ 0 };

        SQLLEN cbItemID{ 0 }, cbCount{ 0 };

        retcode = SQLBindCol(hStmt, 1, SQL_C_LONG, &itemID, 100, &cbItemID);
        retcode = SQLBindCol(hStmt, 2, SQL_C_LONG, &count, 100, &cbCount);

        while ((retcode = SQLFetch(hStmt) != SQL_NO_DATA)) {
            if ((retcode == SQL_SUCCESS) or (retcode == SQL_SUCCESS_WITH_INFO)) {
                result.emplace_back(static_cast<int>(itemID), static_cast<int>(count));
            }

            else {
                LOG_ERR("[DBManager] GetUserItems failed");
                break;
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return result;
}

bool DBManager::UserGetItem(int userId, int itemId, int count)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query = 
        L"EXEC user_get_item " + std::to_wstring(userId) +
        L", " + std::to_wstring(itemId) +
        L", " + std::to_wstring(count);

    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

bool DBManager::UserUseItem(int userId, int itemId, int count)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query =
        L"EXEC user_use_item " + std::to_wstring(userId) +
        L", " + std::to_wstring(itemId) +
        L", " + std::to_wstring(count);

    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

std::vector<QuestData> DBManager::GetUserQuests(const std::wstring& userID)
{
    EnsureThreadConnection(_database);

    std::vector<QuestData> result;

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return result;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    std::wstring query = L"EXEC select_user_quests " + userID;
    retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLexecDirect failed");
        HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return result;
    }

    else {
        SQLINTEGER questID{ 0 };
        SQLINTEGER progressKill{ 0 };

        SQLLEN cbQuestID{ 0 }, cbProgressKill{ 0 }, cbIsCompleted{ 0 };

        retcode = SQLBindCol(hStmt, 1, SQL_C_LONG, &questID, 100, &cbQuestID);
        retcode = SQLBindCol(hStmt, 2, SQL_C_LONG, &progressKill, 100, &cbProgressKill);

        while ((retcode = SQLFetch(hStmt) != SQL_NO_DATA)) {
            if ((retcode == SQL_SUCCESS) or (retcode == SQL_SUCCESS_WITH_INFO)) {
                result.emplace_back(questID, progressKill);
            }

            else {
                LOG_ERR("[DBManager] GetUserItems failed");
                break;
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return result;
}

bool DBManager::UpdateUserQuests(int userId, const std::vector<QuestData>& quests)
{
    EnsureThreadConnection(_database);

    SQLHSTMT hStmt{ SQL_NULL_HSTMT };
    SQLRETURN retcode;

    // 1. Statement Handle Alloc
    retcode = SQLAllocHandle(SQL_HANDLE_STMT, _hDbc, &hStmt);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_INF("[DBManager] SQLAllocHandle STMT Failed");
        return false;
    }

    //SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, 0);

    // 2. Query 실행
    int count = quests.size();
    std::vector<std::wstring> querys;

    if (0 == count) {
        querys.emplace_back(L"EXEC delete_user_quests " + std::to_wstring(userId));
    }

    else {
        querys.reserve(count);

        for (int i = 0; i < count; ++i) {
            querys.emplace_back(L"EXEC update_user_quests " + std::to_wstring(userId) +
                L", " + std::to_wstring(quests[i].questId) +
                L", " + std::to_wstring(quests[i].progress));
        }
    }

    for (const std::wstring& query : querys) {
        retcode = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
        if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
            LOG_ERR("[DBManager] SQLexecDirect failed");
            HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, retcode);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return false;
        }
    }

    LOG_INF("[DBManager] User[%d] UpdateUserInfo Success", userId);
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

void DBManager::HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
    SQLSMALLINT iRec = 0;
    SQLINTEGER iError;
    WCHAR wszMessage[1000];
    WCHAR wszState[SQL_SQLSTATE_SIZE + 1];
    if (RetCode == SQL_INVALID_HANDLE) {
        fwprintf(stderr, L"Invalid handle!\n");
        return;
    }
    while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
        (SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
        // Hide data truncated..
        if (wcsncmp(wszState, L"01004", 5)) {
            fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
        }
    }
}

void DBManager::EnsureThreadConnection(const std::wstring& database)
{
    if (_hDbc != SQL_NULL_HDBC) {
        return;
    }

    SQLRETURN retcode;
    
    retcode = SQLAllocHandle(SQL_HANDLE_DBC, _hEnv, &_hDbc);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("SQLAllocHandle failed");
        return;
    }

    retcode = SQLConnect(_hDbc, (SQLWCHAR*)database.c_str(), SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
    if ((retcode != SQL_SUCCESS) and (retcode != SQL_SUCCESS_WITH_INFO)) {
        LOG_ERR("[DBManager] SQLConnect Failed");
        return;
    }
}

//void DBManager::ThreadLoop()
//{
//    while (_running.load()) {
//       
//    }
//}

