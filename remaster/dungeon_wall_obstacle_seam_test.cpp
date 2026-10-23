// Exact tangent seams to a separately supported authored assembly, not proximity.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;
 c.collisionAttributes=attrs->data();c.placements=tiles->data();
 int cases=0;
 for(int bg=0;bg<2;bg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int mode=0;mode<9;mode++) {
  attrs->fill(1);tiles->fill({});c.placementBackgroundIndex=bg;
  for(int y=8;y<24;y++)for(int x=8;x<24;x++) {
   (*attrs)[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  }
  const int origin=16*64+16,step=axis?1:64;
  for(int n=0;n<4;n++) {
   const int i=origin+n*step;auto &t=(*tiles)[i];t={};t.surface=n==0 || n==3?S::Obstacle:S::Wall;
   t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;(*attrs)[i]=1;
   for(int k=0;k<64;k++)t.height[k]=axis?7-k/8:7-k%8;
   if(n==0 || n==3){t.authoredObstacle=true;t.authoredObstacleAssembly=i+1;t.authoredObstacleContact=true;
    t.authoredObstacleDirection=axis?1:3;}
  }
  // Walls do not have an independently assumed floor along their rise axis.
  // The two endpoints alone contact the reproduced open support region.
  for(int n:{1,2})for(int sign:{-1,1}) {
   const int j=origin+n*step+(axis?64:1)*sign;(*attrs)[j]=1;(*tiles)[j]={};
  }
  if(mode==1)(*tiles)[origin].authoredObstacleAssembly=(*tiles)[origin+3*step].authoredObstacleAssembly=0;
  if(mode==2)(*tiles)[origin].height[3]++; // incompatible overwritten endpoint; other endpoint still proves it
  if(mode==3){(*tiles)[origin].height[3]++;(*tiles)[origin+3*step].height[3]++;}
  if(mode==4){(*tiles)[origin].height.fill(0);(*tiles)[origin+3*step].height.fill(0);}
  if(mode==5){(*attrs)[origin+step]=(*attrs)[origin+2*step]=0;} // underpass/occluder
  if(mode==6){(*tiles)[origin+step].height[27]++;(*tiles)[origin+2*step].height[27]++;}
  if(mode==7)for(auto &h:(*tiles)[origin+3*step].height)h+=8; // two different seam datums
  if(mode==8){(*tiles)[origin].hasHeight=false;(*tiles)[origin+3*step].hasHeight=false;}
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  const bool solved=mode==0 || mode==2;
  for(int n:{1,2})if((map->structuralPlaneOffsets[0][origin+n*step]!=255)!=solved){std::cerr<<bg<<' '<<axis<<' '<<flip<<' '<<mode<<' '<<n<<" base="<<map->structuralPlaneOffsets[0][origin+n*step]<<'\n';need(false,"tangent proof/negative");}
  if(solved)for(int n:{1,2})need(map->offsets[origin+n*step]==0,"seam base not support relief");
  cases++;
 }
 std::cout<<"PASS "<<cases<<" tangent assembly cases: axes/flips/BGs/overwrites/ambiguous/flat/underpass/relief\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
