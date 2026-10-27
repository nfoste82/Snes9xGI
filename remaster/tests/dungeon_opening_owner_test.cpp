// Exact final opening words, not artwork connectivity or nearest floor, own
// terminal door/stair datums. Frame-only template equivalence is insufficient.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 uint8_t pixels[64]={};RemasterProfile p;
 RemasterTileContentId doorId{S9xRemasterHashTile(4,pixels),1,4};
 pixels[0]=1;(*gfx)[32]=128;
 RemasterTileContentId floorId{S9xRemasterHashTile(4,pixels),1,4};
 auto &door=p.assets[doorId];door.tileId=doorId;door.tileRole=RemasterTileRole::Doorway;
 door.hasHeight=door.hasDoorframeMask=true;door.doorframeMask=UINT64_MAX;
  auto &floor=p.assets[floorId];floor.tileId=floorId;floor.tileRole=RemasterTileRole::Floor;floor.hasHeight=true;
  pixels[0]=2;(*gfx)[65]=128;
  RemasterTileContentId wallId{S9xRemasterHashTile(4,pixels),1,4};
  auto &wall=p.assets[wallId];wall.tileId=wallId;wall.tileRole=RemasterTileRole::Wall;wall.hasHeight=true;
  C c;c.verifiedAlttpRom=c.indoors=true;c.placementBackgroundIndex=0;c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();
 for(int split=0;split<2;split++)for(int family=0;family<(split?1:2);family++)for(int d=0;d<(family?2:4);d++)for(int flip=0;flip<4;flip++) {
  const int starts[]={0x2716,0x29f0,0x2c6a,0x2e6a};
  const int off=family?(d?0x11e8:0x11a8):starts[d],w=d<2?4:3,h=d<2?3:4;
   tpl->fill(255);maps->fill(255);floor.height.fill(family?11:0);
   for(int k=0;k<64;k++)door.height[k]=7-k%8;
  auto cell=[&](int x,int y){return (10+(flip&2?h-1-y:y))*64+10+(flip&1?w-1-x:x);};
  for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
   bool frame=d<2?(x==0 || x==w-1 || y==(d? h-1:0)):(y==0 || y==h-1 || x==(d==2?0:w-1));
   int n=off+2*(x*(family?4:h)+y+(family && d==1?1:0)),word=frame?0:1,i=cell(x,y);
   (*tpl)[n]=word;(*tpl)[n+1]=0;
   const bool outer=d<2?y==(d?h-1:0):x==(d==2?0:w-1);
   const int at=(split && outer?8192:0)+2*i;
   (*maps)[at]=word;(*maps)[at+1]=flip<<6;
  }
   const int opening=cell(1,1);
   // An exact door remains the stronger owner even if ordinary wall art
   // touches both ends of its full outer band. Test the legacy east-band
   // reclassifier, including split-BG ownership, without a room fixture.
   if(d==3 && flip==0)for(int yy:{9,14}) {
    const int at=(split?8192:0)+2*(yy*64+12);
    (*maps)[at]=2;(*maps)[at+1]=0;
   }
   S9xRemasterDungeonPlacements(c,p,*tiles);
   if(d==3 && flip==0 && !split)assert((*tiles)[10*64+12].surface==S::Door);
  assert((*tiles)[opening].doorAssembly==10*64+11 && (*tiles)[opening].doorDirection==(d^((flip&(d<2?2:1))?1:0)));
   assert((*tiles)[opening].height==floor.height && floor.height[0]==(family?11:0));
   assert((*tiles)[opening].splitDoorPart==bool(split));
  // A changed opening must not inherit the still-intact frame's ownership.
  (*maps)[2*opening]=2;S9xRemasterDungeonPlacements(c,p,*tiles);assert(!(*tiles)[opening].doorAssembly);
  (*maps)[2*opening]=1;floor.geometryByFlip=true;
  S9xRemasterDungeonPlacements(c,p,*tiles);assert(!(*tiles)[opening].doorAssembly);
  floor.geometryByFlip=false;
 }
 std::cout<<"PASS exact opening ownership: ordinary/stair, all axes/flips, nonflat intrinsic relief, overwritten and ambiguous source rejection\n";
}
