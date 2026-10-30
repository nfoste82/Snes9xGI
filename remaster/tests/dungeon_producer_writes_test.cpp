// CPU write observer: equal final words are not equal producer ownership.
#include <iostream>
#include <vector>
#include <stdexcept>
#if __has_include("../runtime/dungeon_producer_writes.h")
#include "../runtime/dungeon_producer_writes.h"
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto ram=std::make_unique<std::array<uint8_t,131072>>();
 auto rom=std::make_unique<std::array<uint8_t,1048576>>();
 (*ram)[0x1b]=1;(*ram)[0x10]=7;(*ram)[0xa0]=9;
 auto word=[&](int a,int v){(*ram)[a]=v;(*ram)[a+1]=v>>8;};
 auto &trace=S9xRemasterDungeonWrites();trace.enable(true);
 for(int both=0;both<2;both++) {
  trace.clear();const int source=both?0x10e8:0x26f6,origin=20*64+12;
  for(int x=0;x<4;x++)for(int y=0;y<4;y++) {
   const int value=0x800+x*4+y;
   word(0xe,4-x);(*rom)[0x1b52+source+2*(x*4+y)]=value;(*rom)[0x1b53+source+2*(x*4+y)]=value>>8;
   for(int bg=both?0:1;bg<2;bg++) {
    const int address=(bg?0x2000:0x4000)+2*(origin+y*64+x);
    const uint32_t single[]={0x0197f7,0x0197fc,0x019801,0x019806};
    const uint32_t paired[]={0x019822,0x01982d,0x019838,0x019843};
    const uint32_t pc=both?paired[y]+(bg?4:0):single[y];
    (*rom)[(pc&0xffff)-(both?4:2)]=both?0x9f:0x97;
    if(both)(*rom)[(pc&0xffff)-1]=0x7e;
    trace.write(ram->data(),rom->data(),address,value,2,pc,both?2*x:source+8*x,both?source+8*x:0);
    word(address,value);
   }
  }
  const auto token=trace.cells[4096+origin].owner;
  check(token!=0,"no actual producer witness");
  for(int bg=both?0:1;bg<2;bg++)for(int x=0;x<4;x++)for(int y=0;y<4;y++) {
   const auto &p=trace.cells[bg*4096+origin+y*64+x];
   check(p.owner==token && p.source==source && p.part==x*4+y,"source/part/layer ownership lost");
  }
  // A foreign same-value write must revoke ownership, not pass an equal-word test.
  const int address=0x2000+2*origin;
  trace.write(ram->data(),rom->data(),address,0x800,2,0x019000,0,0);
  check(trace.cells[4096+origin].owner==0,"equal-word foreign producer retained");
  trace.write(ram->data(),rom->data(),address+1,8,1,0x019000,0,0);
  check(trace.cells[4096+origin].owner==0,"partial byte overwrite retained");
 }
 // Odd-address word writes touch two cells, even if both bytes are equal.
 trace.cells[4096].owner=1;trace.cells[4097].owner=1;
 trace.write(ram->data(),rom->data(),0x2001,0,2,0,0,0);
 check(!trace.cells[4096].owner && !trace.cells[4097].owner,"unaligned overwrite retained second cell");
 // Never carry a previous room's mutable writes across gameplay lifetimes.
 (*ram)[0x10]=9;trace.write(ram->data(),rom->data(),0x2000,0,2,0,0,0);
 check(trace.room==0xffff,"foreign module retained room producer");
 trace.enable(false);std::cout<<"PASS observed Nx4 source/part/BG, unique writer, same-word and byte overwrite revocation\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
#else
int main(){std::cerr<<"missing live producer write witness\n";return 1;}
#endif
