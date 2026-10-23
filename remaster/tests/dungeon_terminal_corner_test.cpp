// Two owned folded faces attach to authored tangent seams, never trim support.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using S=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=88;c.linkY=88;
 c.placements=tiles->data();c.collisionAttributes=attr->data();
 for(int flip=0;flip<4;flip++) {
  *tiles={};attr->fill(1);
  for(int y=11;y<15;y++)for(int x=11;x<15;x++) {
   auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attr)[y*64+x]=0;
  }
  const int cx=flip&1?15:10,cy=flip&2?15:10,dx=flip&1?-1:1,dy=flip&2?-1:1;
  const int corner=cy*64+cx,east=cy*64+cx+dx,south=(cy+dy)*64+cx;
  auto &t=(*tiles)[corner];t.surface=S::Wall;t.hasHeight=true;t.terminalCorner=true;t.wallTemplateOffset=0xbe6;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=std::max(7-x,7-y);
  for(int i:{east,south}) {
   auto &w=(*tiles)[i];w.surface=S::Wall;w.hasHeight=true;w.wallTemplateOffset=0xbe6;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)w.height[y*8+x]=16+(i==east?7-y:7-x);
  }
  // Preserve displayed geometry while transforming canonical arrays.
  for(int i:{corner,east,south}) {auto &w=(*tiles)[i];w.hflip=flip&1;w.vflip=flip&2;}
  auto &shell=(*tiles)[cy*64+cx-dx];shell.surface=S::Obstacle;shell.hasHeight=true;shell.height.fill(90);
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  assert(map->offsets[east]==-16 && map->offsets[south]==-16 && map->offsets[corner]==0);
  assert(map->structuralConflicts==0 && map->fallbackDirections[corner]==0);
  auto original=(*tiles)[south].height;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)if(xx==0)(*tiles)[south].height[yy*8+xx]++;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  // Flat floor seam can solve each branch yet their folded-face offsets
  // disagree: explicit conflict, never choose one convenient face.
  assert(map->offsets[corner]==255);
  (*tiles)[south].height=original;
  // No corner-only datum; both faces are mandatory and ownership must agree.
  (*tiles)[south].wallTemplateOffset=0xc06;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->offsets[corner]==255);
  // One overwritten inward branch can use its perpendicular exact proved
  // high-perimeter face.04 is permitted only with solved template ownership;
  // the exact max-ramp extrapolates one pixel beyond its clipped tangent.
  (*attr)[south]=1;(*tiles)[east].surface=S::Unknown;
  const int high=(cy-dy)*64+cx;auto &h=(*tiles)[high];h.surface=S::Wall;h.hasHeight=true;
  h.wallTemplateOffset=0xb66;h.highPerimeterFace=true;h.hflip=flip&1;h.vflip=flip&2;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)h.height[yy*8+xx]=8+7-yy;
  (*attr)[high]=4;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  // This synthetic high face is disconnected; explicit relative-only base
  // cannot identify a supported face and must NOT rescue the corner.
  assert(map->offsets[corner]==255);
  (*tiles)[south].wallTemplateOffset=0xbe6;(*tiles)[south].surface=S::Unknown;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->offsets[corner]==255);
  (*tiles)[south].surface=S::Wall;(*attr)[south]=0; // underpass is not structural support
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->offsets[corner]==255);
 }
 std::cout<<"PASS terminal two-face seams, shell separation, missing/background/underpass rejection\n";
}
