// Partial overwritten frames must not erase surviving explicit jamb support,
// nor lend whole-assembly ownership to the overwritten outer band.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto c=std::make_unique<C>();auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();auto m=std::make_unique<RemasterDungeonHeightMap>();
 c->verifiedAlttpRom=c->indoors=true;c->placements=tiles->data();c->collisionAttributes=attrs->data();
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++) {
  tiles->fill({});attrs->fill(1);
  auto cell=[&](int u,int v){int x=d<2?v:u,y=d<2?u:v;return (flip&2?63-y:y)*64+(flip&1?63-x:x);};
  const bool high=d==0||d==2;const int foot=cell(high?12:10,20),middle=cell(11,20);
  const int outside=high?13:9;
  for(int u=high?13:4;u<(high?19:10);u++)for(int v=18;v<26;v++) {
   auto &t=(*tiles)[cell(u,v)];t.surface=S::Floor;t.hasHeight=true;(*attrs)[cell(u,v)]=0;
  }
  c->linkX=cell(outside+(high?2:-2),21)%64*8;c->linkY=cell(outside+(high?2:-2),21)/64*8;
  for(int i:{foot,middle}) {auto &t=(*tiles)[i];t.surface=S::Door;t.hasHeight=true;t.height.fill(-3);
   t.hasDoorframeMask=true;t.doorframeMask=UINT64_MAX;t.doorwayColumnContact=foot+1;t.doorwayColumnSource=0x2716;t.doorwayColumnBand=i==foot?0:1;
   t.hflip=flip&1;t.vflip=flip&2;
  }
  auto &f=(*tiles)[foot];f.doorwaySupport=true;
  f.doorwaySupportDirection=d<2?(high?RemasterSupportDirection::South:RemasterSupportDirection::North):(high?RemasterSupportDirection::East:RemasterSupportDirection::West);
  const auto original=f;const auto key=S9xRemasterDungeonEntryStateKey(*c,50);
  f.doorwayColumnBand++;
  check(!f.sameGeometry(original) && S9xRemasterDungeonEntryStateKey(*c,50)!=key,"column cache identity omitted");f=original;
  check(S9xRemasterBuildDungeonHeightMap(*c,50,*m),"solve");check(m->doorframeAttachments.size()==2,"surviving column denominator");
  for(const auto &a:m->doorframeAttachments)check(a.base==(a.cell==foot?0:8)&&a.openingBase==0,"column bands and datum");
  check(!f.doorAssembly && !(*tiles)[middle].doorAssembly,"forged whole-frame ownership");
  auto saved=f.doorwaySupportDirection;f.doorwaySupportDirection=RemasterSupportDirection::Unknown;
  check(S9xRemasterBuildDungeonHeightMap(*c,50,*m)&&m->doorframeAttachments.empty(),"missing direction accepted");f.doorwaySupportDirection=saved;
  const int neighbor=cell(outside,20);(*tiles)[neighbor].stairSourceAmbiguous=true;
  check(S9xRemasterBuildDungeonHeightMap(*c,50,*m)&&m->doorframeAttachments.empty(),"inactive alternate support accepted");
 }
 std::cout<<"PASS surviving column all axes/flips; explicit plane support, bands, missing direction and alternate-source rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
