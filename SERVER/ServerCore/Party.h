#pragma once

struct MemberInfo;

class Party : public std::enable_shared_from_this<Party>
{
public:
	Party(int partyId) : _partyId(partyId) {}

public:
	int GetId() const { return _partyId; }
	bool Empty() const { return _members.empty(); }

	std::vector<MemberInfo> GetMemberInfos() const;
	
public:
	static constexpr int MAX_MEMBER{ 6 };

	bool AddMember(const std::shared_ptr<GameSession>& session);
	void RemoveMember(int sessionId);

public:
	void Update();
	void Disband();
	void ShareExp(int totalExp);

public:
	void Broadcast(const std::vector<char>& packet);

private:
	int _partyId;

	std::atomic<int> _memberCount{ 0 };

	std::unordered_map<int, std::atomic<std::weak_ptr<GameSession>>> _members;
	mutable std::shared_mutex _memberMutex;
};

