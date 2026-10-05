#include "remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.previousRoomIndex=8;
 c.placementBackgroundIndex=0;c.linkPlane=1;c.linkX=80;c.linkY=96;
 c.collisionAttributes=attr->data();c.placements=tiles->data();
 c.stairCount=1;c.stairs[0].x=10;c.stairs[0].y=14;c.stairs[0].changesPlane=true;
 auto build=[&](bool reverse,int slope,bool flip) {
  attr->fill(1);tiles->fill({});c.stairs[0].highIsNorth=reverse;
  for(int side=0;side<2;side++)for(int y=(side?18:12);y<(side?21:15);y++)for(int x=8;x<18;x++) {
   const int p=(side==int(reverse))?1:0;
   (*attr)[p*4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  }
  for(int y=15;y<18;y++)for(int x=8;x<18;x++) {
   auto &t=(*tiles)[y*64+x];t.surface=S::Wall;t.hasHeight=true;t.wallTemplateOffset=0xc26;t.vflip=flip;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=slope*((reverse!=flip)?7-yy:yy);
  }
 };
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 const int low=4096+13*64+10,high=19*64+10;
 for(bool reverse:{false,true})for(int slope:{1,2})for(bool flip:{false,true}) {
  build(reverse,slope,flip);assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
  const int l=reverse?4096+19*64+10:low,h=reverse?13*64+10:high;
  assert(m->planeOffsets[1][l%4096]==0 && m->planeOffsets[0][h]==24*slope);
  assert(m->configuredStairFallbacks==0 && m->sharedStructuralFaces==30);
  assert(m->authoredSupportRelations.size()==1 && m->authoredSupportRelations[0].rise==24*slope);
  assert(m->structuralConflicts==0);
 }
 build(false,1,false);S9xRemasterBuildDungeonHeightMap(c,50,*m);
 auto relations=m->authoredSupportRelations;
 // Other BG has no visible wall path: transfer a signed support relationship,
 // never copy structural heights or merge same-coordinate overlapping planes.
 c.placementBackgroundIndex=1;c.supportRelations=&relations;
 for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*tiles)[y*64+x]={};
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->planeOffsets[0][high]==24);
 assert(m->configuredStairFallbacks==0 && m->planeOffsets[1][high]!=m->planeOffsets[0][high]);
 RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
 RemasterFrameTileInstance bg;bg.source=RemasterSourceType::Background;bg.sourceIndex=1;
 f.tileInstances.push_back(bg);for(auto &px:f.mainPixels)px.instanceId=1;
 c.backgroundScrollX=80;c.backgroundScrollY=19*8;S9xRemasterApplyDungeonHeightMap(f,c,*m);
 assert(f.tileInstances[0].heightOffset==24);
 c.entrySupport={low,12,8,123,456,S9xRemasterDungeonEntryStateKey(c,50)};assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->entrySupportConsumed && m->planeOffsets[1][low%4096]==12 && m->planeOffsets[0][high]==36);
 (*tiles)[high].tileHash++;S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(!m->entrySupportConsumed);
 (*tiles)[high].tileHash--;
 S9xRemasterBuildDungeonHeightMap(c,51,*m);assert(!m->entrySupportConsumed);
 const auto key=S9xRemasterDungeonHeightInputKey(c,50);
 c.entrySupport.predecessorState=0;assert(S9xRemasterDungeonHeightInputKey(c,50)!=key);
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(!m->entrySupportConsumed && m->planeOffsets[0][high]==24);
 c.entrySupport={low,12,9,123,456};S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(!m->entrySupportConsumed);
 c.entrySupport={8191,12,8,123,456};S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(!m->entrySupportConsumed);
 c.entrySupport={};c.supportRelations=nullptr;c.placementBackgroundIndex=0;
 build(false,1,false);
 // Overwrite/ambiguous paired art and nonblocking underpass cannot identify a
 // shared face. Without that evidence retain the explicit configured fallback.
 for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*tiles)[y*64+x].distinctLowerUsage=true;
 S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(m->sharedStructuralFaces==0 && m->configuredStairFallbacks==1);
 build(false,1,false);for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*attr)[4096+y*64+x]=0;
 S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(m->sharedStructuralFaces==0);
 build(false,1,false);c.graphicsBase=0x4000;c.bg1GraphicsBase=0x6000;
 S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(m->sharedStructuralFaces==0);
 c.bg1GraphicsBase=c.graphicsBase;
 // Conflicting paths are not resolved by a datum or configured rise.
 build(false,1,false);for(auto &h:(*tiles)[16*64+10].height)h*=2;
 S9xRemasterBuildDungeonHeightMap(c,50,*m);assert(m->structuralConflicts>0 && m->authoredSupportRelations.empty());
 std::cout<<"PASS shared authored faces, variable/signed/transformed rise, cross-BG relationships, explicit entry pins, ambiguity/underpass/conflict rejection\n";
}
