#pragma once

class GameSession;
class QuestType;

enum QuestStatus : char {
	ACCEPTED,
	COMPLETED
};

class Quest {
public:
	Quest() = default;
	Quest(int questId, const std::string& title, const std::string& description,
		std::unique_ptr<QuestType> type, int rewardExp, int rewardItemId, int rewardItemCount, int nextQuestId);

public:
	void OnEvent(const std::shared_ptr<GameSession>& session, int param);
	bool IsComplete() const;

public:
	int GetId() const { return _questId; }
	const std::string& GetTitle() const { return _title; }
	const std::string& GetDescription() const { return _description; }
	QuestType* GetType() const { return _type.get(); }
	int GetRewardExp() const { return _rewardExp; }
	int GetRewardItemId() const { return _rewardItemId; }
	int GetRewardItemCount() const { return _rewardItemCount; }
	int GetNextQuestId() const { return _nextQuestId; }

private:
	int _questId;
	std::string _title;
	std::string _description;

	std::unique_ptr<QuestType> _type;

	int _rewardExp;
	int _rewardItemId;
	int _rewardItemCount;

	int _nextQuestId{ 0 };
};

