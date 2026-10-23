#include "../../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 std::array<uint8_t,8192> attributes;C c;c.verifiedAlttpRom=c.indoors=true;
 c.roomIndex=7;c.linkX=160;c.linkY=152;c.placements=tiles->data();c.collisionAttributes=attributes.data();
 int cases=0;
 // Same clipped assembly and ownership, with an intrinsic datum translation.
 // Only its placement base changes; displayed effective geometry must agree.
 for(int datum:{-300,0})for(int flip=0;flip<4;flip++) {
  *tiles={};attributes.fill(1);
  for(int y=20;y<23;y++)for(int x=20;x<24;x++) {
   attributes[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  }
  for(int n=0;n<3;n++) {
   auto &t=(*tiles)[17*64+((flip&1)?12-n:10+n)];t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=datum+(n==0?std::min(13,14-x-y):n==1?14-x-y:std::max(0,10-x-y));
  }
  auto &floor=(*tiles)[17*64+((flip&1)?9:13)];floor.surface=S::Floor;floor.hasHeight=true;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)floor.height[y*8+x]=std::max(0,3-((flip&2)?7-y:y));
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"clipped solve");
  need(map->offsets[17*64+((flip&1)?12:10)]==12-datum,"clipped high base");
  need(map->offsets[17*64+11]==4-datum,"clipped middle base");
  need(map->offsets[17*64+((flip&1)?10:12)]==-datum,"clipped low base");cases++;
 }
 // Mixed floor's placement-local difference may exceed a byte, while both
 // floor bases retain the existing valid range and unknown sentinel.
 for(int difference:{256,512}) {
  *tiles={};attributes.fill(1);
  for(int y:{20,26})for(int yy=y;yy<y+3;yy++)for(int x=20;x<24;x++) {
   auto &t=(*tiles)[yy*64+x];t.surface=S::Floor;t.hasHeight=true;attributes[yy*64+x]=0;
  }
  for(int y=23;y<26;y++)for(int x=20;x<24;x++) {
   auto &t=(*tiles)[y*64+x];t.surface=S::Wall;t.hasHeight=true;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
  }
  const int cell=21*64+19;auto &t=(*tiles)[cell];t.surface=S::Wall;t.hasHeight=true;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   t.height[y*8+x]=x<3?difference+(2-x):0;
   if(x>=3)t.groundedFloorMask|=UINT64_C(1)<<(y*8+x);
  }
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"mixed solve");
  const int base=map->structuralPlaneOffsets[0][cell];
  need(base!=255,"mixed structural source");
  need(map->mixedFloorBases[0][cell]==24,"mixed signed local difference accepted");
  auto local=S9xRemasterMixedFloorHeight(t.height,base,24);
  need(local[7]==24-base,"mixed override not truncated");
  need(local[0]==t.height[0],"mixed authored slope preserved");cases++;
 }
 for(int datum:{-300,512,32728})for(int flip=0;flip<4;flip++) {
  *tiles={};for(auto &t:*tiles){t.semanticBoundaryKnown=true;t.surface=S::Floor;t.hasHeight=true;}
  const int up=flip&2?1:-1,start=flip&2?5:40,tangent=bool(flip&1)==bool(flip&2)?-1:1;
  for(int col=0;col<2;col++)for(int row=0;row<5;row++) {
   auto &t=(*tiles)[(start+col*tangent+row*up)*64+10+col];
   t.semanticSoutheastWall=true;t.surface=S::Wall;t.hflip=flip&1;t.vflip=flip&2;t.height.fill(datum);
   if(!row){t.diagonalHeightPreview=true;t.diagonalPreviewRow=3;t.diagonalPreviewFoot=datum;}
  }
  S9xRemasterSemanticDiagonalExtents(*tiles);
  const auto &top=(*tiles)[(start+4*up)*64+10];
  need(top.diagonalMinimum==datum && top.diagonalMaximum==datum+32,"semantic model limits not truncated");
  need(top.diagonalIntercept==datum+39,"unclipped intercept not truncated");
  need(top.height[0]==datum+32 && top.height[63]==datum+25,"semantic samples not truncated");cases++;
 }
 std::array<int16_t,64> boundary={};boundary[0]=32767;
 auto local=S9xRemasterMixedFloorHeight(boundary,-32743,24);
 need(local[7]==32767 && local[0]==32767,"mixed int16 maximum");
 need(S9xRemasterMixedFloorHeight(boundary,-32744,24)==boundary,"unrepresentable difference not narrowed");
 C::Placement t;t.diagonalPreviewFoot=-32768;t.diagonalMinimum=-32768;t.diagonalMaximum=32767;t.diagonalIntercept=32768;
 need(t.diagonalPreviewFoot==-32768 && t.diagonalMaximum==32767 && t.diagonalIntercept==32768,"model datum storage");
 std::cout<<"PASS "<<cases<<" signed dungeon geometry cases\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
