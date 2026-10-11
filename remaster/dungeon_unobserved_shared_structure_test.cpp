// Shared solid BG2 shell supplies a signed structural anchor, NOT movement.
#include "remaster.h"
#include <stdexcept>
#include <iostream>
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=1;c.linkX=80;c.linkY=80;
 c.collisionAttributes=attrs->data();c.placements=tiles->data();
 for(int flip=0;flip<4;flip++) {
  tiles->fill({});attrs->fill(1);
  // Observed lower support contacts ONLY the leftmost shared solid wall.
  for(int y=10;y<14;y++)for(int x=8;x<12;x++) {(*attrs)[4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;}
  for(int x=12;x<16;x++) {
   auto &t=(*tiles)[11*64+x];t.surface=S::Wall;t.hasHeight=true;t.wallTemplateOffset=0x2c8;t.distinctLowerUsage=true;
   t.hflip=flip&1;t.vflip=flip&2;
   for(int y=0;y<8;y++)for(int xx=0;xx<8;xx++)t.height[y*8+xx]=flip&1?7-xx:xx;
   (*attrs)[4096+11*64+x]=x==12?1:0; // remainder is occluder over lower support
  }
  require(S9xRemasterBuildDungeonHeightMap(c,50,*m),"solve");
  require(m->sharedStructuralFaces==1 && m->structuralConflicts==0,"exact shared witness");
  require(m->requiredPlaneCells[0]==0 && m->planeOffsets[0][10*64+10]==255,"unobserved movement invented");
  for(int x=12;x<16;x++)require(m->offsets[11*64+x]==8*(x-12),"signed wall chain did not inherit witnessed base");
  // BG1 differing artwork cannot assert equality with the visible BG2 shell.
  c.placementBackgroundIndex=0;require(S9xRemasterBuildDungeonHeightMap(c,50,*m) && !m->sharedStructuralFaces && m->offsets[11*64+15]==255,"ambiguous BG1 borrowed visible shell");c.placementBackgroundIndex=1;
  // Partial overwrite removes ROM ownership; no common-solid bridge remains.
  (*tiles)[11*64+12].wallTemplateOffset=0;
  require(S9xRemasterBuildDungeonHeightMap(c,50,*m) && !m->sharedStructuralFaces && m->offsets[11*64+15]==255,"unowned overwritten join anchored graph");
  (*tiles)[11*64+12].wallTemplateOffset=0x2c8;(*attrs)[4096+11*64+12]=0;
  require(S9xRemasterBuildDungeonHeightMap(c,50,*m) && !m->sharedStructuralFaces && m->offsets[11*64+15]==255,"underpass supplied shell equality");
 }
 std::cout<<"PASS unobserved structural chains from common solid BG2 witness; all flips, signed seams, no movement activation, overwrite/BG1/underpass rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
