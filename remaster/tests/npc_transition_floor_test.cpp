#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

int main() {
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 std::vector<uint8_t> attr(8192,1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.placements=tiles->data();c.collisionAttributes=attr.data();c.linkX=160;c.linkY=152;
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++) {
  *tiles={};attr.assign(8192,1);c.placementBackgroundIndex=bg;
  for(int y:{20,26})for(int yy=y;yy<y+3;yy++)for(int x=20;x<24;x++) {
   auto &t=tiles->at(yy*64+x);t.surface=Surface::Floor;t.hasHeight=true;attr[yy*64+x]=0;
  }
  for(int y=23;y<26;y++)for(int x=20;x<24;x++) {
   auto &t=tiles->at(y*64+x);t.surface=Surface::Wall;t.hasHeight=true;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
  }
  // The flat transition strip participates in semantic seams, not movement.
  const int cell=20*64+24;auto &t=tiles->at(cell);t.surface=Surface::Floor;t.hasHeight=true;
  t.hflip=flip&1;t.vflip=flip&2;attr[cell]=0x22;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  const auto &d=map->roomDiagnostics->cells[cell];
  assert(map->planeOffsets[0][cell]==24 && d.floorId==-1);
  assert(d.semanticFloorSupport && d.floorBase==24 && !d.heightConflict);
  assert(!map->roomDiagnostics->cells[4096+cell].planeObserved);
  const auto required=map->requiredPlaneCells;
  for(int reject=0;reject<5;reject++) {
   const auto save=t;
   if(reject==0)t.height[0]=-1; // signed local relief is not a flat datum
   if(reject==1)t.surface=Surface::Unknown;
   if(reject==2)t.authoredObstacle=true;
   if(reject==3)t.structuralFloorPlaneMask=2;
   if(reject==4)attr[cell]=1; // a solid blocker is NOT a transition floor
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(!map->roomDiagnostics->cells[cell].semanticFloorSupport);
   t=save;attr[cell]=0x22;
  }
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->requiredPlaneCells==required);
 }
 std::cout<<"PASS transition semantic floor support: both BGs, H/V, no movement inflation, signed relief/unknown/blocker/plane rejection\n";
}
