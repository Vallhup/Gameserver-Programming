#include "pch.h"
#include "PartyManager.h"

void PartyManager::CreateParty(const std::shared_ptr<GameSession>& requester, const std::shared_ptr<GameSession>& responser)
{
    std::shared_ptr<Party> party;

    if (auto exist = requester->GetParty()) {
        party = exist;
        party->AddMember(responser);
    }

    else {
        int newId = _nextPartyId++;

        party = std::make_shared<Party>(newId);
        {
            std::unique_lock lock{ _mutex };
            _parties.emplace(newId, party);
        }

        party->AddMember(requester);
        party->AddMember(responser);
    }

    party->Update();
}

void PartyManager::DisbandParty(int partyId)
{
    std::unique_lock lock{ _mutex };

    auto it = _parties.find(partyId);
    if (it == _parties.end()) {
        return;
    }

    _parties.erase(it);
}

std::shared_ptr<Party> PartyManager::GetParty(int id) const
{
    std::shared_lock lock{ _mutex };

    auto it = _parties.find(id);
    if (it == _parties.end()) {
        return nullptr;
    }

    return it->second.load();;
}

bool PartyManager::HandleRequest(const std::shared_ptr<GameSession>& requester, const std::shared_ptr<GameSession>& responser)
{
    // 0. 요청자와 응답자가 동일한 경우 처리하지 않음
    if (requester->GetUserID() == responser->GetUserID()) {
        LOG_WRN("Player %d attempted to invite themselves to a party", requester->GetUserID());
        auto failPacket = PacketFactory::BuildPartyResultPacket(responser->GetUserID(), false);
        requester->Send(failPacket);
        return false;
    }

    // 1. 이미 파티가 있는지 확인
    if (auto exist = responser->GetParty()) {
        LOG_INF("Player %d already in party %d", responser->GetUserID(), exist->GetId());

        // 이미 다른 파티가 있으면 실패
        auto failPacket = PacketFactory::BuildPartyResultPacket(responser->GetUserID(), false);
        requester->Send(failPacket);

        return false;
    }

    // 2. 이미 다른 초대가 pending 중인지 확인
    if (auto exist = responser->GetPendingPartyRequester()) {
        LOG_INF("Player %d already has a pending invite from %d", responser->GetUserID(), exist->GetUserID());

        // 이미 다른 초대가 있으면 실패
        auto failPacket = PacketFactory::BuildPartyResultPacket(responser->GetUserID(), false);
        requester->Send(failPacket);

        return false;
    }

    // 3. responser Pending Party Requester Setting
    responser->SetPendingPartyRequester(requester);

    // 4. responser Session에게 초대 알림 Send
    auto packetForTarget = PacketFactory::BuildPartyRequestPacket(requester->GetUserID());
    responser->Send(packetForTarget);

    return true;
}

bool PartyManager::HandleResponse(const std::shared_ptr<GameSession>& responser, bool response)
{
    // 1. Pending Party Requester 파싱
    auto requester = responser->ConsumePendingPartyRequester();
    if (nullptr == requester) {
        LOG_WRN("Party Requester is Null");
        return false;
    }

    // 2. 이미 파티에 속해 있으면 거절
    if (nullptr != responser->GetParty()) {
        LOG_INF("Player %d tried to accept invite but is already in party", responser->GetUserID());

        requester->Send(PacketFactory::BuildPartyResultPacket(responser->GetUserID(), false));
        return true;
    }

    // 3. requester가 속해 있는 파티가 정원이 찼으면 자동 거절
    if (auto exist = requester->GetParty()) {
        if (exist->GetMemberInfos().size() >= Party::MAX_MEMBER) {
            LOG_INF("Party[%d] is full, cannot add player %d", exist->GetId(), responser->GetUserID());

            requester->Send(PacketFactory::BuildPartyResultPacket(responser->GetUserID(), false));
            return true;
        }
    }

    // 4. 실제로 수락 or 거절 하였을 경우 Send
    auto packetForTarget = PacketFactory::BuildPartyResultPacket(responser->GetUserID(), response);
    requester->Send(packetForTarget);

    if (not response) {
        return true;
    }

    // 5. 파티 생성
    CreateParty(requester, responser);

    return true;
}

bool PartyManager::HandleLeave(const std::shared_ptr<GameSession>& session)
{
    auto party = session->GetParty();
    if (nullptr == party) {
        LOG_WRN("Leave Party is Null");
        return false;
    }

    // 1. 파티에서 제거
    party->RemoveMember(session->GetUserID());
    session->SetParty(nullptr);

    // 2. 파티 해산 or 파티 갱신
    if (party->Empty()) {
        party->Disband();
        DisbandParty(party->GetId());
    }

    else {
        party->Update();
    }

    return true;
}
