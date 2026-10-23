// Blocked floor-pattern caps inherit direct authored seams, never an isolated datum.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=72;c.placements=tiles->data();c.collisionAttributes=attrs->data();
 for(int flip=0;flip<4;flip++){
  *tiles={};attrs->fill(1);
  for(int y=10;y<13;y++)for(int x=10;x<14;x++){
   auto &t=(*tiles)[y*64+x];t.surface=Surface::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;
  }
  // Authored 16..23 relief joins support0 with base-16, cap8. Reflect both.
  const int wallY=(flip&2)?13:9,capY=(flip&2)?14:8;
  for(int x=10;x<14;x++){
   auto &w=(*tiles)[wallY*64+x];w.surface=Surface::Wall;w.hasHeight=true;w.hflip=flip&1;w.vflip=flip&2;w.wallTemplateOffset=0x2e8;
   for(int y=0;y<8;y++)for(int xx=0;xx<8;xx++)w.height[y*8+xx]=23-y;
   auto &cap=(*tiles)[capY*64+x];cap.surface=Surface::Floor;cap.hasHeight=true;cap.structuralFloorPlaneMask=0;
  }
  // Identical blocked pattern beside the cap has no assembly ownership.
  (*tiles)[capY*64+14]=(*tiles)[capY*64+13];
  (*tiles)[2*64+2]=(*tiles)[capY*64+13];
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  assert(map->offsets[11*64+11]==0);
  assert(map->offsets[wallY*64+11]==-16);
  assert(map->offsets[capY*64+11]==8);
  assert(map->offsets[capY*64+14]==255 && map->offsets[2*64+2]==255);
  assert(map->structuralConflicts==0 && map->unknownPlaneCells[0]==0);
  // Without ROM assembly provenance the same local profile is insufficient.
  for(int x=10;x<14;x++)(*tiles)[wallY*64+x].wallTemplateOffset=0;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  assert(map->offsets[capY*64+11]==255);
 }
 // An exact max-ramp corner owns a one-unit boundary continuation even at
 // its clipped endpoint. A solved side face then propagates along tangents.
 for(int flip=0;flip<4;flip++){
 *tiles={};attrs->fill(1);
 for(int y=11;y<14;y++)for(int x=9;x<14;x++){
  auto &t=(*tiles)[y*64+x];t.surface=Surface::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;
 }
 c.linkY=11*8;
 auto &corner=(*tiles)[9*64+9];corner.surface=Surface::Wall;corner.hasHeight=true;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)corner.height[y*8+x]=std::max(x,y);
 auto &face=(*tiles)[9*64+10];face.surface=Surface::Wall;face.hasHeight=true;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)face.height[y*8+x]=x;
 auto &bottom=(*tiles)[10*64+9];bottom.surface=Surface::Wall;bottom.hasHeight=true;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)bottom.height[y*8+x]=y;
 for(auto *t:{&corner,&face,&bottom}){
  const auto displayed=t->height;t->hflip=flip&1;t->vflip=flip&2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)t->height[(t->vflip?7-y:y)*8+(t->hflip?7-x:x)]=displayed[y*8+x];
 }
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
 assert(map->offsets[9*64+9]==-16 && map->offsets[9*64+10]==-8);
 }
 std::cout<<"PASS cap: direct flipped authored seams, no blocked-pattern datum/flood\n";
}
