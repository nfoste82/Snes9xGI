// Mixed cap/face corner: two independently solved faces, never ceiling support.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void check(bool v,const char *s){if(!v)throw std::runtime_error(s);}
int main(){try{
 using S=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=13;c.linkX=88;c.linkY=88;
 c.placements=tiles->data();c.collisionAttributes=attr->data();
 for(int flip=0;flip<4;flip++)for(int relief:{-256,0,37}) {
  *tiles={};attr->fill(1);
  for(int y=11;y<15;y++)for(int x=11;x<15;x++) {auto &t=tiles->at(y*64+x);t.surface=S::Floor;t.hasHeight=true;(*attr)[y*64+x]=0;}
  const int cx=flip&1?15:10,cy=flip&2?15:10,dx=flip&1?-1:1,dy=flip&2?-1:1;
  const int corner=cy*64+cx,east=cy*64+cx+dx,south=(cy+dy)*64+cx;
  auto &t=tiles->at(corner);t.surface=S::Wall;t.hasHeight=true;t.highCornerFace=true;t.wallTemplateOffset=0xb66;
  t.hflip=flip&1;t.vflip=flip&2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=relief+std::min(6,std::max(7-x,7-y));
  for(int i:{east,south}) {auto &w=tiles->at(i);w.surface=S::Wall;w.hasHeight=true;w.wallTemplateOffset=0xb66;w.hflip=t.hflip;w.vflip=t.vflip;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)w.height[y*8+x]=relief+(i==east?7-y:7-x);}
  // An adjacent ceiling is authored relief, not a walkable height anchor.
  auto &ceiling=tiles->at(cy*64+cx-dx);ceiling.surface=S::Obstacle;ceiling.hasHeight=true;ceiling.height.fill(600);
  check(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  check(map->offsets[corner]==-relief,"signed clipped two-face base");
  check(map->planeOffsets[0][corner]==255 && map->fallbackDirections[corner]==0,"ceiling is not support/fallback");
  const auto original=t.height;
  auto old=tiles->at(south).height;tiles->at(south).height[7]++;
  check(S9xRemasterBuildDungeonHeightMap(c,50,*map),"mixed solve");
  check(map->offsets[corner]==255,"incompatible two-face levels remain unknown");
  tiles->at(south).height=old;tiles->at(south).wallTemplateOffset=0xba6;
  check(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[corner]==255,"ambiguous assembly");
  tiles->at(south).wallTemplateOffset=0xb66;(*attr)[south]=0;
  check(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[corner]==255,"underpass cannot own face");
  (*attr)[south]=1;t.highCornerFace=false;
  check(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[corner]==255,"unowned clipped artwork");
  check(t.height==original,"authored relief immutable");
 }
 std::cout<<"PASS clipped corners: four flips, signed relief, mixed ceiling, independent bands, ownership and underpass rejection\n";
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
