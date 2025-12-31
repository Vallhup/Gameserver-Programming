#include "pch.h"
#include "QuestManager.h"

QuestManager::QuestManager(const std::shared_ptr<Service>& service) : _service(service)
{
    Init();
}

void QuestManager::Init()
{
    InitQuests();
}

void QuestManager::RegisterPlayer(int sessionId)
{
    std::unique_lock lock{ _mutex };
    _playerQuests.try_emplace(sessionId);
}

void QuestManager::UnregisterPlayer(int sessionId)
{
    std::unique_lock lock{ _mutex };
    _playerQuests.erase(sessionId);
}

const Quest* QuestManager::GetQuest(int questId) const
{
    if (not _quests.contains(questId)) {
        LOG_ERR("QuestManager: quest[%d] not found", questId);
        return nullptr;
    }

    return &_quests.at(questId);
}

const std::unordered_map<int, ActiveQuest>& QuestManager::GetPlayerQuests(int sessionId) const
{
    std::shared_lock lock{ _mutex };
    return _playerQuests.at(sessionId);
}

const std::vector<std::string>& QuestManager::GetQuestScripts(int questId) const
{
    auto it = _scripts.find(questId);
    if (it == _scripts.end()) {
        return {};
    }

    else {
        return it->second;
    }
}

bool QuestManager::SetUserQuests(const std::shared_ptr<GameSession>& session, const std::vector<QuestData>& quests)
{
    for (const QuestData& questData : quests) {
        auto quest = GetQuest(questData.questId);
        if (nullptr == quest) {
            return false;
        }

        int sessionId = session->GetId();
        auto questType = quest->GetType()->Clone();
        if (questType->GetEventType() == KillMonster) {
            for (int i = 0; i < questData.progress; ++i) {
                questType->OnEvent(session);
            }
        }

        else if (questType->GetEventType() == UseItem) {
            if (questData.progress) {
                questType->OnEvent(session);
            }
        }

        else if (questType->GetEventType() == TalkToNpc) {
            if (questData.progress) {
                questType->OnEvent(session);
            }
        }

        std::string progressText = questType->GetProgressText();
        {
            std::unique_lock lock{ _mutex };
            auto it = _playerQuests.find(sessionId);
            if (it == _playerQuests.end()) {
                return false;
            }

            else {
                it->second.try_emplace(questData.questId, questData.questId, QuestStatus::ACCEPTED, std::move(questType));
            }
        }

        session->Send(PacketFactory::BuildQuestAcceptPacket(questData.questId,
            quest->GetTitle(), quest->GetDescription(),
            progressText));
    }

    return true;
}

bool QuestManager::AcceptQuest(const std::shared_ptr<GameSession>& session, int questId)
{
    auto quest = GetQuest(questId);
    if (nullptr == quest) {
        return false;
    }

    int sessionId = session->GetId();
    {
        std::unique_lock lock{ _mutex };

        // _playerQuests에 sessionId, questId 확인해서
        auto it = _playerQuests.find(sessionId);
        if (it == _playerQuests.end()) {
            LOG_WRN("Player[%d] is not INGAME", sessionId);
            return false;
        }

        // Quest 진행 중이면 return false (모든 Quest가 연속 퀘스트)
        if (not it->second.empty()) {
            return false;
        }

        // 아니면 Quest 진행 중으로 수정하고 return true
        else {
            it->second.try_emplace(questId, questId, QuestStatus::ACCEPTED, quest->GetType()->Clone());
        }
    }

    session->Send(PacketFactory::BuildQuestAcceptPacket(questId,
        quest->GetTitle(), quest->GetDescription(),
        quest->GetType()->GetProgressText()));

    int npcId = quest->GetType()->GetNpcId();
    char symbol = GetNpcQuestSymbol(session, npcId);
    session->Send(PacketFactory::BuildQuestSymbolUpdatePacket(npcId, symbol));

    return true;
}

bool QuestManager::UpdateProgress(const std::shared_ptr<GameSession>& session, int questId, int param)
{
    std::string progressText;
    bool isComplete{ false };
    {
        std::unique_lock lock{ _mutex };

        auto playerIt = _playerQuests.find(session->GetId());
        if (playerIt == _playerQuests.end()) {
            return false;
        }

        auto questIt = playerIt->second.find(questId);
        if (questIt == playerIt->second.end()) {
            return false;
        }

        ActiveQuest& aq = questIt->second;

        aq.condition->OnEvent(session, param);
        progressText = aq.condition->GetProgressText();
    }

    session->Send(PacketFactory::BuildQuestUpdatePacket(questId, progressText));

    return true;
}

bool QuestManager::CompleteQuest(const std::shared_ptr<GameSession>& session, int questId)
{
    {
        std::unique_lock lock{ _mutex };

        auto it = _playerQuests.find(session->GetId());
        if (it == _playerQuests.end()) {
            return false;
        }

        if (it->second.erase(questId) == 0) {
            return false;
        }
    }

    const Quest* quest = GetQuest(questId);
    GrantReward(session, quest);

    if (quest->GetNextQuestId() != 0) {
        AcceptQuest(session, quest->GetNextQuestId());
    }

    return true;
}

void QuestManager::GrantReward(const std::shared_ptr<GameSession>& session, const Quest* quest)
{
    const int rewardExp = quest->GetRewardExp();
    const int rewardItemId = quest->GetRewardItemId();
    const int rewardItemCount = quest->GetRewardItemCount();

    if (auto service = _service.lock()) {
        service->UserGetItem(session, rewardItemId, rewardItemCount);
    }

    session->AddExp(rewardExp);
    session->GetInventory()->AddItem(rewardItemId, rewardItemCount);

    session->Send(PacketFactory::BuildQuestCompletePacket(quest->GetId()));
    session->Send(PacketFactory::BuildStatChangePacket(*session));
    session->Send(PacketFactory::BuildAddItemPacket(rewardItemId, rewardItemCount));
}

bool QuestManager::HandleEvent(const std::shared_ptr<GameSession>& session, QuestEventType eventType, int param)
{
    const int sid = session->GetId();
    std::unordered_set<int> update;
    {
        std::shared_lock lock{ _mutex };

        auto playerIt = _playerQuests.find(sid);
        if (playerIt == _playerQuests.end()) {
            return false;
        }

        for (auto& [questId, activeQuest] : playerIt->second) {
            auto type = activeQuest.condition->GetEventType();
            if (type == eventType) {
                update.insert(questId);
            }

            else if (eventType == QuestEventType::TalkToNpc) {
                if (activeQuest.condition->IsComplete()) {
                    update.insert(questId);
                }
            }
        }
    }

    bool returnValue{ false };
    for (int questId : update) {
        if (eventType == QuestEventType::TalkToNpc) {
            if (CompleteQuest(session, questId)) {
                returnValue = true;
            }
        }

        else {
            if (UpdateProgress(session, questId, param)) {
                returnValue = true;
            }
        }

        const Quest* quest = GetQuest(questId);

        int npcId = quest->GetType()->GetNpcId();
        char symbol = GetNpcQuestSymbol(session, npcId);
        session->Send(PacketFactory::BuildQuestSymbolUpdatePacket(npcId, symbol));
    }

    return returnValue;
}

std::vector<QuestData> QuestManager::GetUserQuestData(int sessionId)
{
    std::vector<QuestData> quests;
    {
        std::shared_lock lock{ _mutex };

        auto it = _playerQuests.find(sessionId);
        if (it == _playerQuests.end()) {
            return quests;
        }

        else {
            size_t count = it->second.size();
            quests.reserve(count);

            for (auto& [questId, aq] : it->second) {
                quests.emplace_back(questId, aq.condition->GetProgress());
            }
        }
    }

    return quests;
}

QuestSymbol QuestManager::GetNpcQuestSymbol(const std::shared_ptr<GameSession>& session, int npcId)
{
    const int sid = session->GetId();
    bool hasOnQuest{ false };
    {
        std::shared_lock lock{ _mutex };
        
        auto it = _playerQuests.find(sid);
        if (it != _playerQuests.end()) {
            for (const auto& [questId, aq] : it->second) {
                if ((aq.condition->GetEventType() == QuestEventType::TalkToNpc) and (not aq.condition->IsComplete())) {
                    continue;
                }

                if (auto quest = GetQuest(questId)) {
                    if (aq.condition->GetNpcId() == npcId) {
                        if (aq.condition->IsComplete()) {
                            return QuestSymbol::QUESTION;
                        }

                        else {
                            hasOnQuest = true;
                        }
                    }
                }
            }
        }
    }

    if (hasOnQuest) {
        return QuestSymbol::NONE;
    }

    for (auto& [questId, quest] : _quests) {
        if (quest.GetType()->GetNpcId() != npcId) continue;
        if (_playerQuests[sid].contains(questId)) continue;

        return QuestSymbol::EXCLAMATION;
     }

    return QuestSymbol::NONE;
}

bool QuestManager::HasAvailableQuest(int sessionId, int npcId, int& outQuestId)
{
    std::shared_lock lock{ _mutex };

    for (const auto& [questId, quest] : _quests) {
        auto type = quest.GetType();
        if (type->GetEventType() != QuestEventType::TalkToNpc) continue;

        auto talkType = static_cast<TalkToNpcType*>(type);
        if ((nullptr == talkType) or (talkType->GetNpcId() != npcId)) continue;

        const auto& userQuests = _playerQuests[sessionId];
        if (userQuests.find(questId) == userQuests.end()) {
            outQuestId = questId;
            return true;
        }
    }

    return false;
}

void QuestManager::InitQuests()
{
    _quests.try_emplace(1, 1, "PFMonster Hunt", "Monster defeated: ",
        std::make_unique<KillMonsterType>(MAX_USER + MAX_NPC, 1, 3), 100, 0, 0, 2);

    _quests.try_emplace(2, 2, "PRMonster Hunt", "Monster defeated: ",
        std::make_unique<KillMonsterType>(MAX_USER + MAX_NPC, 2, 5), 200, 0, 0, 3);

    _quests.try_emplace(3, 3, "AFMonster Hunt", "Monster defeated: ",
        std::make_unique<KillMonsterType>(MAX_USER + MAX_NPC, 3, 7), 300, 0, 0, 4);

    _quests.try_emplace(4, 4, "ARMonster Hunt", "Monster defeated: ",
        std::make_unique<KillMonsterType>(MAX_USER + MAX_NPC, 4, 10), 400, HpPotion, 10, 5);

    _quests.try_emplace(5, 5, "Use item", "Try using potions",
        std::make_unique<UseItemType>(MAX_USER + MAX_NPC, HpPotion), 500, 0, 0, 0);
}