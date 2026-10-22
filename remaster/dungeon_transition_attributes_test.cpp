#include "dungeon_transition_attributes.h"
#include <iostream>
#include <memory>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
	auto ram=std::make_unique<std::array<uint8_t,0x20000>>();
	std::array<uint8_t,8192> result;
	auto word=[&](int at,int v){(*ram)[at]=v;(*ram)[at+1]=v>>8;};
	(*ram)[0xfe01]=0x10;
	for(int flip=0;flip<4;flip++)word(0x2000+flip*2,1|(flip<<14));
	word(0x49c,2);word(0x6ec,10*64+10);
	word(0x42c,2);word(0x540,(20*64+20)*2);
	word(0x1980,0);word(0x19a0,(5*64+5)*2);word(0x19c0,1);
	const auto before=*ram;
	need(S9xRemasterCompleteDungeonAttributes(ram->data(),result),"complete builder rejected");
	need(*ram==before,"emulated RAM changed");
	for(int flip=0;flip<4;flip++)need(result[flip]==(0x10|flip),"basic flip attributes");
	need(result[4096+10*64+10]==2 && result[13*64+10]==2,"table2 sides");
	need(result[11*64+11]==0x3e && result[4096+12*64+12]==0x3e,"both-plane staircase");
	need(result[20*64+20]==0x70 && result[21*64+21]==0x70,"indexed object collision");
	need(result[6*64+6]==0x80 && result[10*64+7]==0x80,"door attribute span");
	word(0x42c,65);need(!S9xRemasterCompleteDungeonAttributes(ram->data(),result),"odd/overflow counter accepted");
	word(0x42c,2);word(0x540,0x3ffe);need(!S9xRemasterCompleteDungeonAttributes(ram->data(),result),"out-of-bounds object accepted");
	std::cout<<"PASS independent complete attributes: flips, both stair planes, indexed objects, doors, immutable RAM, invalid inventories\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
