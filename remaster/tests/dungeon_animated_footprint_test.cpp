// Animated member identities must not revoke complete ROM footprint ownership.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto rom=std::make_unique<std::array<uint8_t,12876>>();rom->fill(255);
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterProfile p;p.upperFloorHeight=50;RemasterTileContentId ids[4];
 for(int n=0;n<4;n++) {
  uint8_t pixels[64];std::fill_n(pixels,64,n);
  for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=p.assets[ids[n]];a.hasHeight=true;
  a.tileRole=n==2?RemasterTileRole::Obstacle:RemasterTileRole::Unknown;
  a.height.fill(n?8:0);
 }
 p.assetGroups["dungeon_floor"].tileIds={ids[0]};
 // Bilateral ROM torch, cap animated in place; authored group names one phase.
 const int templateWords[]={1,2,0x4001,0x4002};
 for(int k=0;k<4;k++){(*rom)[0xeca+k*2]=templateWords[k];(*rom)[0xecb+k*2]=templateWords[k]>>8;}
 p.obstacleGroups={{"arbitrary-name",{{ids[1],0,0,0,false,false,false},{ids[2],0,1,0,false,false,true}}}};
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;
 c.constructedMaps=words->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();c.dungeonTemplateData=rom->data();
 c.stairCount=1;c.stairs[0]={20,20,0,0,true,false};
 auto build=[&](int flip,bool overwrite) {
  words->fill(0);attrs->fill(1);
  for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int y=24;y<36;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int oy:{14,28})for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
   int w=templateWords[(flip&1?1-xx:xx)*2+(flip&2?1-yy:yy)]^((flip&1?0x4000:0)|(flip&2?0x8000:0));
   w|=0x2000; // priority-only write is geometry-neutral
   int cell=(oy+yy)*64+10+xx;(*words)[cell*2]=w;(*words)[cell*2+1]=w>>8;(*attrs)[cell]=1;
  }
  if(overwrite)(*words)[(14*64+10)*2]=3;
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
 };
 for(int flip=0;flip<4;flip++)for(int phase=0;phase<3;phase++) {
  for(int y=0;y<8;y++){(*gfx)[32+y*2]=phase==1?0:255;(*gfx)[33+y*2]=phase==1?255:0;}
  // Phase1 uses the foot identity too: ambiguous authored slot matches cannot
  // erase the independent complete ROM ownership of this placement.
  build(flip,false);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  for(int oy:{14,28})for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
   int cell=(oy+yy)*64+10+xx;
   need((*tiles)[cell].obstacleAssembly,"complete animated ROM footprint");
   need(map->offsets[cell]==(oy==14?50:0),"animated base lost");
   need((*tiles)[cell].height==p.assets.at({(*tiles)[cell].tileHash,1,4}).height,"authored samples changed");
  }
  build(flip,true);need(!(*tiles)[14*64+10].obstacleAssembly,"partial overwrite retained");
 }
 // Independent same-level planes are ambiguous without a complete lower-floor
 // ownership proof; solid upper body + distinct lower floor makes only plane0
 // the obstacle owner, while both movement supports survive independently.
 for(int y=0;y<8;y++){(*gfx)[32+y*2]=255;(*gfx)[33+y*2]=255;}
 build(0,false);c.stairCount=2;c.stairs[1]={40,40,0,0,true,true};
 for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[4096+y*64+x]=0;
 for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)(*attrs)[4096+(14+yy)*64+10+xx]=1;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==255,"ambiguous planes accepted");
 for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
  (*attrs)[4096+(14+yy)*64+10+xx]=0;
  auto &t=(*tiles)[(14+yy)*64+10+xx];t.distinctLowerUsage=true;t.floorPlaneMask=2;
 }
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==50,"proven upper body ownership");
 (*tiles)[14*64+10].floorPlaneMask=0;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[14*64+10]==255,"partial lower-floor evidence accepted");
 // Duplicate template families are ambiguous, never first-match ownership.
 build(0,false);std::copy_n(rom->data()+0xeca,8,rom->data()+0xec2);
 S9xRemasterDungeonPlacements(c,p,*tiles);need(!(*tiles)[14*64+10].obstacleAssembly,"ambiguous family accepted");
 std::cout<<"PASS animated complete ROM footprints, authored subset, all flips, priority overwrite, independent50/0, partial/ambiguous rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
