#pragma once

struct APos {
	short x, y;
	bool operator==(const APos& rhs) const { return (x == rhs.x) and (y == rhs.y); }
};

namespace std {
	template<>
	struct hash<APos> {
		size_t operator()(const APos& p) const {
			return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
		}
	};
}

int Heuristic(const APos& a, const APos& b);

struct Node {
	APos pos;
	std::shared_ptr<Node> parent;

	// 지금까지 실제로 이동한 거리
	int gCost{ 0 };

	// 목표까지의 예측 거리(휴리스틱)
	int hCost{ 0 };

	// fCost = gCost + hCost
	int fCost() const { return gCost + hCost; }
};

using NodePtr = std::shared_ptr<Node>;

struct Compare {
	bool operator()(const NodePtr& a, const NodePtr& b) const
	{
		return a->fCost() > b->fCost();
	}
};

// node의 parent를 따라가면서 전체 경로를 path에 넣고 return
std::deque<APos> ReconstructPath(NodePtr node);

class Sector;
std::deque<APos> AStar(std::array<std::array<bool, MAP_SIZE>, MAP_SIZE>& map, APos start, APos goal);