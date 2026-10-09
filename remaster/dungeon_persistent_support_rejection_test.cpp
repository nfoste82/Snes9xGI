#include "remaster.h"
#include <cassert>
#include <iostream>
int main(){using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto c=std::make_unique<C>();auto t=std::make_unique<std::array<C::Placement,4096>>();auto a=std::make_unique<std::array<uint8_t,8192>>();
 c->verifiedAlttpRom=c->indoors=true;c->roomIndex=7;c->linkX=80;c->linkY=80;c->placements=t->data();c->collisionAttributes=a->data();
 auto m=std::make_unique<RemasterDungeonHeightMap>();auto baseline=std::make_unique<RemasterDungeonHeightMap>();
 for(bool incompatible:{false,true}) {
  a->fill(1);t->fill({});
  // Two legal ordinary regions separated by a 16px indexed pot strip.
  for(int y=10;y<20;y++)for(int x=10;x<22;x++)if(x<15 || x>16){int i=y*64+x;(*a)[i]=0;(*t)[i].surface=S::Floor;(*t)[i].hasHeight=true;}
  if(incompatible)for(int y=10;y<20;y++)for(int x=18;x<22;x++)(*t)[y*64+x].height.fill(8);
  for(int y=10;y<20;y++)for(int x=15;x<17;x++){int i=y*64+x;(*a)[i]=0x70;(*t)[i].surface=S::Unknown;(*t)[i].hasHeight=true;}
  assert(S9xRemasterBuildDungeonHeightMap(*c,50,*baseline));
  for(int y=10;y<20;y++)for(int x=15;x<17;x++){auto &v=(*t)[y*64+x];v.surface=S::Obstacle;v.removableSupportPlanes=1;}
  assert(S9xRemasterBuildDungeonHeightMap(*c,50,*m));
  assert(m->requiredPlaneCells==baseline->requiredPlaneCells && m->unknownPlaneCells==baseline->unknownPlaneCells);
  assert(m->structuralConflicts==baseline->structuralConflicts && m->componentCount==baseline->componentCount);
  assert(m->persistentSupportRequired[0]>0 && m->persistentSupportMissing[0]>0); // body touching two owners stays unresolved
  for(int y=10;y<20;y++)for(int x=10;x<22;x++)if(x<15 || x>16)assert(m->planeOffsets[0][y*64+x]==baseline->planeOffsets[0][y*64+x]);
 }
 std::cout<<"persistent body dual-owner rejection / no component merge / no constraints or fallback corruption PASS\n";
}
