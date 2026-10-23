#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto c=std::make_unique<C>();auto t=std::make_unique<std::array<C::Placement,4096>>();auto a=std::make_unique<std::array<uint8_t,8192>>();auto m=std::make_unique<RemasterDungeonHeightMap>();
 c->verifiedAlttpRom=c->indoors=true;c->roomIndex=7;c->linkX=c->linkY=80;c->placements=t->data();c->collisionAttributes=a->data();
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++) {
  a->fill(1);t->fill({});int x=24,y=24,dx=d==2?1:d==3?-1:0,dy=d==0?1:d==1?-1:0;
  int ox=d==0?x:d==1?x:d==2?x-2:x,oy=d==0?y-2:d==1?y:y;
  for(int yy=20;yy<=28;yy++)for(int xx=20;xx<=28;xx++)if((dx && (xx-x)*dx>0) || (dy && (yy-y)*dy>0)){int i=yy*64+xx;(*a)[i]=0;(*t)[i].hasHeight=true;(*t)[i].surface=S::Floor;}
  const int cell=y*64+x;auto &door=(*t)[cell];door.surface=S::Door;door.hasHeight=door.hasDoorframeMask=true;door.doorframeMask=UINT64_MAX;
  door.doorAssembly=oy*64+ox+1;door.doorDirection=d;door.doorTemplateOffset=d<2?(d==0?0x2716:0x29f0):(d==2?0x2c6a:0x2e6a);door.hflip=flip&1;door.vflip=flip&2;
  for(int py=0;py<8;py++)for(int px=0;px<8;px++){int k=(door.vflip?7-py:py)*8+(door.hflip?7-px:px);door.height[k]=dx?(dx>0?7-px:px):(dy>0?7-py:py);}
  assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));assert(m->doorframeAttachments.size()==1 && m->doorframeAttachments[0].base==0 && m->doorframeAttachments[0].visible);
  assert(m->doorframeAttachments[0].openingBase==0);
  auto saved=door;door.doorAssembly++;assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));
  // Axial origin changes cannot match the innermost foot part.
  door=saved;if(d<2)door.doorAssembly+=64;else door.doorAssembly++;
  assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));assert(m->doorframeAttachments.empty());door=saved;
  const int neighbor=(y+dy)*64+x+dx;(*t)[neighbor].height.fill(1);
  assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));assert(m->doorframeAttachments.empty());(*t)[neighbor].height.fill(0);
  (*a)[neighbor]=0x70;assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));assert(m->doorframeAttachments.empty());
 }
 std::cout<<"door innermost zero-foot assembly contact all axes/flips / wrong part / nonzero seam / special blocker rejection PASS\n";
}
