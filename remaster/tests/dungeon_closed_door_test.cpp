// A closed final dispatch owns its leaves AND its cap; open cutouts do not.
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
 RemasterTileContentId frameId{S9xRemasterHashTile(4,pixels),1,4};
 pixels[0]=1;(*gfx)[32]=128;RemasterTileContentId leafId{S9xRemasterHashTile(4,pixels),1,4};
 auto &frame=p.assets[frameId];frame.tileId=frameId;frame.tileRole=RemasterTileRole::Doorway;
 frame.hasHeight=frame.hasDoorframeMask=true;frame.doorframeMask=UINT64_C(0xffffffffff000000);
 auto &leaf=p.assets[leafId];leaf.tileId=leafId;leaf.tileRole=RemasterTileRole::Door;leaf.hasHeight=true;
 C c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++) {
  const int starts[]={0x2716,0x29f0,0x2c6a,0x2e6a},off=starts[d],w=d<2?4:3,h=d<2?3:4;
  tpl->fill(255);maps->fill(255);
  auto cell=[&](int x,int y){return (10+(flip&2?h-1-y:y))*64+10+(flip&1?w-1-x:x);};
  for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
   bool interior=d<2?(x==1 || x==2) && y!=(d?h-1:0):(y==1 || y==2) && x!=(d==2?0:w-1);
   int n=off+2*(x*h+y),word=interior?1:0,i=cell(x,y);
   (*tpl)[n]=word;(*tpl)[n+1]=0;(*maps)[2*i]=word;(*maps)[2*i+1]=flip<<6;
  }
  S9xRemasterDungeonPlacements(c,p,*tiles);
  for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
   const auto &t=(*tiles)[cell(x,y)];
   assert(t.doorAssembly==10*64+11 && t.closedDoorAssembly);
   assert(t.height==((t.surface==S::ClosedDoor)?leaf.height:frame.height));
  }
  // Any overwritten final member invalidates the closed barrier contract.
  (*maps)[2*cell(1,1)]=2;S9xRemasterDungeonPlacements(c,p,*tiles);
  for(const auto &t:*tiles)assert(!t.closedDoorAssembly);
  (*maps)[2*cell(1,1)]=1;leaf.geometryByFlip=true;
  S9xRemasterDungeonPlacements(c,p,*tiles);for(const auto &t:*tiles)assert(!t.closedDoorAssembly);
  leaf.geometryByFlip=false;
  leaf.tileRole=RemasterTileRole::Floor;S9xRemasterDungeonPlacements(c,p,*tiles);
  for(const auto &t:*tiles)assert(!t.closedDoorAssembly);leaf.tileRole=RemasterTileRole::Door;
 }
 // The solver and real consumer must not flatten closed zero relief or use
 // the Doorway mask on a cap belonging to the final closed dispatch.
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);tiles->fill({});
 c.placements=tiles->data();c.collisionAttributes=attrs->data();c.linkX=88;c.linkY=48;
 for(int y=3;y<10;y++)for(int x=8;x<16;x++) {auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;}
 for(int y=10;y<13;y++)for(int x=10;x<14;x++) {
  auto &t=(*tiles)[y*64+x];t.surface=x==11 || x==12?S::ClosedDoor:S::Door;
  t.closedDoorAssembly=true;t.hasHeight=true;t.doorAssembly=10*64+11;t.doorDirection=1;t.doorTemplateOffset=0x29f0;
  t.hasDoorframeMask=true;t.doorframeMask=UINT64_C(0xffffffffff000000);
  if(y==10 && x==10){t.doorwaySupport=true;t.doorwaySupportDirection=RemasterSupportDirection::North;}
 }
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->doorframeAttachments.size()==12);
 for(const auto &a:map->doorframeAttachments) {
  assert(a.mask==UINT64_MAX && a.base==8*(a.cell/64-10));
  RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;f.tileInstances.push_back(i);
  RemasterFrameAssetMetadata md;md.tileRole=(*tiles)[a.cell].surface==S::ClosedDoor?RemasterTileRole::Door:RemasterTileRole::Doorway;
  md.hasHeight=true;md.height.fill(0);f.assetMetadata.push_back(md);
  for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
  auto apply=c;apply.backgroundScrollX=a.cell%64*8;apply.backgroundScrollY=a.cell/64*8;apply.constructedMaps=nullptr;
  S9xRemasterApplyDungeonHeightMap(f,apply,*map);
  for(const auto &pixel:f.mainPixels)assert(f.tileInstances[pixel.instanceId-1].heightOffset==a.base);
 }
 // An authored zero in an otherwise prephased closed cap is relief, not a
 // legacy open cutout. Do not subtract a phase from this ambiguous source.
 auto &cap=(*tiles)[12*64+11];cap.height.fill(16);cap.height[0]=0;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
 for(const auto &a:map->doorframeAttachments)if(a.cell==12*64+11)assert(a.base==16 && a.mask==UINT64_MAX);
 std::cout<<"PASS final closed dispatch: all axes/HV, full members, overwrite, alternate source and open negatives\n";
}
