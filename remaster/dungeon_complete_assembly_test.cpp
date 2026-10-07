// Complete column / floor-torch provenance: roles contextual, relief immutable.
#include "remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using S=RemasterDungeonFloorContext::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterProfile profile;std::array<RemasterTileContentId,12> ids;
 for(int n=0;n<12;n++) {
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*gfx)[n*32+(b/2)*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[ids[n]];
  a.hasHeight=true;for(int k=0;k<64;k++)a.height[k]=7-k%8;
 }
 auto put=[&](int i,int word){(*maps)[i*2]=word;(*maps)[i*2+1]=word>>8;};
 auto source=[&](int off,int n,int word){(*tpl)[off+n*2]=word;(*tpl)[off+n*2+1]=word>>8;};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();
 for(int flip=0;flip<4;flip++) {
  maps->fill(0xff);tpl->fill(0xff);profile.assetGroups.clear();
  profile.assetGroups["wall_faces"].tileIds={ids[0],ids[8]};
  for(int n=0;n<12;n++){int word=n|(flip<<14);source(0x89a,n,word);put((20+n%4)*64+20+n/4,word);}
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+21].surface==S::Wall && (*tiles)[23*64+21].surface==S::Wall);
  assert((*tiles)[20*64+21].height==profile.assets[ids[4]].height);
  put(23*64+22,999);S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+21].surface==S::Unknown); // overwritten endpoint
  put(23*64+22,11|(flip<<14));profile.assets[ids[4]].tileRole=RemasterTileRole::Floor;
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[23*64+21].surface==S::Unknown);
  profile.assets[ids[4]].tileRole=RemasterTileRole::Unknown;
  auto saved=profile.assets[ids[4]].height;profile.assets[ids[4]].height.fill(0);
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[20*64+21].surface==S::Unknown);
  profile.assets[ids[4]].height=saved; // placeholder local arrays cannot own ramps
  profile.assetGroups["obstacles"].tileIds={ids[4]};S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+21].surface==S::Obstacle && (*tiles)[23*64+21].surface==S::Unknown);
  profile.assetGroups.erase("obstacles");
  profile.assets[ids[4]].hasHeight=false;S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[23*64+21].surface==S::Unknown);profile.assets[ids[4]].hasHeight=true;
  profile.assetGroups.clear();S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+21].surface==S::Unknown); // no original wall seed
  maps->fill(0xff);tpl->fill(0xff);
  for(int n=0;n<4;n++){int word=n|(flip<<14);source(0xeca,n,word);put((30+n%2)*64+30+n/2,word);}
  profile.assetGroups["obstacles"].tileIds={ids[1],ids[3]};
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[30*64+30].surface==S::Obstacle);
  assert((*tiles)[30*64+30].height==profile.assets[ids[0]].height);
  put(31*64+31,999);S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[30*64+30].surface==S::Unknown);
  // A matching footprint solely in the other background cannot own this BG.
  for(int n=0;n<4;n++)put(4096+(30+n%2)*64+30+n/2,n|(flip<<14));
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[30*64+30].surface==S::Unknown);
 }
 // Original low-corner triangle witnesses both final branches, even where
 // the visible BG interior is covered. Both branches/flips/context required.
 for(int kind=0;kind<4;kind++) {
  maps->fill(0xff);tpl->fill(0xff);profile.assetGroups.clear();
  const int off=0xbe6+kind*0x20,flip=(kind>=2?1:0)|(kind%2?2:0);
  profile.assets[ids[0]].height={};for(int y=0;y<8;y++)for(int x=0;x<8;x++)profile.assets[ids[0]].height[y*8+x]=std::max(7-x,7-y);
  profile.assetGroups["wall_faces"].tileIds={ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[9]};
  for(int yy=0;yy<3;yy++)for(int xx=0;xx<4-yy;xx++) {
   const int tx=kind>=2?3-xx:xx,ty=kind%2?3-yy:yy,n=xx*3+yy,word=n|(flip<<14);
   source(off,tx*4+ty,word);put((20+ty)*64+20+tx,word);put(4096+(20+ty)*64+20+tx,word);
  }
  const int cx=20+(kind>=2?3:0),cy=20+(kind%2?3:0),corner=cy*64+cx;
  c.placementBackgroundIndex=1;c.bg1GraphicsBase=c.graphicsBase=0;
  S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[corner].terminalCorner);
  // Overwritten interior is allowed only with complete original counterpart.
  const int inside=(cy+(kind%2?-1:1))*64+cx+(kind>=2?-1:1);
  put(inside,999);S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[corner].terminalCorner);
  c.bg1GraphicsBase=32;S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!(*tiles)[corner].terminalCorner);c.bg1GraphicsBase=0;
  put(4096+inside,999);S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!(*tiles)[corner].terminalCorner);
 }
 std::cout<<"PASS complete columns/torch transforms, overwrites, floor veto, absent witnesses, BG ownership, immutable relief\n";
}
