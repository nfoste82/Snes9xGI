#define ALTTP_TEST_GRAPH_WORK
#include <cstdint>
#include <algorithm>
#include "../runtime/dungeon_graph_membership.h"
#include <cstdio>
#include <cstdlib>
static void check(bool b){if(!b)std::abort();}
int main(){
	// Multiple directed components, cycles, duplicate edges, isolated roots and
	// nodes assigned in an earlier traversal. Preserve first-discovery parents.
	for(int root:{0,13,8191}) {
		std::vector<int> parent(8192,-1),queue{root};
		for(int n=0;n<8192;n++)if(n!=root && n%3==0){parent[n]=queue.back();queue.push_back(n);}
		for(int n=0;n<8192;n++)check(S9xRemasterDungeonTraversalContains(queue,parent,root,n)==
			(std::find(queue.begin(),queue.end(),n)!=queue.end()));
	}
	std::printf("membership work=%llu limit=24576\n",(unsigned long long)S9xRemasterDungeonMembershipWork());
	check(S9xRemasterDungeonMembershipWork()<=24576);
}
