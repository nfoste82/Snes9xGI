// Nonblocking beveled wall feet are terminal floor consumers, not seam bridges.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;
 c.collisionAttributes=attrs->data();c.placements=tiles->data();int cases=0;
 for(int bg=0;bg<2;bg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int mode=0;mode<10;mode++) {
  attrs->fill(1);tiles->fill({});c.placementBackgroundIndex=bg;c.stairCount=0;
  for(int y=8;y<24;y++)for(int x=8;x<24;x++) {
   (*attrs)[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  }
  const int i=16*64+16;auto &t=(*tiles)[i];t={};t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
  // Irregular, non-affine bevel; zero foot, no uniform outer-stack boundary.
  for(int k=0;k<64;k++){int n=axis?k/8:k%8;t.height[k]=std::max(0,n-(axis?k%8:k/8)/2);}
  const int sign=axis?(t.vflip?1:-1):(t.hflip?1:-1),j=i+sign*(axis?64:1);
  // Only the declared downhill edge borders explicit Floor. Other sides
  // retain occupied support under unknown art; it is not wall ownership.
  for(auto d:{std::array<int,2>{1,0},{-1,0},{0,1},{0,-1}}){int n=i+d[0]+64*d[1];if(n!=j)(*tiles)[n].surface=S::Unknown;}
  if(mode==1)t.hasHeight=false;
  if(mode==2)t.height.fill(0);
  if(mode==3)for(auto &h:t.height)h++;
  if(mode==4)t.height[axis?4*8:4]= -1; // nonmonotone section
  if(mode==5)(*tiles)[j].surface=S::Unknown;
  if(mode==6)(*tiles)[j].height.fill(1); // no zero-local seam
  if(mode==7){c.stairCount=1;c.stairs[0]={30,30,0,0,true,true};for(int n=0;n<4096;n++)(*attrs)[4096+n]=(*attrs)[n];} // equal-height overlap is ambiguous
  if(mode==8)t.authoredObstacle=true; // independently owned assembly cannot be rescued
  if(mode==9)(*attrs)[i]=1; // solid face belongs to existing structural solve
  require(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  if(mode==6)require(map->structuralConflicts>0,"contradictory floor datum remains conflict");
  const bool solved=mode==0;
  if(mode!=1 && mode!=9 && (map->structuralPlaneOffsets[0][i]!=255)!=solved){std::cerr<<bg<<' '<<axis<<' '<<flip<<' '<<mode<<" base="<<map->structuralPlaneOffsets[0][i]<<'\n';require(false,"terminal contact/negative");}
  if(mode==1)require(map->localHeightPreviews.empty(),"missing source not synthesized");
  if(solved){require(map->offsets[i]==0,"foot base");require(map->planeOffsets[0][i]==0,"support independent");require(map->structuralPlaneOffsets[1][i]==255,"unobserved plane");require(!map->fallbackDirections[i],"no inferred ramp");}
  cases++;
 }
 std::cout<<"PASS "<<cases<<" direct foot contacts axes/flips/BGs/missing/nonmonotone/ownership/plane negatives\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
