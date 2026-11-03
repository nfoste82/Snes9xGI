#include "../runtime/dungeon_door_index.h"
#include <memory>
#include <iostream>
#include <stdexcept>
static void need(bool b){if(!b)throw std::runtime_error("mandatory-word origin enumeration");}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();uint32_t state=7;
 for(auto &b:*maps){state=state*1664525+1013904223;b=state>>24;}
 for(int repeated=0;repeated<2;repeated++) {
  if(repeated)maps->fill(0);
  auto index=std::make_unique<RemasterDungeonDoorWordIndex>(maps->data());
  for(int bg=0;bg<2;bg++)for(int w:{3,4})for(int ax:{0,w-1})for(int ay:{0,6-w})for(int cell=0;cell<4096;cell+=17) {
   const int h=7-w,at=bg*8192+2*cell;const uint16_t word=(*maps)[at]|(*maps)[at+1]<<8;
   need(RemasterDungeonDoorWordIndex::key(word)==RemasterDungeonDoorWordIndex::key(word^0x2000));
   const auto origins=index->origins(bg*8192,word,w,h,ax,ay);
   int previous=-1,matching=0;
   for(int n=0;n<origins.count();n++) {
    const int origin=origins[n];if(origin<0)continue;need(origin>previous);previous=origin;
    const int pos=bg*8192+2*(origin+ay*64+ax);
    if((((*maps)[pos]|(*maps)[pos+1]<<8)&0xdfff)==(word&0xdfff))matching++;
    else need(!origins.cells); // original full-grid fallback retains full predicate
   }
   int expected=0;for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++) {
    const int pos=bg*8192+2*((y+ay)*64+x+ax);
    expected+=(((*maps)[pos]|(*maps)[pos+1]<<8)&0xdfff)==(word&0xdfff);
   }
   need(matching==expected);if(repeated)need(!origins.cells);
  }
 }
 std::cout<<"PASS exact masked words, sorted origins, edges, both maps, flips and all-same-word bounded fallback; bytes="<<sizeof(RemasterDungeonDoorWordIndex)<<'\n';
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
