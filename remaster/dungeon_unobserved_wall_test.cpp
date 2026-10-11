// Room050 failure class: an observed underpass is not a high-wall anchor.
#include "remaster.h"
#include <stdexcept>
#include <iostream>
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using S=RemasterDungeonFloorContext::Surface;
 auto placements=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=1;c.linkX=80;c.linkY=80;
 c.placements=placements->data();c.collisionAttributes=attrs->data();
 const int wall=12*64+12;
 for(int flip=0;flip<4;flip++) {
  placements->fill({});attrs->fill(1);
  for(int y=10;y<16;y++)for(int x=10;x<16;x++) {
   (*attrs)[4096+y*64+x]=0;
   auto &p=(*placements)[y*64+x];p.surface=S::Floor;p.hasHeight=true;p.floorPlaneMask=2;
  }
  auto &p=(*placements)[wall];p.surface=S::Wall;p.wallTemplateOffset=0x2c8;p.hflip=flip&1;p.vflip=flip&2;
  for(int k=0;k<64;k++)p.height[k]=k%8;
  require(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  require(map->requiredPlaneCells[0]==0 && map->requiredPlaneCells[1]>0,"invented unobserved movement");
  require(map->planeOffsets[1][wall]==0 && map->structuralPlaneOffsets[1][wall]==255 &&
   map->offsets[wall]==255 && map->unknownWallCells==1,"underpass supplied unproved wall datum");
  // Contradictory/disconnected art cannot license activation by similarity.
  p.distinctLowerUsage=true;
  require(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[wall]==255,"ambiguous wall filled");
 }
 std::cout<<"PASS unobserved solid-wall/observed underpass separation, all flips, explicit unresolved ownership\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
