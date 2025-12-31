#include "pch.h"
#include "QuestType.h"

KillMonsterType::KillMonsterType(int npcId, int monsterId, int count) : _monsterId(monsterId), _targetCount(count), _currentCount(0)
{
	_npcId = npcId;
}

TalkToNpcType::TalkToNpcType(int npcId) : _talked(false)
{
	_npcId = npcId;
}

UseItemType::UseItemType(int npcId, int itemId) : _targetItemId(itemId), _used(false)
{
	_npcId = npcId;
}

std::unique_ptr<QuestType> KillMonsterType::Clone() const
{
	return std::make_unique<KillMonsterType>(*this);
}

std::unique_ptr<QuestType> TalkToNpcType::Clone() const
{
	return std::make_unique<TalkToNpcType>(*this);
}

std::unique_ptr<QuestType> UseItemType::Clone() const
{
	return std::make_unique<UseItemType>(*this);
}

void KillMonsterType::OnEvent(const std::shared_ptr<GameSession>& session)
{
	++_currentCount;
}

void TalkToNpcType::OnEvent(const std::shared_ptr<GameSession>& session)
{
	_talked = true;
}

void UseItemType::OnEvent(const std::shared_ptr<GameSession>& session)
{
	_used = true;
}

void KillMonsterType::OnEvent(const std::shared_ptr<GameSession>& session, int monsterId)
{
	if ((monsterId != _monsterId) or (_currentCount >= _targetCount)) {
		return;
	}

	_currentCount++;
}

void TalkToNpcType::OnEvent(const std::shared_ptr<GameSession>& session, int npcId)
{
	if (npcId == _npcId) {
		_talked = true;
	}
}

void UseItemType::OnEvent(const std::shared_ptr<GameSession>& session, int itemId)
{
	if (itemId == _targetItemId) {
		_used = true;
	}
}

bool KillMonsterType::IsComplete() const
{
	return _currentCount >= _targetCount;
}

bool TalkToNpcType::IsComplete() const
{
	return _talked;
}

bool UseItemType::IsComplete() const
{
	return _used;
}

std::string KillMonsterType::GetProgressText() const
{
	return std::to_string(_currentCount) + " / " + std::to_string(_targetCount);
}

std::string TalkToNpcType::GetProgressText() const
{
	return _talked ? "Talked to NPC" : "Not yet Talked";
}

std::string UseItemType::GetProgressText() const
{
	return _used ? "Used complete" : "Use the item";
}

int KillMonsterType::GetProgress() const
{
	return _currentCount;
}

int TalkToNpcType::GetProgress() const
{
	return _talked;
}

int UseItemType::GetProgress() const
{
	return _used;
}

QuestEventType KillMonsterType::GetEventType() const
{
	return QuestEventType::KillMonster;
}

QuestEventType TalkToNpcType::GetEventType() const
{
	return QuestEventType::TalkToNpc;
}

QuestEventType UseItemType::GetEventType() const
{
	return QuestEventType::UseItem;
}