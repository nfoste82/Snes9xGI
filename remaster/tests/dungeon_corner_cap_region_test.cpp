// Ordered corner ceiling regions are not exterior-floor contact surfaces.
// This is a placement-local graph rule; authored arrays never change.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.placements=tiles->data();c.collisionAttributes=attrs->data();
 for(int flip=0;flip<4;flip++)for(int folded=0;folded<2;folded++) {
  auto index=[&](int x,int y){return ((flip&2)?63-y:y)*64+((flip&1)?63-x:x);};
  auto setup=[&] {
   *tiles={};attrs->fill(1);
   for(int y=10;y<14;y++)for(int x=13;x<17;x++) {
    auto &t=(*tiles)[index(x,y)];t.surface=Surface::Floor;t.hasHeight=true;
    (*attrs)[index(x,y)]=0;
   }
   for(int y=10;y<14;y++)for(int x=10;x<13;x++) {
    auto &t=(*tiles)[index(x,y)];t.surface=Surface::Wall;t.hasHeight=true;
    t.wallTemplateOffset=0xb86;t.highCornerFace=true;t.hflip=flip&1;t.vflip=flip&2;
    for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)
     t.height[yy*8+xx]=x==10?std::min(5,folded?std::max(7-xx,yy):7-xx):folded?std::max(7-xx,yy):7-xx;
   }
   // Folded rows are tested as tangent contacts only; use an affine foot
   // row so every retained face still has an independent support anchor.
   if(folded)for(int x=10;x<13;x++) {
    auto &t=(*tiles)[index(x,13)];for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=x==10?std::min(5,7-xx):7-xx;
   }
   for(int y=10;y<14;y++) {auto &t=(*tiles)[index(9,y)];t.surface=Surface::Floor;t.hasHeight=true;}
   for(int x=9;x<17;x++) {auto &t=(*tiles)[index(x,14)];t.surface=Surface::Floor;t.hasHeight=true;}
   c.linkX=((flip&1)?47:13)*8;c.linkY=((flip&2)?50:10)*8;
  };
  setup();const auto original=*tiles;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  if(!folded) {
   assert(map->offsets[index(12,11)]==0);
   assert(map->offsets[index(11,11)]==8);
   assert(map->offsets[index(10,11)]==16);
   assert(map->structuralConflicts==0);
  }
  else assert(map->offsets[index(10,11)]==-5 && map->structuralConflicts==0);
  for(int i=0;i<4096;i++)assert((*tiles)[i].height==original[i].height);
  // No ROM ownership / overwritten downhill source / arbitrary relief:
  // do not drop the cap contact to conceal a contradiction.
  if(!folded)for(int negative=0;negative<6;negative++) {
   setup();
   for(int y=10;y<14;y++) {
    if(negative==0)(*tiles)[index(10,y)].wallTemplateOffset=0;
    if(negative==1)(*tiles)[index(11,y)].wallTemplateOffset=0;
    if(negative==2)(*tiles)[index(10,y)].height[3]=3;
    if(negative==3)(*tiles)[index(11,y)].wallTemplateOffset=0xba6;
    // Ordinary full-height authored usage, even with the same template,
    // retains its exterior contact. No blanket "corner means closure".
    if(negative==4)for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)
     (*tiles)[index(10,y)].height[yy*8+xx]=7-xx;
    if(negative==5)(*tiles)[index(9,y)].surface=Surface::Obstacle;
   }
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->structuralConflicts>0 && map->offsets[index(10,11)]==255);
  }
 }
 std::cout<<"PASS corner cap region: transforms, authored preservation, ownership/overwrite/relief rejection\n";
}
