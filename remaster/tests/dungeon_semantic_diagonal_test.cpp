#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 int cases=0;
 for(int extent:{5,7,9,12,20})for(int width:{2,5})for(int flip=0;flip<4;flip++) {
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
  for(auto &t:*tiles){t.semanticBoundaryKnown=true;t.surface=RemasterDungeonFloorContext::Surface::Floor;t.hasHeight=true;}
  const int up=(flip&2)?1:-1,start=(flip&2)?5:40,tangent=bool(flip&1)==bool(flip&2)?-1:1;
  for(int col=0;col<width;col++)for(int row=0;row<extent;row++) {
   auto &t=(*tiles)[(start+col*tangent+row*up)*64+10+col];
   t.semanticSoutheastWall=true;t.surface=RemasterDungeonFloorContext::Surface::Wall;
   t.hflip=flip&1;t.vflip=flip&2;t.height.fill(24); // repeated authored internal cap
   t.tileHash=100+row; // canonical same-band identity, transformed by placement
   if(!row)t.groundedFloorMask=1;
  }
  const auto original=*tiles;
  S9xRemasterSemanticDiagonalExtents(*tiles);
  for(int col=0;col<width;col++)for(int row=0;row<extent;row++) {
   const auto &t=(*tiles)[(start+col*tangent+row*up)*64+10+col];
   check(t.diagonalHeightPreview,"actual placed semantic extent activated");
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)check(t.height[y*8+x]==std::max(0,std::min((extent-1)*8,7+row*8-x-y)),"no fixed cap repeated intrinsic");
  }
  *tiles=original;(*tiles)[(start+extent*up)*64+10].semanticBoundaryKnown=false;
  S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[start*64+10].diagonalHeightPreview,"unknown top rejects");
  *tiles=original;for(int col=0;col<width;col++)(*tiles)[(start+col*tangent)*64+10+col].groundedFloorMask=0;
  S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[start*64+10].diagonalHeightPreview,"unsupported foot rejects");
  *tiles=original;(*tiles)[(start+2*up)*64+10].hflip=!(flip&1);
  S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[start*64+10].diagonalHeightPreview,"ambiguous transformed assembly rejects");
  if(extent>5 && width==5) {
   *tiles=original;
   // Final cardinal overwrite removes the first two foot bands from one
   // column. Surviving upper bands retain exact tangent/top ownership.
   for(int row=0;row<2;row++) {
    auto &t=(*tiles)[(start+row*up)*64+10];t.semanticSoutheastWall=false;t.groundedFloorMask=0;
   }
   const auto partial=*tiles;S9xRemasterSemanticDiagonalExtents(*tiles);
   for(int row=2;row<extent;row++)check((*tiles)[(start+row*up)*64+10].diagonalHeightPreview,"overwritten foot tangent attachment");
   check(!(*tiles)[start*64+10].diagonalHeightPreview,"overwriter not reclassified");
   *tiles=partial;(*tiles)[(start+extent*up)*64+10].semanticBoundaryKnown=false;
   S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[(start+2*up)*64+10].diagonalHeightPreview,"partial unknown top rejects");
   *tiles=partial;for(int row=2;row<extent;row++)(*tiles)[(start+row*up)*64+10].tileHash+=1000;
   S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[(start+2*up)*64+10].diagonalHeightPreview,"unrelated tangent identities reject");
   *tiles=partial;for(int row=2;row<extent;row++)(*tiles)[(start+row*up)*64+10].hflip=!(flip&1);
   S9xRemasterSemanticDiagonalExtents(*tiles);check(!(*tiles)[(start+2*up)*64+10].diagonalHeightPreview,"partial transformed ownership rejects");
  }
  cases++;
 }
 std::cout<<"PASS "<<cases<<" actual semantic placement extents5/7/9/12/20, repeated cap, widths2/5, all flips, unknown boundary/support/transform rejection\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
