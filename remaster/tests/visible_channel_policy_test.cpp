// Game-independent visible-first per-channel donors, not BG2-first admission.
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 for(unsigned order=0;order<2;order++)for(unsigned flags=0;flags<64;flags++)for(unsigned hv=0;hv<4;hv++){
  RemasterFrame f;f.width=1;f.height=1;f.originalRgb555={17};f.sceneRgb555={19};
  for(unsigned j=0;j<2;j++){
   RemasterFrameTileInstance i;i.tileId={100+j,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=j;i.heightOffset=j?40:-8;i.hFlip=hv&1;i.vFlip=hv&2;f.tileInstances.push_back(i);
   RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.hasHeight=j?true:bool(flags&1);m.height.fill(j?7:0);
   m.hasNormals=j?true:bool(flags&2);for(unsigned k=0;k<64;k++){m.normalXyz[k*3]=j?255:128;m.normalXyz[k*3+1]=128;m.normalXyz[k*3+2]=255;}
   m.hasOcclusion=j?true:bool(flags&4);m.occlusion.fill(j?123:0);
   m.hasEmission=j?true:bool(flags&8);m.emissionRgba.fill(j?99:0);
   m.hasMaterialSelectors=j?true:bool(flags&16);m.materialSelectors.fill(j?"donor":"visible");
   m.hasDoorframeMask=j?true:bool(flags&32);m.doorframeMask=j?UINT64_MAX:0;f.assetMetadata.push_back(m);
  }
  // Swap physical BG indices, not the observed PPU ordering.
  f.tileInstances[0].sourceIndex=order;f.tileInstances[1].sourceIndex=1-order;
  RemasterFramePixel a{uint32_t(RemasterSourceType::Background)<<24|order<<16,1,13};
  RemasterFramePixel b{uint32_t(RemasterSourceType::Background)<<24|(1-order)<<16,2,42};
  f.mainPixels={a};f.subPixels={b};f.scenePixels={b};
  RemasterLightingFieldCache cpu;cpu.update(f,true);
  need(cpu.height[1] && cpu.height[0]==S9xRemasterSignedHeightField(flags&1?-8:40,flags&1?0:7),"visible authored zero wins; missing height uses other contextual base");
  need(cpu.authoredNormals[0],"normal presence independent of height");
  need(cpu.occlusion[0]==(flags&4?0:123),"occlusion zero is data");
  need(cpu.emission[0]==(flags&8?0:99),"emission zero is data");
  const auto &sources=cpu.channelSources[0];
  for(unsigned c=0;c<6;c++)need(sources.donors[c].instanceId==((flags&(1<<c))?1:2),"independent exact donor provenance including mask/material");
  need(sources.donors[0].tilePixel==((flags&1)?13:42),"canonical index travels with donor; never copy into visible UV");
  need((cpu.surface[1]<0)==bool(hv&1),"normal uses its donor transform in common world vector frame");
  // Independent normal vectors have no scalar base. They are transformed from
  // their own occurrence into world axes, NOT interpreted in the height donor's
  // local frame. Height is scalar base+local from ONE donor, even unequal bases.
  auto old=cpu.height[0];f.tileInstances[1].heightOffset+=9;cpu.update(f,true);
  need(cpu.height[0]==old+((flags&1)?0:9/255.0f),"fallback donor base invalidates cached scalar height");
  f.tileInstances[1].heightOffset-=9;
  need(f.mainPixels[0].instanceId==1 && f.scenePixels[0].instanceId==2 && f.originalRgb555[0]==17,"stock presentation unchanged");
  // No source is still absent; provisional sheet is not authored height.
  auto absent=f;for(auto &m:absent.assetMetadata)m.hasHeight=m.hasNormals=m.hasOcclusion=m.hasEmission=m.hasMaterialSelectors=m.hasDoorframeMask=false;
  cpu.update(absent,true);for(const auto &p:cpu.channelSources[0].donors)need(!p.instanceId,"all missing remains absent");
  need(!cpu.authoredNormals[0],"missing normals not authored flat");
  // OBJ and invalid canonical pixels cannot become other-layer BG donors.
  auto obj=f;obj.tileInstances[0].source=RemasterSourceType::Object;obj.mainPixels[0].owner=uint32_t(RemasterSourceType::Object)<<24;
  cpu.update(obj,true);for(unsigned c=0;c<6;c++)need(cpu.channelSources[0].donors[c].instanceId==((flags&(1<<c))?1:0),"OBJ stops BG fallback");
  auto invalid=f;invalid.subPixels[0].tilePixel=64;invalid.scenePixels[0]=invalid.subPixels[0];cpu.update(invalid,true);
  for(unsigned c=0;c<6;c++)need(cpu.channelSources[0].donors[c].instanceId==((flags&(1<<c))?1:0),"invalid alternate pixel rejected");
  // Explicit unresolved ownership is not absence permission to revive its art.
  auto conflict=f;conflict.tileInstances[1].geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;cpu.update(conflict,true);
  need(cpu.channelSources[0].donors[0].instanceId==((flags&1)?1:0),"conflicting height donor rejected");
 }
 std::cout<<"PASS visible channel policy\n";
}
