#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 uint8_t pixels[64]={};RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};RemasterProfile profile;auto &a=profile.assets[id];a.tileId=id;a.hasHeight=true;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();c.collisionAttributes=attr->data();
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++) {
  const int starts[]={0x2716,0x29f0,0x2c6a,0x2e6a},w=d<2?4:3,h=d<2?3:4,off=starts[d];
  tpl->fill(255);maps->fill(255);attr->fill(1);a.hasDoorframeMask=false;a.tileRole=RemasterTileRole::Unknown;
  for(int k=0;k<64;k++)a.height[k]=16+7-(d<2?k/8:k%8);
  for(int x=0;x<w;x++)for(int y=0;y<h;y++){int n=off+2*(x*h+y),i=(10+y)*64+10+x;(*tpl)[n]=(*maps)[2*i]=0;(*tpl)[n+1]=flip<<6;(*maps)[2*i+1]=(flip<<6)|0x20;}
  (*attr)[11*64+11]=0x80+(d>=2);
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[10*64+10].doorframeMask==UINT64_MAX && (*tiles)[10*64+10].doorDirection==d);
  const int middle=11*64+11;assert(!(*tiles)[middle].hasDoorframeMask); // opening never promoted
  const auto authored=a.height;
  (*maps)[2*(10*64+10)]^=1;S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!(*tiles)[10*64+10].doorframeMask);
  (*maps)[2*(10*64+10)]^=1;a.hasDoorframeMask=true;a.doorframeMask=0;a.tileRole=RemasterTileRole::Door;
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!(*tiles)[10*64+10].doorframeMask); // explicit empty is respected
  a.hasDoorframeMask=false;a.height[0]++;S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!(*tiles)[10*64+10].doorframeMask);a.height=authored;
  // Transparent zero-height opening columns are excluded, never raised.
  a.tileRole=RemasterTileRole::Door;a.hasOcclusion=true;uint64_t mask=0;
  for(int k=0;k<64;k++){bool ink=d<2?k%8<6:k/8<6;a.occlusion[k]=ink?255:0;if(!ink)a.height[k]=0;else mask|=UINT64_C(1)<<k;}
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[10*64+10].doorframeMask==mask);
  a.hasOcclusion=false;a.height=authored;
 }
 std::cout<<"door part coordinates / all axes-flips / overwritten / opening / explicit empty / authored mismatch PASS\n";
}
