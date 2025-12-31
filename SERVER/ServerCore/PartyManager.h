#pragma once

class Party;
class GameSession;

class PartyManager
{
public:
	PartyManager() = default;

	void CreateParty(const std::shared_ptr<GameSession>& requester, const std::shared_ptr<GameSession>& responser);
	void DisbandParty(int partyId);

	std::shared_ptr<Party> GetParty(int id) const;

	bool HandleRequest(const std::shared_ptr<GameSession>& requester, const std::shared_ptr<GameSession>& responser);
	bool HandleResponse(const std::shared_ptr<GameSession>& responser, bool response);
	bool HandleLeave(const std::shared_ptr<GameSession>& session);

private:
	std::atomic<int> _nextPartyId{ 0 };
	std::unordered_map<int, std::atomic<std::shared_ptr<Party>>> _parties;
	mutable std::shared_mutex _mutex;
};