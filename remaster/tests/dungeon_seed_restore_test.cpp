// EndFrame differential: ordered movement seeds are solve inputs, not artwork.
#define ALTTP_TEST_CACHE_STAGES
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(255);
 RemasterProfile profile;profile.upperFloorHeight=50;
 for(int phase=0;phase<2;phase++) {
  uint8_t pixels[64];std::fill_n(pixels,64,phase);
  RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[id];
  a.tileId=id;a.hasHeight=true;a.tileRole=RemasterTileRole::Floor;a.height.fill(0);
 }
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 c.replacementEvidence.available=true;c.replacementEvidence.room=c.roomIndex;
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);
 auto &s=S9xRemasterState();uint16_t screen[64]={};auto frame=std::make_unique<RemasterFrame>();
 auto art=[&](int phase){for(int y=0;y<8;y++)(*gfx)[y*2]=phase?255:0;};
 auto run=[&]{S9xRemasterDungeonCacheStages()={};S9xRemasterBeginFrame(64,8,8,8);
  S9xRemasterEndFrame(screen,8,8,8,frame.get(),&c);};
 auto inventory=[&]{std::ostringstream o;o<<dungeonMapInventory(s.runtimeDungeonHeightMap)<<dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
  for(int bg=0;bg<2;bg++)for(const auto &p:bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)dungeonPlacementInventory(o,p);
  return o.str();};
 auto parity=[&]{const auto before=inventory();auto prior=std::make_unique<RemasterDungeonHeightMap>(s.runtimeDungeonHeightMap);
  std::vector<uint8_t> warm,cold;need(S9xSerializeRemasterFrame(*frame,warm),"serialize warm");
  s.dungeonClassificationInputs->valid=false;s.dungeonSnapshotValid=false;s.runtimeDungeonHeightValid=false;s.runtimeDungeonHeightKey=0;
  s.dungeonArtworkStates.clear();s.dungeonClassifiedArtworkStates.clear();run();
  need(S9xRemasterDungeonCacheStages().classifications==2,"cold oracle did not classify");
  if(before!=inventory())for(int p=0;p<2;p++)for(int i=0;i<4096;i++) {
   const auto &cold=s.runtimeDungeonHeightMap;
   if(prior->occupiedFloorOwners[p][i]!=cold.occupiedFloorOwners[p][i] || prior->actorThresholdOwners[p][i]!=cold.actorThresholdOwners[p][i]) {
    std::cerr<<"owner sample plane="<<p<<" cell="<<i<<" occupied cached/cold="<<prior->occupiedFloorOwners[p][i]<<'/'<<cold.occupiedFloorOwners[p][i]
     <<" threshold cached/cold="<<prior->actorThresholdOwners[p][i]<<'/'<<cold.actorThresholdOwners[p][i]<<'\n';p=2;break;
   }
  }
  need(before==inventory(),"movement seed full map/placement mismatch");
  need(S9xSerializeRemasterFrame(*frame,cold)&&warm==cold,"movement seed packet mismatch");};
 for(bool unchanged:{false,true})
  for(int plane=0;plane<2;plane++)for(int threshold=0;threshold<3;threshold++) {
  attr->fill(1);c.linkPlane=plane;c.stairCount=threshold==2?1:0;
  if(c.stairCount){c.stairs[0].x=12;c.stairs[0].y=13;c.stairs[0].changesPlane=true;}
  for(int p=0;p<2;p++)for(int y=10;y<20;y++)for(int x=10;x<36;x++)
   if(x<20||x>=26)(*attr)[p*4096+y*64+x]=0;
  if(threshold)for(int p=0;p<2;p++)for(int y=13;y<15;y++)for(int x=10;x<20;x++)(*attr)[p*4096+y*64+x]=0x80;
  c.linkX=11*8;c.linkY=10*8;art(0);run();
  c.linkX=27*8;c.linkY=10*8;if(!unchanged)art(1);run();
  if(!unchanged){art(0);run();}
   need(S9xRemasterDungeonCacheStages().solves>0,"disconnected seed incorrectly certified");
   std::cout<<"case plane="<<plane<<" thresholds="<<threshold<<" unchanged="<<unchanged<<" solves="<<S9xRemasterDungeonCacheStages().solves<<" hits="<<s.dungeonArtworkStateHits<<std::endl;
  parity();
  // Same ordered seed, irrelevant graphics and a recurring art phase stay hot.
  run();need(!S9xRemasterDungeonCacheStages().solves,"static seed rebuilt");
  (*gfx)[64000]^=1;run();need(!S9xRemasterDungeonCacheStages().solves,"irrelevant graphics rebuilt");
  art(1);run();art(0);const auto hits=s.dungeonArtworkStateHits;run();
  need(!S9xRemasterDungeonCacheStages().solves&&!S9xRemasterDungeonCacheStages().classifications&&s.dungeonArtworkStateHits==hits+1,"same-seed resident return missed");parity();
   // Ordinary same-component motion must not re-solve after warming. Directed
   // thresholds retain the independent cold oracle, not a raw component shortcut.
   for(int x:{27*8+1,28*8}){c.linkX=x;run();
    if(!threshold)need(!S9xRemasterDungeonCacheStages().solves && !S9xRemasterDungeonCacheStages().classifications,"ordinary motion rebuilt");
    parity();}
   if(!threshold) {
    c.linkX=28*8;art(0);run();art(1);run();c.linkX++;
    const auto hits=s.dungeonArtworkStateHits;art(0);run();
    need(!S9xRemasterDungeonCacheStages().solves && !S9xRemasterDungeonCacheStages().classifications && s.dungeonArtworkStateHits==hits+1,"certified moving resident return missed");parity();
   }
  }
  // Multiple directed threshold claim bands, crossed orientation, both planes.
  // The certificate must reject states outside the first SCC, but may admit
  // legal members even when directed edges elsewhere in the map exist.
  for(int plane=0;plane<2;plane++)for(int axis=0;axis<2;axis++) {
   attr->fill(1);c.linkPlane=plane;c.stairCount=1;
   c.stairs[0].x=12;c.stairs[0].y=13;c.stairs[0].changesPlane=true;
   for(int p=0;p<2;p++)for(int y=10;y<25;y++)for(int x=10;x<25;x++)
    (*attr)[p*4096+y*64+x]=0;
   for(int p=0;p<2;p++)for(int y=10;y<25;y++)for(int x=10;x<25;x++)
    if(axis?(x==15 || x==20):(y==15 || y==20))(*attr)[p*4096+y*64+x]=axis?0x81:0x80;
   c.linkX=88;c.linkY=80;art(0);run();
   for(int k=0;k<9;k++) {
    c.linkX=88+(axis?k*8:0);c.linkY=80+(axis?0:k*8);
    run();parity();
   }
  }
  std::cout<<"PASS ordered seed closure / disconnected components / directed thresholds / stairs / both planes\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
