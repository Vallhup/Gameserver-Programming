#pragma once

struct ViewListDiff {
	std::vector<int> addViewList;
	std::vector<int> moveViewList;
	std::vector<int> removeViewList;
};

class ViewListHelper
{
public:
	static std::unordered_set<int> CollectViewList(const std::shared_ptr<GameObject> self, const std::shared_ptr<Service> service);
	static std::unordered_set<int> UpdateViewList(const std::shared_ptr<GameSession>& session, const std::unordered_set<int>& newViewList);
	static ViewListDiff CalcViewListDiff(const std::unordered_set<int>& oldList, const std::unordered_set<int>& newList);

	static ViewListDiff SyncViewList(const std::shared_ptr<GameSession>& session);

private:
	static bool CanSee(const std::shared_ptr<GameObject> self, const std::shared_ptr<GameObject> target);
};

