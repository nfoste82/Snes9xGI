// Optional real-ROM regression. ROM/snapshot are local and never committed.
// The ownership witness here is deliberately synthetic: this proves equivalent
// description canonicalization and complete-map production, not publication.
#include "../runtime/dungeon_visible_face_templates.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace S9xRemasterVisibleFace;
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
static std::vector<uint8_t> read(const char *p){std::ifstream f(p,std::ios::binary);need(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv) {
 need(argc==3,"snapshot and ROM required");auto raw=read(argv[1]),rom=read(argv[2]);
 constexpr size_t offset=14+224*(30+256*6);
 need(raw.size()==offset+65536+512 && rom.size()==1048576,"invalid input sizes");
 auto word=[](const uint8_t *p){return uint16_t(p[0]|p[1]<<8);};
 std::array<std::array<uint16_t,4096>,2> maps={};
 for(int bg=0;bg<2;bg++) {
  int base=word(raw.data()+18+bg*10);
  for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
   int addr=(base+y/32*2048+x/32*1024+y%32*32+x%32)&32767;
   maps[bg][y*64+x]=word(raw.data()+offset+addr*2);
  }
 }
 auto witness=[](int bg,const TemplateSection &s,const std::array<int,6> &cells) {
  TemplateSection::Equivalence e;e.established=true;e.family=(s.source<0x300?1:2);
  // Same displayed footprint means same synthetic candidate owner. Actual
  // owners must be established independently by the eventual runtime adapter.
  e.owner=1+cells[0]+bg*4096;e.plane=1;
  int sign=(s.reverse?-1:1)*(s.axis?(s.flip&1?-1:1):(s.flip&2?-1:1));
  e.dx=s.axis?sign:0;e.dy=s.axis?0:sign;
  return e;
 };
 auto rooms=collectRoomSections(maps[0].data(),maps[1].data(),rom.data()+0x1b52,12876,witness);
 for(auto test:{std::array<int,3>{1,13,11},std::array<int,3>{0,15,11}}) {
  int bg=test[0],cell=test[2]*64+test[1];auto c=rooms[bg].claims[cell];
  need(c.section>=0 && !c.ambiguous,"actual mirrored ROM footprint not canonicalized");
  auto first=rooms[bg].sections[c.section];int aliases=0;
  for(const auto &s:rooms[bg].sections)if(s.x==first.x && s.y==first.y && s.axis==first.axis && s.source!=first.source)aliases++;
  need(aliases>0,"actual mirror description not found");
  std::cout<<"BG"<<bg+1<<" cell "<<test[1]<<','<<test[2]<<" canonical section "<<c.section<<" alias descriptions "<<aliases<<'\n';
 }
 for(int bg=0;bg<2;bg++) {
  int ok=0,amb=0;for(auto c:rooms[bg].claims){ok+=c.section>=0;amb+=c.ambiguous;}
  std::cout<<"BG"<<bg+1<<" sections="<<rooms[bg].sections.size()<<" accepted="<<ok<<" ambiguous="<<amb<<'\n';
 }
}
