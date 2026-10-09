#include "remaster.h"
#include <cassert>
#include <iostream>
// Same movement component is support evidence, NOT a relation between visible
// obstacle relief and an adjacent ceiling's independently authored local60.
int main(){
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 for(int plane:{0,1})for(int local:{18,37,60})for(bool hf:{false,true})for(bool vf:{false,true}) {
  attr->fill(1);tiles->fill({});C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
  c.linkPlane=plane;c.linkX=80;c.linkY=80;c.collisionAttributes=attr->data();c.placements=tiles->data();
  for(int y=10;y<18;y++)for(int x=10;x<18;x++){
   (*attr)[plane*4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;t.height.fill(local);
  }
  auto &ob=(*tiles)[13*64+13];ob.surface=S::Obstacle;ob.hflip=hf;ob.vflip=vf;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)ob.height[y*8+x]=(x+y)%8;
  // Unlike support offset, a nonblocking ceiling/floor usage can have a
  // nonzero authored array. The support itself remains the reproduced node.
  auto preserved=ob.height;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
  assert(m->unknownPlaneCells[plane]==0 && m->geometryConflicts.empty());
  assert(m->planeOffsets[plane][13*64+13]==0 && ob.height==preserved);
  assert(m->planeOffsets[1-plane][13*64+13]==255);
  // Ordinary incompatible floor-to-floor locals are NOT obstacles. Their
  // intrinsic conflict remains; don't globally suppress self-edge conflicts.
  ob.surface=S::Floor;ob.height.fill(0);
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
  assert(!m->geometryConflicts.empty() && m->unknownPlaneCells[plane]>0);
 }
 std::cout<<"PASS48 same-component obstacle/ceiling relief contacts, both planes/all flips, local18/37/60 invariance; ordinary floor conflict retained\n";
}
