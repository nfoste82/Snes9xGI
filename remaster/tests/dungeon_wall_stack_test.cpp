// A whole ROM wall section inherits signed surface seams, not decorative relief.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterProfile profile;std::array<RemasterTileContentId,16> ids;
 for(int n=0;n<16;n++) {
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=1+(k+n*3)%15;pixels[0]=n&15;pixels[1]=n>>4;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*gfx)[n*32+b/2*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[ids[n]];a.hasHeight=true;a.tileRole=RemasterTileRole::Wall;
  for(int k=0;k<64;k++)a.height[k]=7-k%8;
 }
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.graphics=gfx->data();c.constructedMaps=maps->data();c.dungeonTemplateData=tpl->data();c.collisionAttributes=attr->data();
 auto put=[&](int i,int word){(*maps)[i*2]=word;(*maps)[i*2+1]=word>>8;};
 auto src=[&](int off,int n,int word){(*tpl)[off+n*2]=word;(*tpl)[off+n*2+1]=word>>8;};
 int cases=0;
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int mode=0;mode<6;mode++) {
  maps->fill(0xff);tpl->fill(0xff);c.placementBackgroundIndex=bg;
  for(int yy=0;yy<4;yy++)for(int xx=0;xx<4;xx++) {
   const int sx=flip&1?3-xx:xx,sy=flip&2?3-yy:yy,n=sx*4+sy;
   src(0x7aa,n,n);put((20+yy)*64+20+xx,n^(flip<<14));
  }
  if(mode==1)put(20*64+20,999); // one changed tile vetoes all ownership
  if(mode==2)profile.assets[ids[0]].hasHeight=false;
  if(mode==3)profile.assets[ids[0]].tileRole=RemasterTileRole::Floor;
  if(mode==4)for(int n=0;n<16;n++)src(0x7ca,n,n); // distinct template claims ambiguous
  if(mode==5)put(20*64+20,((flip&1?12:0)+(flip&2?3:0))|(flip<<14)|0x2000); // priority-only writer preserves structure
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  for(int yy=0;yy<4;yy++)for(int xx=0;xx<4;xx++)if(bool((*tiles)[(20+yy)*64+20+xx].wallStackAssembly)!=(mode==0 || mode==5)){std::cerr<<"class "<<bg<<' '<<flip<<' '<<mode<<' '<<xx<<' '<<yy<<'\n';need(false,"complete/transformed/overwritten/ambiguous ROM ownership");}
  profile.assets[ids[0]].hasHeight=true;profile.assets[ids[0]].tileRole=RemasterTileRole::Wall;cases++;
 }
 // Solver-only cross sections isolate exact edge ownership. The obstacle has
 // sculpted interior relief but an intact actual contacting edge. Special wall
 // bands require unique ROM ownership, not a collision shortcut.
 c.constructedMaps=nullptr;c.graphics=nullptr;c.dungeonTemplateData=nullptr;c.placements=tiles->data();c.linkX=80;c.linkY=80;
 for(int bg=0;bg<2;bg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int mode=0;mode<8;mode++) {
  tiles->fill({});attr->fill(1);c.placementBackgroundIndex=bg;c.stairCount=0;
  for(int y=8;y<24;y++)for(int x=8;x<24;x++){(*attr)[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;}
  const int origin=16*64+16,normal=axis?64:1,tangent=axis?1:64;
  for(int band=0;band<3;band++)for(int n=0;n<4;n++) {
   const int i=origin+band*normal+n*tangent;auto &t=(*tiles)[i];t={};t.surface=(n==0 || n==3)?S::Obstacle:S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   t.wallStackAssembly=origin+1;(*attr)[i]=band==1?0x6e:1;
   // Authored absolute sectional origin may be rebased; solve cancels it.
   for(int k=0;k<64;k++)t.height[k]=7-(axis?k/8:k%8)+(mode==6?13:0);
   if(n==0 || n==3) {
    for(auto &h:t.height)h+=((flip&(axis?2:1))?band:2-band)*8;
    t.authoredObstacle=true;t.authoredObstacleAssembly=i+1;t.authoredObstacleContact=true;t.authoredObstacleDirection=axis?1:3;
    // Modify only interior pixels, preserving BOTH possible displayed edges.
    t.height[3*8+3]+=4;
    if(mode==1)t.authoredObstacleAssembly=0;
    if(mode==2)for(int k:{3,59,24,31})t.height[k]++;
    if(mode==3 && n==3)for(auto &h:t.height)h+=8;
   }
   if(mode==4)t.wallStackAssembly=0;
   if(mode==5 && n>0 && n<3)(*attr)[i]=0; // underpass, never attach visible wall
   if(mode==7){t.distinctLowerUsage=true;c.stairCount=1;c.stairs[0]={30,30,0,0,true,true};}
  }
  if(mode==7)for(int i=0;i<4096;i++)(*attr)[4096+i]=(*attr)[i];
  // Prevent floor color across the normal axis from acting as an anchor.
  for(int n:{1,2})for(int k:{-1,3}){int j=origin+k*normal+n*tangent;(*attr)[j]=1;(*tiles)[j]={};}
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  const bool solved=mode==0 || mode==6;
  for(int band=0;band<3;band++)for(int n:{1,2}) {
   const int i=origin+band*normal+n*tangent;
   if(mode==4 && band!=1)continue; // only special band requires ROM ownership; ordinary bands have independent preexisting graph policy
   if(mode==3 && band!=1)continue; // preserve independent smaller tangent proofs, but never solve contradictory special extension
   const bool bandSolved=solved;
   if(bool(map->structuralPlaneOffsets[0][i]!=255)!=bandSolved){std::cerr<<bg<<' '<<axis<<' '<<flip<<' '<<mode<<' '<<band<<" base="<<map->structuralPlaneOffsets[0][i]<<'\n';need(false,"section seam/plane/ambiguity negative");}
   const int expected=(flip&(axis?2:1))?band*8:(2-band)*8;
   if(solved && map->offsets[i]!=expected){std::cerr<<"base "<<bg<<' '<<axis<<' '<<flip<<' '<<mode<<' '<<band<<' '<<n<<' '<<map->offsets[i]<<'\n';need(false,"independent edge datum retains authored local origin");}
  }
  if(mode==3)need(map->structuralConflicts>0,"conflicting edge anchors recorded");
  cases++;
 }
 std::cout<<"PASS "<<cases<<" whole-section ownership and sculpted-edge seam cases, BG/axes/HV/overwrites/plane/conflicts\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
