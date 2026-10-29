// Exact independent baseline is optional; the bounded work contract is mandatory.
#define REMASTER_FIELD_COUNT_WORK 1
#include "../runtime/lighting_field_cache.h"
#ifdef REFERENCE_HEADER
#undef REMASTER_LIGHTING_FIELD_CACHE_H
#define RemasterLightingFieldCache FrozenLightingFieldCache
#include REFERENCE_HEADER
#undef RemasterLightingFieldCache
#endif
#include <cassert>
#include <cstdio>

int main()
{
 RemasterFrame f;f.width=128;f.height=64;f.lightingCoordinateScale=64;
 f.mainPixels.resize(f.width*f.height);f.originalRgb555.resize(f.mainPixels.size());
 f.tileInstances.resize(3);
 for(size_t k=0;k<3;++k){auto &i=f.tileInstances[k];i.tileId.hash=100+k;i.source=RemasterSourceType::Background;i.sourceIndex=0;i.material=k==0?"stone":k==1?"":"missing";}
 for(size_t k=0;k<f.mainPixels.size();++k){auto &p=f.mainPixels[k];p.owner=(uint32_t(RemasterSourceType::Background)<<24);p.instanceId=1+(k/64)%3;p.tilePixel=k%64;}
 auto material=[](const char *name,float r){RemasterFrameMaterial m;m.name=name;m.hasDiffuseReflectance=true;m.diffuseReflectance={{r,0.25f,0.5f}};return m;};
 f.materials={material("stone",0.1f),material("",0.2f),material("stone",0.7f)};
 for(size_t k=0;k<3;++k){RemasterFrameAssetMetadata m;m.tileId.hash=100+k;m.hasHeight=true;m.tileRole=RemasterTileRole::Floor;f.assetMetadata.push_back(m);}
 RemasterLightingFieldCache cache;
#ifdef REFERENCE_HEADER
 FrozenLightingFieldCache reference;
#endif
 for(unsigned stage=0;stage<8;++stage){
  // Repeated selectors, empty name, missing material, duplicate LAST metadata,
  // and material switchbacks all use the existing independent full consumer.
  if(stage==1){auto m=f.assetMetadata[0];m.hasMaterialSelectors=true;for(size_t k=0;k<64;++k)m.materialSelectors[k]=k%3==0?"":k%3==1?"stone":"missing";f.assetMetadata.push_back(m);}
  if(stage==2)f.materials.back().diffuseReflectance[0]=0;
  if(stage==3)f.materials.push_back(material("missing",0.9f));
  if(stage==4)f.assetMetadata.pop_back();
  if(stage==5)f.materials.pop_back();
  if(stage==6)f.materials.back().diffuseReflectance[0]=0.7f;
  cache.update(f,false);
#ifdef REFERENCE_HEADER
  reference.update(f,false);
  assert(cache.reflectance==reference.reflectance);
  assert(cache.participation==reference.participation);
  assert(cache.height==reference.height && cache.surface==reference.surface);
  assert(cache.emission==reference.emission && cache.occlusion==reference.occlusion);
  assert(cache.visualClaims==reference.visualClaims);
  for(size_t k=0;k<cache.samples.size();++k){const auto &a=cache.samples[k],&b=reference.samples[k];assert(a.height==b.height && a.domain==b.domain && a.known==b.known && a.sheet==b.sheet && a.coverage==b.coverage && a.wallBase==b.wallBase && a.solidWall==b.solidWall && a.fallbackGround==b.fallbackGround);}
#endif
  if(stage==0 || stage==6)assert(cache.reflectance[0]==0.7f); // last duplicate wins
  if(stage==0){assert(cache.reflectance[64*4+3]==0);assert(cache.reflectance[128*4+3]==0);} // empty default and missing name
  if(stage==1){assert(cache.reflectance[0]==0.2f);assert(cache.reflectance[4]==0.7f);assert(cache.reflectance[8+3]==0);} // empty selector is explicit
  if(stage==2)assert(cache.reflectance[4]==0); // valid authored zero is not absence
#ifdef REMASTER_FIELD_MATERIAL_WORK
  assert(cache.materialLookups<=64+f.tileInstances.size()*2);
#else
  assert(!"baseline repeats material tree lookups per pixel; bounded work contract absent");
#endif
 }
 std::puts("material lookup work and exact material switchbacks PASS");
}
