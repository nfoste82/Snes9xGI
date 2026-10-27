// A corner's contacting face, not its unrelated perpendicular face, backs a mount.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 C c;c.verifiedAlttpRom=true;c.constructedMaps=maps->data();c.dungeonTemplateData=tpl->data();c.placements=tiles->data();
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  maps->fill(255);tpl->fill(255);tiles->fill({});c.placementBackgroundIndex=bg;
  auto m=std::make_unique<RemasterDungeonHeightMap>();
  for(auto &p:m->structuralPlaneOffsets)p.fill(255);m->offsets.fill(255);
  const int off=axis?0xa34:0xa14,w=axis?2:4,h=axis?4:2,x=10,y=10;
  for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++) {
   int sx=flip&1?w-1-xx:xx,sy=flip&2?h-1-yy:yy,n=off+2*(axis?sx*h+sy:sy*w+sx),i=(y+yy)*64+x+xx;
   int word=1+sy*w+sx;(*tpl)[n]=word;(*tpl)[n+1]=0;word^=(flip&1?0x4000:0)|(flip&2?0x8000:0);
   (*maps)[2*i]=word;(*maps)[2*i+1]=word>>8;
   auto &t=(*tiles)[i];t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int py=0;py<8;py++)for(int px=0;px<8;px++)t.height[(t.vflip?7-py:py)*8+(t.hflip?7-px:px)]=7-(axis?px:py);
  }
  for(int band=0;band<2;band++)for(int side=0;side<2;side++) {
   int i=axis?(y+(side?h:-1))*64+x+band:(y+band)*64+x+(side?w:-1);
   auto &t=(*tiles)[i];t.surface=S::Wall;t.hasHeight=true;
   for(int py=0;py<8;py++)for(int px=0;px<8;px++) {
    int normal=7-(axis?px:py),tangent=axis?(side?py:7-py):(side?px:7-px);
    t.height[py*8+px]=std::max(normal,tangent); // contacting edge has tangent=0
   }
   m->offsets[i]=24+8*band;m->structuralPlaneOffsets[1][i]=24+8*band;
  }
  const auto original=*tiles;
  const auto before=*m;
  S9xRemasterAttachWallMounts(c,*m);
  for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++) {
   int i=(y+yy)*64+x+xx;assert(m->offsets[i]==24+8*(axis?xx:yy));
   assert((*tiles)[i].height==original[i].height);
  }
  // Independent lower support cannot select the max/nearest plane. A second
  // valid backing pair with a different datum makes the whole claim ambiguous.
  *m=before;
  for(int band=0;band<2;band++)for(int side=0;side<2;side++) {
   int i=axis?(y+(side?h:-1))*64+x+band:(y+band)*64+x+(side?w:-1);
   m->structuralPlaneOffsets[0][i]=74+8*band;
  }
  S9xRemasterAttachWallMounts(c,*m);assert(m->offsets[y*64+x]==255);
  // Both compatible ends are required on one plane; do not combine planes.
  *m=before;
  for(int band=0;band<2;band++) {
   int i=axis?(y+h)*64+x+band:(y+band)*64+x+w;
   m->structuralPlaneOffsets[1][i]=255;m->structuralPlaneOffsets[0][i]=24+8*band;
  }
  S9xRemasterAttachWallMounts(c,*m);assert(m->offsets[y*64+x]==255);
  *m=before;(*tiles)[y*64+x].hasHeight=false;
  S9xRemasterAttachWallMounts(c,*m);assert(m->offsets[y*64+x]==255);*tiles=original;
  *m=before;
  int end=axis?(y+h)*64+x:y*64+x+w;(*tiles)[end].height.fill(0);
  S9xRemasterAttachWallMounts(c,*m);assert(m->offsets[y*64+x]==255);*tiles=original;
  // A changed final word invalidates the complete footprint; equality cannot
  // attest ordered history or detect a same-value rewrite (explicit limit).
  (*maps)[2*(y*64+x)]^=16;*m=before;
  S9xRemasterAttachWallMounts(c,*m);assert(m->offsets[y*64+x]==255);
 }
 std::cout<<"PASS wall mount contact faces: axes/HV/BGs and overwritten footprint\n";
}
