#include "pch.h"
#include "Party.h"

bool Party::AddMember(const std::shared_ptr<GameSession>& session)
{
	int id = session->GetUserID();

	// 1. 중복 검사
	{
		std::shared_lock lock{ _memberMutex };

		if (_members.contains(id)) {
			LOG_WRN("Party[%d] is duple", _partyId);
			return false;
		}
	}

	// 2. 인원 제한
	int oldCount = _memberCount.fetch_add(1);
	if (oldCount >= MAX_MEMBER) {
		_memberCount.fetch_sub(1);
		return false;
	}

	// 3. 등록
	{
		std::unique_lock lock{ _memberMutex };
		_members.insert(std::make_pair(id, session));
	}

	// 4. session의 Party 설정
	session->SetParty(shared_from_this());

	return true;
}

void Party::RemoveMember(int sessionId)
{
	_memberCount.fetch_sub(1);
	{
		std::unique_lock lock{ _memberMutex };
		_members.erase(sessionId);
	}
}

std::vector<MemberInfo> Party::GetMemberInfos() const
{
	std::vector<MemberInfo> memberInfos;
	memberInfos.reserve(_members.size());
	{
		std::shared_lock lock{ _memberMutex };

		for (auto& [id, member] : _members) {
			PartyMemberState state{ OFFLINE };
			if (auto mp = member.load().lock()) {
				if (mp->IsVisible()) {
					state = ((mp->GetHp() > 0) ? ONLINE_ALIVE : ONLINE_DEAD);
				}
			}

			memberInfos.emplace_back(id, state);
		}
	}

	return memberInfos;
}

void Party::Update()
{
	Broadcast(PacketFactory::BuildPartyUpdatePacket(*this));
}

void Party::Disband()
{
	Broadcast(PacketFactory::BuildPartyDisbandPacket(*this));
}

void Party::ShareExp(int totalExp)
{
	// 살아있는 멤버만 Exp Share
	std::vector<std::shared_ptr<GameSession>> aliveMembers;
	aliveMembers.reserve(_members.size());
	{
		std::shared_lock lock{ _memberMutex };

		for (auto& [id, member] : _members) {
			if (auto mp = member.load().lock()) {
				if (mp->IsAlive()) {
					aliveMembers.push_back(mp);
				}
			}
		}
	}

	if (aliveMembers.empty()) {
		return;
	}

	int shareExp = totalExp / static_cast<int>(aliveMembers.size());

	for (auto member : aliveMembers) {
		member->AddExp(shareExp);
	}

	for (auto& sp : aliveMembers) {
		sp->Send(PacketFactory::BuildStatChangePacket(*sp));
	}
}

void Party::Broadcast(const std::vector<char>& packet)
{
	std::shared_lock lock{ _memberMutex };

	for (auto& [id, member] : _members) {
		if (auto mp = member.load().lock()) {
			mp->Send(packet);
		}
	}
}
