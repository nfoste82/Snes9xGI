// Plane-only EndFrame boundary: preserve classification, never the plane solve.
#define ALTTP_TEST_CACHE_STAGES
#include "remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(255);
 for(int p=0;p<2;p++)for(int y=10;y<16+p;y++)for(int x=10;x<16;x++)(*attr)[p*4096+y*64+x]=0;
 uint8_t zero[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,zero),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].hasHeight=true;
 profile.assets[id].tileRole=RemasterTileRole::Floor;profile.assets[id].height.fill(-256);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=c.linkY=88;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 (*maps)[(12*64+12)*2+1]=0xc0; // transformed placement
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);
 auto &s=S9xRemasterState();uint16_t screen[64]={};
 auto run=[&]{S9xRemasterDungeonCacheStages()={};S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 run();need(S9xRemasterDungeonCacheStages().classifications==2,"initial classification");
 for(int n=0;n<8;n++) {
  c.linkPlane=1-c.linkPlane;auto epoch=s.runtimeDungeonHeightEpoch;run();
  need(s.runtimeDungeonHeightEpoch>epoch && S9xRemasterDungeonCacheStages().solves>=2,"plane solve skipped");
  need(!S9xRemasterDungeonCacheStages().classifications,"plane reclassified");
  need((*s.dungeonRetainedBg2)[0].height[0]==-256,"signed relief changed");
  auto inv=dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
  s.dungeonClassificationInputs->valid=false;s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();
  need(S9xRemasterDungeonCacheStages().classifications==2,"uncached oracle not exercised");
  need(inv==dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"full plane inventory differs");
 }
 // Entry datum/evidence changes affect solves, not authored classification.
 c.entrySupport.cell=c.linkPlane*4096+12*64+12;c.entrySupport.base=37;
 c.entrySupport.predecessorRoom=6;c.entrySupport.transitionEvidence=123;
 run();need(!S9xRemasterDungeonCacheStages().classifications && S9xRemasterDungeonCacheStages().solves>=2,"entry identity reclassified or skipped solve");
 c.entrySupport={};run();need(!S9xRemasterDungeonCacheStages().classifications,"entry removal reclassified");
 (*gfx)[64000]^=1;c.linkPlane=1-c.linkPlane;run();
 need(!S9xRemasterDungeonCacheStages().classifications,"unrelated VRAM reclassified");
 auto rejects=[&]{run();need(S9xRemasterDungeonCacheStages().classifications==2,"changed dependency retained");};
 (*maps)[1]^=0x40;rejects();(*maps)[1]^=0x20;rejects();(*maps)[2]=1;rejects();
 (*attr)[12*64+12]=0x70;rejects();(*tpl)[12875]^=1;rejects();(*gfx)[32]=1;rejects();
 c.bg1GraphicsBase=32;rejects();
 profile.assets[id].height.fill(256);S9xRemasterSetProfile(profile);rejects();
 // Movement scratch is reusable only for identical attributes and ORDERED
 // seeds. Geometry/entry changes still solve independently using that graph.
 auto movement=std::make_unique<RemasterDungeonMovementCache>();
 auto cached=std::make_unique<RemasterDungeonHeightMap>(),full=std::make_unique<RemasterDungeonHeightMap>();
 auto placed=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,profile,*placed);c.placements=placed->data();
 for(int phase=0;phase<64;phase++) {
  c.linkX=88+(phase%8);c.linkY=88+(phase/8);c.linkPlane=phase%2;
  if(phase==4)c.stairCount=1,c.stairs[0]={12,12,1,0,false,true};
  if(phase==8)(*attr)[13*64+13]=0x80; // directed doorway behavior
  if(phase==12)c.entrySupport.cell=12*64+12,c.entrySupport.base=-256,c.entrySupport.transitionEvidence=321;
  c.movementCache=movement.get();need(S9xRemasterBuildDungeonHeightMap(c,50,*cached),"movement cache solve");
  // Second invocation exercises the exact seed/attribute hit, including entry.
  need(S9xRemasterBuildDungeonHeightMap(c,50,*cached),"movement cache repeat");
  c.movementCache=nullptr;need(S9xRemasterBuildDungeonHeightMap(c,50,*full),"movement full solve");
  need(dungeonMapInventory(*cached)==dungeonMapInventory(*full),"movement/entry/plane/phase inventory differs");
 }
 std::cout<<"PASS alternating planes: exact full inventories, solve invalidation, signed relief, maps/flips/priority/overwrite/attributes/templates/referenced graphics/bases/profile rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
