#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.placementBackgroundIndex=0;c.linkPlane=1;
 c.constructedMaps=words->data();c.collisionAttributes=attr->data();c.placements=tiles->data();c.dungeonTemplateData=tpl->data();
 c.stairCount=1;c.stairs[0]={14,20,0,0,true,true};c.linkX=112;c.linkY=184;
 for(int x=0;x<4;x++)for(int y=0;y<4;y++){int n=1+x*4+y;(*tpl)[0x10c8+(x*4+y)*2]=n;}
 auto build=[&](bool north,int rise) {
  tiles->fill({});words->fill(0);attr->fill(1);c.stairs[0].highIsNorth=north;
  for(int p=0;p<2;p++)for(int y=(p==0)==north?14:24;y<((p==0)==north?20:30);y++)for(int x=10;x<22;x++) {
   (*attr)[p*4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  }
  for(int x=0;x<4;x++)for(int y=0;y<4;y++) {
   const int i=(20+y)*64+14+x,n=1+x*4+(north?y:3-y),w=n|(north?0:0x8000)|0x2000;
   (*words)[i*2]=w;(*words)[i*2+1]=w>>8;auto &t=(*tiles)[i];t.surface=S::Stair;t.hasHeight=true;t.vflip=!north;
   // Canonical array must be independent of the placement's V transform.
   for(int py=0;py<8;py++)for(int px=0;px<8;px++)t.height[py*8+px]=std::max(0,rise-1-((north?y:3-y)*8+py)*rise/32);
  }
 };
 for(bool north:{true,false})for(int rise:{24,32}) {
  build(north,rise);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  need(map->configuredStairFallbacks==0 && map->authoredSupportRelations.size()==1 && map->authoredSupportRelations[0].rise==rise,"ROM rail signed rise");
  auto relation=map->authoredSupportRelations;
  // BG2 overwrite removes the template, but exact endpoint components consume
  // the other BG's pre-datum signed relationship, never a copied base.
  words->fill(0);c.placementBackgroundIndex=1;c.supportRelations=&relation;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->configuredStairFallbacks==0,"cross BG transport");
  c.supportRelations=nullptr;c.placementBackgroundIndex=0;
  build(north,rise);(*words)[(20*64+14)*2]^=32;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->configuredStairFallbacks==1,"partial template accepted");
  build(north,rise);(*tiles)[20*64+14].height[7]++;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->configuredStairFallbacks==1,"nonuniform rail accepted");
  build(north,rise);(*tiles)[20*64+14].hasHeight=false;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->configuredStairFallbacks==1,"missing source accepted");
  build(north,rise);(*tiles)[20*64+14].surface=S::Obstacle;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->configuredStairFallbacks==1,"explicit incompatible role accepted");
 }
 // Unsupported floor-art chain joining two body components must not defeat
 // their signed wall path. Same unique-owner flat continuation remains legal.
 c.dungeonTemplateData=nullptr;c.stairCount=0;c.placementBackgroundIndex=1;c.linkPlane=0;
 tiles->fill({});attr->fill(1);
 for(int y=10;y<18;y++)for(int x=6;x<12;x++){(*attr)[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;}
 for(int y=10;y<18;y++)for(int x=16;x<22;x++){(*attr)[y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;}
 for(int x=12;x<16;x++){auto &t=(*tiles)[17*64+x];t.surface=S::Floor;t.hasHeight=true;}
 for(int x=12;x<16;x++){auto &t=(*tiles)[12*64+x];t.surface=S::Wall;t.hasHeight=true;for(int k=0;k<64;k++)t.height[k]=7-k%8;}
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->geometryConflicts.empty(),"unoccupied floor bridged components");
 need(map->planeOffsets[0][12*64+10]==32 && map->planeOffsets[0][12*64+18]==0,"signed wall relation lost");
 // One-owner border still attaches without filling a second support graph.
 (*tiles)[17*64+15]={};
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[17*64+12]==32,"unique semantic continuation rejected");
 // Agreeing same-height independent supports keep their legitimate flat seam.
 (*tiles)[17*64+15].surface=S::Floor;(*tiles)[17*64+15].hasHeight=true;
 for(int x=12;x<16;x++)(*tiles)[12*64+x]={};
 need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->geometryConflicts.empty() && map->offsets[17*64+12]==0,"same-level flat seam lost");
 std::cout<<"PASS complete ROM stairs north/south rise24/32, priority overwrite, cross-BG signed transport; partial/missing/nonuniform rejection; configured50 preserved\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
