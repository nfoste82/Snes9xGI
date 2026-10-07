// Completed-room performance regression: immutable BG placements may be reused
// across relation/entry solves, but never across changed constructed inputs.
#include "remaster.h"
#include <cassert>
#include <chrono>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(0xff);
 for(int p=0;p<2;p++)for(int y:{12,18})for(int yy=y;yy<y+3;yy++)for(int x=8;x<18;x++)(*attr)[p*4096+yy*64+x]=0;
 uint8_t zero[64]={},wall[64];std::fill_n(wall,64,1);
 RemasterTileContentId floorId={S9xRemasterHashTile(4,zero),1,4},wallId={S9xRemasterHashTile(4,wall),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;
 profile.assets[floorId].hasHeight=true;profile.assetGroups["dungeon_floor"].tileIds={floorId};
 profile.assets[wallId].hasHeight=true;profile.assetGroups["wall_faces"].tileIds={wallId};
 for(int y=0;y<8;y++) {(*gfx)[32+y*2]=255;for(int x=0;x<8;x++)profile.assets[wallId].height[y*8+x]=y;}
 // Only BG1 has the authored wall path. Its signed relation must reach BG2
 // without replacing BG2's own flat local profiles or placement provenance.
 for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*maps)[8192+(y*64+x)*2]=1;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x61;c.linkX=80;c.linkY=18*8;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 c.stairCount=1;c.stairs[0].x=10;c.stairs[0].y=14;c.stairs[0].highIsNorth=false;
 S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);
 auto &s=S9xRemasterState();uint16_t screen[64]={};
 auto frame=[&]{S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 frame();assert(s.runtimeDungeonHeightMap.planeOffsets[0][19*64+10]==24);
 assert(s.runtimeDungeonBg1HeightMap.offsets[16*64+10]==8);
 const auto expected=s.runtimeDungeonHeightMap.offsets,lowerExpected=s.runtimeDungeonBg1HeightMap.offsets;
 const auto epoch=s.runtimeDungeonHeightEpoch;
 c.linkX++;c.backgroundScrollX=200;frame();assert(s.runtimeDungeonHeightEpoch==epoch);
 (*gfx)[64000]=1;frame();assert(s.runtimeDungeonHeightEpoch==epoch); // unrelated VRAM
 // Priority-only map changes still invalidate the complete snapshot, but must
 // produce the exact same heights. Benchmark the real EndFrame assembly path.
 const auto start=std::chrono::steady_clock::now();
 for(int n=0;n<12;n++) {
  (*maps)[1]^=0x20;frame();assert(s.runtimeDungeonHeightEpoch==epoch+n+1);
  assert(s.runtimeDungeonHeightMap.offsets==expected);
  assert(s.runtimeDungeonBg1HeightMap.offsets==lowerExpected);
  assert(!s.runtimeDungeonBg1HeightMap.authoredSupportRelations.empty());
 }
 const auto end=std::chrono::steady_clock::now();
 c.linkPlane=1;frame();assert(s.runtimeDungeonHeightMap.requiredPlaneCells[1]>0);
 c.linkPlane=0;frame();assert(s.runtimeDungeonHeightMap.offsets==expected);
 profile.assets[wallId].height.fill(0);S9xRemasterSetProfile(profile);frame();
 assert(s.runtimeDungeonHeightMap.offsets!=expected); // never retain old profile relief
 std::cout<<"PASS separate BG placements, signed relations, unchanged/camera/unrelated VRAM reuse, map/plane/profile invalidation\n";
 std::cout<<"completed_room_cache_miss_ms="<<std::chrono::duration<double,std::milli>(end-start).count()/12<<'\n';
}
