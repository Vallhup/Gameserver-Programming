#include "pch.h"
#include "Quest.h"

Quest::Quest(int questId, const std::string& title, const std::string& description, std::unique_ptr<QuestType> type, int rewardExp, int rewardItemId, int rewardItemCount, int nextQuestId)
	: _questId(questId), _title(title), _description(description), _type(std::move(type)),
	_rewardExp(rewardExp), _rewardItemId(rewardItemId), _rewardItemCount(rewardItemCount), _nextQuestId(nextQuestId)
{
}


void Quest::OnEvent(const std::shared_ptr<GameSession>& session, int param)
{
	_type->OnEvent(session, param);
}

bool Quest::IsComplete() const
{
	return _type->IsComplete();
}