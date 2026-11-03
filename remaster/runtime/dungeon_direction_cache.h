#ifndef _REMASTER_DUNGEON_DIRECTION_CACHE_H_
#define _REMASTER_DUNGEON_DIRECTION_CACHE_H_
#include <array>
#include <cstdint>
// Owned by one solve: placement heights and flips cannot change during it.
struct RemasterDungeonDirectionCache {
	std::array<int8_t,4096*4> values;
	RemasterDungeonDirectionCache(){values.fill(-1);}
	template<class Height> bool get(int i,int dx,int dy,Height height) {
		auto &v=values[i*4+(dx? (dx>0?3:2):(dy>0?1:0))];
		if(v>=0)return v;
		int low=0,high=0;
		for(int k=0;k<8;k++) {
#ifdef ALTTP_TEST_GRAPH_WORK
			S9xRemasterDungeonMembershipWork()++;
#endif
			low+=height(i,dx?(dx>0?7:0):k,dy?(dy>0?7:0):k);
			high+=height(i,dx?(dx>0?0:7):k,dy?(dy>0?0:7):k);
		}
		v=low<high;return v;
	}
};
struct RemasterDungeonPredicateCache {
	std::array<int8_t,4096> values;
	RemasterDungeonPredicateCache(){values.fill(-1);}
	template<class Predicate> bool get(int i,Predicate predicate) {
		auto &v=values[i];if(v<0)v=predicate(i);return v;
	}
};
#endif
