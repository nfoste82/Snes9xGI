#include "../runtime/dungeon_visible_face_templates.h"
#include <cstdlib>
#include <iostream>
using namespace S9xRemasterVisibleFace;
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 std::array<uint8_t,256> rom={};
 for(int i=0;i<8;i++){rom[2*i]=uint8_t(11+i);rom[2*i+1]=8;}
 for(int axis=0;axis<2;axis++)for(int reverse=0;reverse<2;reverse++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  // A second ROM description reverses the depth order and toggles the
  // corresponding placement flip. Its final geometry is the SAME six cells,
  // not two physical owners. Exercise both axes and all placement mirrors.
  const int axisFlip=axis?1:2, wordFlip=axis?0x4000:0x8000;
  for(int t=0;t<2;t++)for(int d=0;d<4;d++) {
   int word=(rom[2*(t*4+3-d)]|rom[2*(t*4+3-d)+1]<<8)^wordFlip;
   rom[32+2*(t*4+d)]=uint8_t(word);rom[33+2*(t*4+d)]=uint8_t(word>>8);
  }
  std::array<uint16_t,4096> words={};int w=axis?4:2,h=axis?2:4;
  TemplateSection s{0,10,11,axis,reverse,flip,{}};
  s.equivalence.established=true;s.equivalence.family=1;
  s.equivalence.plane=0;s.equivalence.owner=7;
  s.equivalence.relief[0][0]=42;s.equivalence.contact[0]=1;
  for(int t=0;t<2;t++)for(int d=0;d<4;d++){
   int x=axis?d:t,y=axis?t:d,tx=(flip&1)?w-1-x:x,ty=(flip&2)?h-1-y:y;
   words[(11+ty)*64+10+tx]=(rom[2*(t*4+d)]|rom[2*(t*4+d)+1]<<8) ^ ((flip&1?0x4000:0)|(flip&2?0x8000:0));
  }
  std::array<SectionClaim,4096> claims;collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  int count=0;for(const auto &c:claims)count+=c.section==0 && !c.ambiguous;
  need(count==6,"complete transformed section recognized on either BG");
  TemplateSection alias{32,10,11,axis,1-reverse,flip^axisFlip,{}};
  alias.equivalence=s.equivalence;
  collectSections(words.data(),rom.data(),rom.size(),{s,alias},claims);
  count=0;for(const auto &c:claims)count+=c.section>=0 && !c.ambiguous;
  need(count==6,"equivalent transformed descriptions merge without losing geometry");
  for(int field=0;field<7;field++) {
   auto other=alias;
   if(field==0)other.equivalence.owner++;
   if(field==1)other.equivalence.plane++;
   if(field==2)other.equivalence.family++;
   if(field==3)other.equivalence.relief[0][0]++;
   if(field==4)other.equivalence.contact[0]++;
   if(field==5)other.equivalence.dx++;
   if(field==6)other.equivalence.established=false;
   collectSections(words.data(),rom.data(),rom.size(),{s,other},claims);
   count=0;for(const auto &c:claims)count+=c.ambiguous;
   need(count==6,"distinct owner, plane, source, relief, contact, direction or missing proof remains ambiguous");
  }
  collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  int foot=-1,trim=-1;
  for(int y=11;y<11+h;y++)for(int x=10;x<10+w;x++){
   int i=y*64+x;if(claims[i].section==0)foot=i;else trim=i;
  }
  words[trim]^=1;collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  need(claims[foot].section==0,"overwritten trim does not change surviving face");
  words[foot]^=0x2000;collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  need(claims[foot].section==0,"priority-only write preserves face");
  words[foot]^=1;collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  for(const auto &c:claims)need(c.section==-1,"overwritten face word invalidates whole section");
  words[foot]^=1;auto competitor=s;competitor.equivalence.owner++;
  collectSections(words.data(),rom.data(),rom.size(),{s,competitor},claims);
  need(claims[foot].ambiguous && claims[foot].section==-1,"competing section owners remain ambiguous");
  s.source=250;collectSections(words.data(),rom.data(),rom.size(),{s},claims);
  need(claims[foot].section==-1,"out-of-range template rejected");
 }
 // Exercise the real two-BG candidate producer, not just caller-made specs.
 // Distinct maps ensure that unswapped BG indexing cannot pass. Both axes,
 // all flips and reversed physical layer assignments are covered.
 for(int bg=0;bg<2;bg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++) {
  std::array<uint8_t,0x400> realrom={};
  const int off=axis?0x2c8:0x2e8, aliasoff=axis?0x2d8:0x2f8;
  for(int t=0;t<2;t++)for(int d=0;d<4;d++) {
   int word=0x800+40*t+d;
   realrom[off+2*(t*4+d)]=word&255;realrom[off+2*(t*4+d)+1]=word>>8;
  }
  const int wordFlip=axis?0x4000:0x8000;
  for(int t=0;t<2;t++)for(int d=0;d<4;d++) {
   int word=(realrom[off+2*(t*4+3-d)]|realrom[off+2*(t*4+3-d)+1]<<8)^wordFlip;
   realrom[aliasoff+2*(t*4+d)]=word&255;realrom[aliasoff+2*(t*4+d)+1]=word>>8;
  }
  std::array<std::array<uint16_t,4096>,2> maps={};
  const int w=axis?4:2,h=axis?2:4;
  for(int t=0;t<2;t++)for(int d=0;d<4;d++) {
   int sx=axis?d:t,sy=axis?t:d;
   int x=flip&1?w-1-sx:sx,y=flip&2?h-1-sy:sy;
   maps[bg][(11+y)*64+10+x]=(realrom[off+2*(t*4+d)]|realrom[off+2*(t*4+d)+1]<<8)^
    ((flip&1?0x4000:0)|(flip&2?0x8000:0));
  }
  auto witness=[&](int layer,const TemplateSection &s,const std::array<int,6>&) {
   TemplateSection::Equivalence e;e.established=true;e.family=1;e.owner=123;e.plane=1;
   e.dx=s.axis ? (s.reverse?-1:1)*(s.flip&1?-1:1):0;
   e.dy=!s.axis ? (s.reverse?-1:1)*(s.flip&2?-1:1):0;
   e.relief[0][0]=9;e.contact[0]=3;
   if(layer!=bg)e.established=false;
   return e;
  };
  auto produced=collectRoomSections(maps[0].data(),maps[1].data(),realrom.data(),realrom.size(),witness);
  int accepted=0;for(auto c:produced[bg].claims)accepted+=c.section>=0 && !c.ambiguous;
  need(accepted==6,"complete final BG producer merges transformed equivalents");
  for(auto c:produced[1-bg].claims)need(c.section<0 && !c.ambiguous,"producer preserves distinct BG maps");
  int cell=-1;for(int i=0;i<4096;i++)if(produced[bg].claims[i].section>=0){cell=i;break;}
  maps[bg][cell]^=1;
  produced=collectRoomSections(maps[0].data(),maps[1].data(),realrom.data(),realrom.size(),witness);
  for(auto c:produced[bg].claims)need(c.section<0,"overwritten face rejects complete ROM section");
 }
 std::cout<<"PASS complete transformed sections; competing owners rejected\n";
}
