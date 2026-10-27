// Compare actual scene CPU channels; source identity and placement bases must not change.
#include "../runtime/lighting_field_cache.h"
#include <iostream>
int main(int argc,char **argv){
 if(argc!=3)return 2;
 auto a=std::make_unique<RemasterFrame>(),b=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(argv[1],*a)||!S9xReadRemasterFrame(argv[2],*b))return 2;
 if(a->mainPixels.size()!=b->mainPixels.size() || a->tileInstances.size()!=b->tileInstances.size())return 3;
 size_t sourceChanges=0,changed=0,recovered=0,lost=0,opaqueChanges=0,stairsChanged=0;
 for(size_t n=0;n<a->tileInstances.size();n++)if(!RemasterLightingFieldCache::instanceEqual(a->tileInstances[n],b->tileInstances[n]))sourceChanges++;
 RemasterLightingFieldCache ca,cb;ca.update(*a,true);cb.update(*b,true);
 auto equal=[](const RemasterFramePixel &x,const RemasterFramePixel &y){return x.owner==y.owner && x.instanceId==y.instanceId && x.tilePixel==y.tilePixel;};
 for(size_t k=0;k<a->mainPixels.size();k++){
  if(!equal(a->mainPixels[k],b->mainPixels[k]) || !equal(a->subPixels[k],b->subPixels[k]))sourceChanges++;
  bool diff=!equal(a->scenePixels[k],b->scenePixels[k]);
  for(int c=0;c<2;c++)diff|=ca.height[k*2+c]!=cb.height[k*2+c] || ca.occlusion[k*2+c]!=cb.occlusion[k*2+c];
  for(int c=0;c<4;c++)diff|=ca.surface[k*4+c]!=cb.surface[k*4+c] || ca.emission[k*4+c]!=cb.emission[k*4+c];
  changed+=diff;recovered+=!ca.height[k*2+1] && cb.height[k*2+1];lost+=ca.height[k*2+1] && !cb.height[k*2+1];
  auto p=a->scenePixels[k];
  if(p.instanceId && p.instanceId<=a->tileInstances.size()){
   const auto &i=a->tileInstances[p.instanceId-1];
   if(i.source==RemasterSourceType::Background && i.sourceIndex==1)opaqueChanges+=diff;
   const auto *m=S9xRemasterFrameMetadataForTile(*a,i.tileId);
   if(m && (m->tileRole==RemasterTileRole::StairTop || m->tileRole==RemasterTileRole::StairBottom || i.assetGroup=="stair_treads" || i.assetGroup=="stair_rails"))stairsChanged+=diff;
  }
 }
 std::cout<<"{\"samples\":"<<a->mainPixels.size()<<",\"changed\":"<<changed<<",\"recovered\":"<<recovered<<",\"lost\":"<<lost<<",\"source_changes\":"<<sourceChanges<<",\"opaque_BG2_changes\":"<<opaqueChanges<<",\"stair_changes\":"<<stairsChanged<<"}\n";
 return sourceChanges || lost || opaqueChanges || stairsChanged;
}
