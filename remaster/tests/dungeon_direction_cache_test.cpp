#define ALTTP_TEST_GRAPH_WORK
#include <algorithm>
#include "../runtime/dungeon_graph_membership.h"
#include "../runtime/dungeon_direction_cache.h"
#include <memory>
#include <cstdlib>
#include <cstdio>
int main(){
	auto cache=std::make_unique<RemasterDungeonDirectionCache>();
	for(int flip=0;flip<4;flip++)for(int shape=0;shape<8;shape++)for(int sign:{-1,1})for(int axis=0;axis<2;axis++) {
		const int i=flip*32+shape*4+(sign>0?2:0)+axis,dx=axis?0:sign,dy=axis?sign:0;
		auto height=[&](int,int x,int y){x=flip&1?7-x:x;y=flip&2?7-y:y;
			switch(shape){case 0:return 0;case 1:return x;case 2:return -y;case 3:return x+y;case 4:return std::max(x,y);case 5:return std::min(x,y);case 6:return (x+y)/2;default:return (x*17+y*31)%13-6;}};
		int low=0,high=0;for(int k=0;k<8;k++){low+=height(i,dx?(dx>0?7:0):k,dy?(dy>0?7:0):k);high+=height(i,dx?(dx>0?0:7):k,dy?(dy>0?0:7):k);}
		for(int repeat=0;repeat<100;repeat++)if(cache->get(i,dx,dy,height)!=(low<high))std::abort();
	}
	std::printf("direction samples=%llu limit=1024\n",(unsigned long long)S9xRemasterDungeonMembershipWork());
	if(S9xRemasterDungeonMembershipWork()>1024)std::abort();
	auto predicates=std::make_unique<RemasterDungeonPredicateCache>();int evaluations=0;
	for(int pass=0;pass<3;pass++)for(int i=0;i<4096;i++)
		if(predicates->get(i,[&](int n){evaluations++;return n%3==0;})!=(i%3==0))std::abort();
	std::printf("predicate evaluations=%d limit=4096\n",evaluations);
	if(evaluations>4096)std::abort();
	// No context/profile lifetime survives a solve: changed height at the SAME
	// index is evaluated afresh by the next invocation's scratch, including zero.
	for(int generation=0;generation<3;generation++) {
		auto fresh=std::make_unique<RemasterDungeonDirectionCache>();
		if(fresh->get(0,1,0,[&](int,int x,int){return (generation-1)*x;})!=(generation==0))std::abort();
		auto next=std::make_unique<RemasterDungeonPredicateCache>();int calls=0;
		for(int n=0;n<3;n++)if(next->get(0,[&](int){calls++;return generation==1;})!=(generation==1))std::abort();
		if(calls!=1)std::abort();
	}
}
