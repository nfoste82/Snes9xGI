#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

// A table1/1e assembly is high NORTH, even when entered on the high floor.
// Exercise the real detection -> independent support -> cross-BG -> application
// path. No profile reauthoring and no injected plane-wide height convention.
int main() {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 for(int origin:{10,34})for(int rise:{24,50})for(bool flip:{false,true}) {
  attr->fill(1);tiles->fill({});
  C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=23;c.previousRoomIndex=22;
  c.placementBackgroundIndex=0;c.linkPlane=0;c.linkX=(origin+1)*8;c.linkY=12*8;
  c.collisionAttributes=attr->data();c.placements=tiles->data();
  for(int y=11;y<22;y++)for(int x=origin-2;x<origin+8;x++) {
   auto &t=(*tiles)[y*64+x];t.hasHeight=true;
   if(y<15 || y>=18){t.surface=S::Floor;(*attr)[y*64+x]=(*attr)[4096+y*64+x]=0;}
   else {
    t.surface=S::Wall;t.wallTemplateOffset=0xc26;t.vflip=flip;
    for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++) {
     const int displayed=flip?7-yy:yy;
     t.height[yy*8+xx]=7-displayed;
    }
   }
  }
  // No authored wall relation: retain the explicit configured signed50,
  // rather than manufacturing or rescaling a local24 stair/wall profile.
  if(rise==50)for(int y=15;y<18;y++)for(int x=origin-2;x<origin+8;x++)(*tiles)[y*64+x]={};
  for(int y=15;y<18;y++)for(int x=origin;x<origin+4;x++)(*tiles)[y*64+x]={};
  // Recognition footprint kept separate from the wall seam corridor.
  for(int p=0;p<2;p++)for(int y=16;y<=17;y++) {
   (*attr)[p*4096+y*64+origin]=(*attr)[p*4096+y*64+origin+3]=1;
   (*attr)[p*4096+y*64+origin+1]=(*attr)[p*4096+y*64+origin+2]=0x1e;
  }
  c.stairCount=1;c.stairs[0].x=origin;c.stairs[0].y=15;
  // Reproduce old captured/table metadata, repaired only by complete signature.
  c.stairs[0].highIsNorth=false;c.stairs[0].changesPlane=true;
  S9xRemasterDungeonStairsFromAttributes(c);
  assert(c.stairCount==1 && c.stairs[0].highIsNorth);
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  const int high=13*64+origin+1,low=19*64+origin+1;
  const int solvedRise=map->planeOffsets[0][high]-map->planeOffsets[1][low];
  assert(solvedRise==rise && map->configuredStairFallbacks==(rise==50));
  auto relations=map->authoredSupportRelations;
  assert(rise==50 ? relations.empty() : relations.size()==1 && relations[0].rise==rise);
  for(int y=15;y<18;y++)for(int x=origin-2;x<origin+8;x++)(*tiles)[y*64+x]={};
  c.placementBackgroundIndex=1;c.supportRelations=&relations;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  assert(map->planeOffsets[0][high]==rise && map->planeOffsets[1][low]==0);
  // Explicit completed upper entries24/50 shift the signed graph, NOT all
  // planes. A50 source entering a24 assembly correctly solves its low floor26.
  for(int sourceBase:{24,50}) {
  c.entrySupport={high,sourceBase,22,123,456,S9xRemasterDungeonEntryStateKey(c,50)};
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->entrySupportConsumed);
  assert(map->planeOffsets[0][high]==sourceBase && map->planeOffsets[1][low]==sourceBase-rise);
  auto frame=std::make_unique<RemasterFrame>();frame->width=frame->height=8;frame->mainPixels.resize(64);
  RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
  frame->tileInstances.push_back(instance);for(auto &px:frame->mainPixels)px.instanceId=1;
  c.backgroundScrollX=(origin+1)*8;c.backgroundScrollY=13*8;
  S9xRemasterApplyDungeonHeightMap(*frame,c,*map);
  assert(frame->tileInstances[0].heightOffset==sourceBase);
  }
  (*attr)[16*64+origin+1]=0;const auto before=c.stairs[0].highIsNorth;
  S9xRemasterDungeonStairsFromAttributes(c);assert(c.stairs[0].highIsNorth==before);
  c.entrySupport.destinationState++;S9xRemasterBuildDungeonHeightMap(c,50,*map);
  assert(!map->entrySupportConsumed);
 }
 std::cout<<"PASS north table1 high entry: origins10/34, authored24/configured50, flips, signed cross-BG, entry shift, runtime application, stale pins\n";
}
