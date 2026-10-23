#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=1;c.linkX=80;c.linkY=80;
 c.placements=tiles->data();c.collisionAttributes=attr->data();c.placementBackgroundIndex=0;
 // All lower support is hidden by neutral art: no floor-role seed. A full
 // legal collision body must still discover the support, including relief.
 for(int y=10;y<20;y++)for(int x=10;x<14;x++){
  int i=y*64+x;(*attr)[4096+i]=0;auto &t=(*tiles)[i];t.hasHeight=true;t.distinctLowerUsage=true;
 }
 auto m=std::make_unique<RemasterDungeonHeightMap>();assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->requiredPlaneCells[1]==40 && m->unknownPlaneCells[1]==0);
 assert(m->planeOffsets[1][18*64+11]==0 && m->offsets[18*64+11]==0);
 // Pit semantics split support. Never fill the hidden floor beneath it.
 for(int x=10;x<14;x++)(*attr)[4096+15*64+x]=0x20;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->planeOffsets[1][15*64+11]==255 && m->offsets[15*64+11]==255);
 // An obstacle's authored relief is local, not a wall/floor seam.
 auto &ob=(*tiles)[18*64+11];ob.surface=Surface::Obstacle;ob.height.fill(5);
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(ob.height[0]==5);
 // Unobserved other plane stays unknown despite legal collision there.
 for(int y=10;y<20;y++)for(int x=10;x<14;x++)(*attr)[y*64+x]=0;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->planeOffsets[0][18*64+11]==255);
 std::cout<<"PASS hidden neutral lower support, pit exclusion, local relief, unobserved plane\n";
}
