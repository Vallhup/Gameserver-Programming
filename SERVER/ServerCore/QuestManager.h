#pragma once

#include "QuestType.h"

class Quest;
class Service;
class GameSession;

struct QuestData;

enum QuestSymbol : char {
	NONE,
	EXCLAMATION,
	QUESTION
};

struct ActiveQuest {
	int questId;
	QuestStatus status;
	std::unique_ptr<QuestType> condition;
};

class QuestManager {
public:
	QuestManager(const std::shared_ptr<Service>& service);

	void Init();

	void RegisterPlayer(int sessionId);
	void UnregisterPlayer(int sessionId);

	const Quest* GetQuest(int questId) const;
	const std::unordered_map<int, ActiveQuest>& GetPlayerQuests(int sessionId) const;

	const std::vector<std::string>& GetQuestScripts(int questId) const;

	bool SetUserQuests(const std::shared_ptr<GameSession>& session, const std::vector<QuestData>& quests);
	bool AcceptQuest(const std::shared_ptr<GameSession>& session, int questId);
	bool UpdateProgress(const std::shared_ptr<GameSession>& session, int questId, int param);
	bool CompleteQuest(const std::shared_ptr<GameSession>& session, int questId);

	void GrantReward(const std::shared_ptr<GameSession>& session, const Quest* quest);

	bool HandleEvent(const std::shared_ptr<GameSession>& session, QuestEventType eventType, int param);

	std::vector<QuestData> GetUserQuestData(int sessionId);
	QuestSymbol GetNpcQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId);

	bool HasAvailableQuest(int sessionId, int npcId, int& outQuestId);

private:
	void InitQuests();
	void InitScripts();

private:
	std::unordered_map<int, Quest> _quests;
	std::unordered_map<int, std::vector<std::string>> _scripts;

	// <sessionId, <questId, ActiveQuest>>
	std::unordered_map<int, std::unordered_map<int, ActiveQuest>> _playerQuests;
	mutable std::shared_mutex _mutex;

	std::weak_ptr<Service> _service;
};

