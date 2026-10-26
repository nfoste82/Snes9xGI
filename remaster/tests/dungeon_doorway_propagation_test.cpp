// Fixture-free exact-owned frame transport: masks are not member graphs.
#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try{
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto c=std::make_unique<C>();auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();auto m=std::make_unique<RemasterDungeonHeightMap>();
 c->verifiedAlttpRom=c->indoors=true;c->placements=tiles->data();c->collisionAttributes=attrs->data();
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++)for(int split=0;split<2;split++){
  tiles->fill({});attrs->fill(1);c->doorwaySupportRelations=nullptr;
  auto cell=[&](int u,int v){int x=d<2?v:u,y=d<2?u:v;return (flip&2?63-y:y)*64+(flip&1?63-x:x);};
  const int direction=d<2?(d^((flip&2)?1:0)):(d^((flip&1)?1:0));
  const int origin=std::min({cell(10,20),cell(12,23),cell(10,23),cell(12,20)});
  const bool highSide=d==0 || d==2;
  const int foot=highSide?12:10,outside=highSide?13:9;
  c->linkX=cell(outside+(highSide?2:-2),21)%64*8;c->linkY=cell(outside+(highSide?2:-2),21)/64*8;
  for(int u=highSide?13:4;u<(highSide?19:10);u++)for(int v=18;v<26;v++){
   auto &t=(*tiles)[cell(u,v)];t.surface=S::Floor;t.hasHeight=true;(*attrs)[cell(u,v)]=0;
  }
  // Two disconnected sides, non-mask cap, arbitrary signed intrinsic relief.
  for(int u=10;u<=12;u++)for(int v:{20,23}){
   auto &t=(*tiles)[cell(u,v)];t.surface=S::Door;t.hasHeight=true;t.height.fill(-11);
   t.doorAssembly=origin+1;t.doorDirection=direction;t.doorTemplateOffset=0x2716;t.splitDoorPart=split;
   t.hflip=flip&1;t.vflip=flip&2;
   if(u==foot && v==20){t.doorwaySupport=true;t.doorwaySupportDirection=d<2?(highSide?RemasterSupportDirection::South:RemasterSupportDirection::North):(highSide?RemasterSupportDirection::East:RemasterSupportDirection::West);}
   // Explicit empty region on the other foot is legitimate opening-only art.
   if(u==foot && v==23)t.hasDoorframeMask=true;
  }
  require(S9xRemasterBuildDungeonHeightMap(*c,50,*m),"solve");
   require(m->doorframeAttachments.size()==6,"disconnected nonmask member denominator");
   // Exact distinct authored floors on both maps preserve BOTH gameplay
   // datums, but only the basic-attribute writer's map owns visible art.
   // It must not depend on which plane the controllable actor occupies.
   if(!split){
    auto saved=*tiles;auto savedAttrs=*attrs;
    for(auto &t:*tiles)if(t.surface==S::Floor){t.floorPlaneMask=3;t.distinctLowerUsage=true;}
    for(int i=0;i<4096;i++)if((*attrs)[i]==0)(*attrs)[4096+i]=0;
    for(int actor=0;actor<2;actor++)for(int bg=0;bg<2;bg++){
     c->linkPlane=actor;c->placementBackgroundIndex=bg;
     require(S9xRemasterBuildDungeonHeightMap(*c,50,*m),"independent plane solve");
     for(const auto &a:m->doorframeAttachments)require(a.visible==(a.plane==(bg?0:1)),"hidden support conflated with visible frame");
    }
    c->linkPlane=0;c->placementBackgroundIndex=1;*tiles=saved;*attrs=savedAttrs;
    require(S9xRemasterBuildDungeonHeightMap(*c,50,*m),"restored solve");
   }
  for(const auto &a:m->doorframeAttachments){
   int u=d<2?((flip&2)?63-a.cell/64:a.cell/64):((flip&1)?63-a.cell%64:a.cell%64);
   require(a.base==8*(highSide?12-u:u-10) && a.openingBase==0,"assembly-relative rise and landing");
   require((*tiles)[a.cell].height[0]==-11,"intrinsic relief mutated");
  }
  auto relation=m->doorwaySupportRelations;
  // One split BG contains only the remote cap, but the other BG's directional
  // proof names this exact complete assembly. No same-coordinate base copying.
  if(split){
   auto saved=*tiles;for(auto &t:*tiles)if(t.surface==S::Door)t={};
   const int cap=cell(highSide?10:12,23);(*tiles)[cap]=saved[cap];c->doorwaySupportRelations=&relation;
   require(S9xRemasterBuildDungeonHeightMap(*c,50,*m),"split solve");require(m->doorframeAttachments.size()==1 && m->doorframeAttachments[0].base==16,"split disconnected cap datum");
   (*tiles)[cap].doorAssembly++;require(S9xRemasterBuildDungeonHeightMap(*c,50,*m) && m->doorframeAttachments.empty(),"competing split owner accepted");
   *tiles=saved;c->doorwaySupportRelations=nullptr;
  }
  const int cap=cell(highSide?10:12,23);auto saved=(*tiles)[cap];
  (*tiles)[cap].doorDirection=0xfe;require(S9xRemasterBuildDungeonHeightMap(*c,50,*m) && m->doorframeAttachments.size()==5,"ambiguous owner accepted");
  (*tiles)[cap]=saved;(*tiles)[cap].doorAssembly++;require(S9xRemasterBuildDungeonHeightMap(*c,50,*m) && m->doorframeAttachments.size()==5,"unrelated owner accepted");
  (*tiles)[cap]=saved;(*tiles)[cap].surface=S::ClosedDoor;require(S9xRemasterBuildDungeonHeightMap(*c,50,*m) && m->doorframeAttachments.size()==5,"actual closed Door accepted");
 }
 std::cout<<"PASS doorway band/member transport all axes/flips, disconnected nonmask caps, split BG, competing ownership and actual Door rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
