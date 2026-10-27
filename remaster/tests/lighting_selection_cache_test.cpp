#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
static void equal(const RemasterLightingFieldCache&a,const RemasterLightingFieldCache&b){
 need(a.height==b.height&&a.surface==b.surface&&a.occlusion==b.occlusion&&a.emission==b.emission&&a.participation==b.participation&&a.opposite==b.opposite&&a.authoredNormals==b.authoredNormals&&a.reflectance==b.reflectance&&a.depths==b.depths&&a.omnidirectional==b.omnidirectional,"all fields match forced consumer");
 for(size_t k=0;k<a.channelSources.size();k++)for(size_t c=0;c<7;c++)need(a.channelSources[k].donors[c].instanceId==b.channelSources[k].donors[c].instanceId,"source parity");
}
int main(){
 for(int contextual=0;contextual<2;contextual++)for(int bg=0;bg<2;bg++)for(int hv=0;hv<4;hv++)for(int mutation=0;mutation<29;mutation++){
  RemasterFrame f;f.width=8;f.height=8;f.originalRgb555.resize(64);f.sceneRgb555.resize(64);
  for(int j=0;j<2;j++){
   RemasterFrameTileInstance i;i.tileId={uint64_t(10+j),1,4};i.source=RemasterSourceType::Background;i.sourceIndex=j^bg;i.hFlip=hv&1;i.vFlip=hv&2;
   f.tileInstances.push_back(i);RemasterFrameAssetMetadata m;m.tileId=i.tileId;
   m.hasHeight=m.hasNormals=m.hasOcclusion=j;m.height.fill(0);m.normalXyz.fill(128);m.occlusion.fill(0);f.assetMetadata.push_back(m);
  }
  for(int k=0;k<64;k++){
   RemasterFramePixel a{uint32_t(RemasterSourceType::Background)<<24|uint32_t(bg)<<16,1,uint8_t(k)},b{uint32_t(RemasterSourceType::Background)<<24|uint32_t(1-bg)<<16,2,uint8_t(k)};
   f.mainPixels.push_back(a);f.scenePixels.push_back(a);f.subPixels.push_back(b);
  }
  if(contextual){auto &c=f.contextGeometry[2];c.hasHeight=true;c.height.fill(0);c.hasNormals=true;c.normalXyz.fill(128);}
  RemasterLightingFieldCache cached,full;cached.update(f,true);
  cached.update(f,true);need(cached.selectedPixels==0 && cached.resolvedInstances==0,"unchanged frame must not rerun channel selection or geometry resolution");
  const auto original=f;
  if(mutation==0)f.mainPixels[0]={};
  if(mutation==1)f.subPixels[0]={};
  if(mutation==2)f.scenePixels[0]=f.subPixels[0];
  if(mutation==3)f.observedBgPixels[0]=f.subPixels;
  if(mutation==4)f.observedBgDepth[0].assign(64,99);
  if(mutation==5)f.assetMetadata[0].hasHeight=true;
  if(mutation==6)f.assetMetadata[0].hasEmission=true;
  if(mutation==7){f.assetMetadata[1].hasEmission=true;f.assetMetadata[1].emissionRgba.fill(50);}
  if(mutation==8)f.tileInstances[1].heightOffset=32;
  if(mutation==9)f.tileInstances[1].hFlip=!f.tileInstances[1].hFlip;
  if(mutation==10)f.tileInstances[1].geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;
  if(mutation==11){auto &c=f.contextGeometry[2];c.hasHeight=true;c.height.fill(12);}
  if(mutation==12)f.assetMetadata[1].hasNormals=false;
  if(mutation==13)f.assetMetadata[1].hasOcclusion=false;
  if(mutation==14)f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
  if(mutation==15)f.lightingCoordinateScale=33;
  if(mutation==16)f.tileInstances[1].material="new";
  if(mutation==17){f.assetMetadata[1].hasDoorframeMask=true;f.assetMetadata[1].doorframeMask=0;}
  if(mutation==18)f.contextGeometry.clear();
  if(mutation==19){auto &c=f.contextGeometry[2];c.hasNormals=true;c.normalXyz.fill(240);}
  if(mutation==20){f.tileInstances[1].hasPlacementHeight=true;f.tileInstances[1].placementHeight.fill(-12);}
  if(mutation==21){f.assetMetadata[1].geometryByFlip=true;auto &g=f.assetMetadata[1].flipGeometry[hv];g.hasHeight=true;g.height.fill(23);}
  if(mutation==22){f.width=16;f.height=4;}
  if(mutation==23){RemasterFrameMaterial m;m.name="floor";f.materials.push_back(m);f.tileInstances[1].material="floor";}
  if(mutation==24){f.tileInstances[1].hasPaletteArtwork=true;f.tileInstances[1].paletteRgb555.fill(15);f.assetMetadata[1].hasEmission=true;f.assetMetadata[1].emissionRgba.fill(255);}
  if(mutation==25){f.assetMetadata[1].hasEmission=true;f.assetMetadata[1].emissionDepth=7;f.assetMetadata[1].emissionOmnidirectional=true;}
  if(mutation==26)f.area.id=320;
  if(mutation==27){f.tileInstances.push_back(f.tileInstances[1]);f.subPixels[0].instanceId=3;}
  if(mutation==28){f.originalRgb555[0]=42;f.assetMetadata[1].hasEmission=true;f.assetMetadata[1].emissionColorMask=0;}
  for(int event=0;event<3;event++){
   cached.update(f,true);full.update(f,true,true);equal(cached,full);
   f=event==0?original:f;
  }
 }
 std::cout<<"PASS selection reuse both BG orders/HV/context, authored zeros, 29 dependency mutations and switchback\n";
}
