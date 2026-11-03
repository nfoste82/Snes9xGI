#ifndef _REMASTER_DUNGEON_GRAPH_MEMBERSHIP_H_
#define _REMASTER_DUNGEON_GRAPH_MEMBERSHIP_H_
#include <cstdint>
#include <vector>
#ifdef ALTTP_TEST_GRAPH_WORK
inline uint64_t &S9xRemasterDungeonMembershipWork(){static uint64_t n=0;return n;}
#endif
// Parent is freshly initialized to -1 for this traversal; only newly queued
// children receive a parent. The root itself intentionally keeps parent -1.
inline bool S9xRemasterDungeonTraversalContains(const std::vector<int> &queue,
	const std::vector<int> &parent,int root,int node)
{
	(void)queue;
#ifdef ALTTP_TEST_GRAPH_WORK
	S9xRemasterDungeonMembershipWork()++;
#endif
	return node==root || (node>=0 && size_t(node)<parent.size() && parent[node]>=0);
}
#endif
