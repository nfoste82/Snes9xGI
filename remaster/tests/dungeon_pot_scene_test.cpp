#include "../runtime/remaster.h"
#include "../runtime/lighting_field_cache.h"
#include <cassert>
#include <iostream>
int main(){
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto attrs=std::make_unique<std::array<uint8_t,8192>>();auto tpl=std::make_unique<std::array<uint8_t,12876>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.collisionAttributes=attrs->data();c.dungeonTemplateData=tpl->data();c.graphics=gfx->data();c.backgroundScrollX=c.backgroundScrollY=80;
 for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++)for(int flip=0;flip<4;flip++)for(int off:{0xe82,0xe92}){
 maps->fill(255);attrs->fill(1);tpl->fill(255);gfx->fill(0);
 RemasterFrame f;f.width=f.height=16;f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 RemasterFramePixel empty;empty.owner=uint32_t(RemasterSourceType::Backdrop)<<24;f.mainPixels.assign(256,empty);f.subPixels=f.mainPixels;f.scenePixels=f.mainPixels;f.sceneRgb555.resize(256);
 for(int xx=0;xx<2;xx++)for(int yy=0;yy<2;yy++){
  int word=1+yy,n=off+2*(xx*2+yy);if(xx)word|=0x4000;(*tpl)[n]=word;(*tpl)[n+1]=word>>8;(*gfx)[(word&1023)*32]=128>>(word&1023);
  uint8_t pixels[64]={};pixels[word&1023]=1;
  RemasterFrameTileInstance inst;inst.source=RemasterSourceType::Background;inst.sourceIndex=bg;inst.tileId={S9xRemasterHashTile(4,pixels),1,4};inst.hFlip=bool(word&0x4000)!=bool(flip&1);inst.vFlip=flip&2;inst.hasPaletteArtwork=true;inst.paletteRgb555.fill(123);f.tileInstances.push_back(inst);
  RemasterFrameAssetMetadata md;md.tileId=inst.tileId;md.hasHeight=md.hasNormals=true;md.height.fill(word&1023);md.normalXyz.fill(128);if(xx==0)f.assetMetadata.push_back(md);
  int x=flip&1?1-xx:xx,y=flip&2?1-yy:yy,i=(10+y)*64+10+x;word^=flip<<14;int at=(bg?0:8192)+2*i;(*maps)[at]=word;(*maps)[at+1]=word>>8;(*attrs)[plane*4096+i]=0x73;
  for(int py=0;py<8;py++)for(int px=0;px<8;px++)f.subPixels[(y*8+py)*16+x*8+px]={uint32_t(RemasterSourceType::Background)<<24|uint32_t(bg)<<16,uint32_t(f.tileInstances.size()),uint8_t((inst.vFlip?7-py:py)*8+(inst.hFlip?7-px:px))};
 }
 const auto original=f;auto saved=*maps;auto savedAttrs=*attrs;
 // BG2 is not recovered from BG1 color-math; it already has its normal path.
 assert(S9xRemasterRestoreSceneDungeonPots(f,c)==(bg?0:256));
 if(!bg){RemasterLightingFieldCache fields;fields.update(f,true);for(int k=0;k<256;k++)assert(fields.height[k*2+1]);}
  for(int mode=0;mode<10;mode++){
  f=original;*maps=saved;*attrs=savedAttrs;
  if(mode==0)for(auto &m:f.assetMetadata)m.hasNormals=false;
  if(mode==1)(*maps)[(bg?0:8192)+2*(10*64+10)]^=16;
  if(mode==2)(*attrs)[plane*4096+10*64+10]=0x74;
  if(mode==3)for(auto &m:f.assetMetadata)m.tileRole=RemasterTileRole::Other;
  if(mode==4)c.verifiedAlttpRom=false;
  if(mode==5)f.mainPixels=f.subPixels;
  if(mode==6)for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)(*attrs)[(1-plane)*4096+(10+yy)*64+10+xx]=0x74;
  if(mode==7)for(auto &i:f.tileInstances)i.tileId.hash++;
   if(mode==8){auto bg2=f.tileInstances.front();bg2.sourceIndex=1;f.tileInstances.push_back(bg2);for(auto &p:f.mainPixels)p={uint32_t(RemasterSourceType::Background)<<24|1<<16,uint32_t(f.tileInstances.size()),0};}
   if(mode==9)for(auto &p:f.mainPixels)p={uint32_t(RemasterSourceType::Background)<<24|2<<16,0,0};
   assert(S9xRemasterRestoreSceneDungeonPots(f,c)==(!bg && (mode==0 || mode==9)?256:0));c.verifiedAlttpRom=true;
 }
 *maps=saved;*attrs=savedAttrs;
 // Missing lower normals do not suppress legitimate upper geometry or acquire
 // invented normals. Unknown metadata role and placement base remain unchanged.
 f=original;f.assetMetadata[1].hasNormals=false;
  assert(S9xRemasterRestoreSceneDungeonPots(f,c)==(bg?0:256));
 for(auto &m:f.assetMetadata)assert(m.tileRole==RemasterTileRole::Unknown);
 for(auto &i:f.tileInstances)assert(!i.hasPlacementNormals && i.heightOffset==0);
 // A selected legitimate context normal is consumed without copying it into
 // the canonical asset or generating a default for the missing lower half.
 f=original;for(auto &m:f.assetMetadata)m.hasNormals=false;
 for(size_t n=0;n<f.tileInstances.size();n++){RemasterTileContext ctx;ctx.hasHeight=ctx.hasNormals=true;ctx.height.fill(3);ctx.normalXyz.fill(128);f.contextGeometry[n+1]=ctx;}
 assert(S9xRemasterRestoreSceneDungeonPots(f,c)==(bg?0:256));
 }
 std::cout<<"PASS exact pot occurrence: BGs/planes/HV/light-dark, overwrite/ambiguity/missing normals and main receiver\n";
}
