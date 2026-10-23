// Production EndFrame differential: exact/cropped real packets, no synthetic art.
#include "../runtime/lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 need(argc==5,"usage: prior-output repaired-output expected-visible expected-recovered");
 auto prior=std::make_unique<RemasterFrame>(),after=std::make_unique<RemasterFrame>();
 need(S9xReadRemasterFrame(argv[1],*prior) && S9xReadRemasterFrame(argv[2],*after),"packets");
 need(prior->assetMetadata.size()==after->assetMetadata.size() && prior->tileInstances.size()==after->tileInstances.size(),"no invented assets/instances");
 for(size_t i=0;i<prior->assetMetadata.size();i++) {
  const auto &a=prior->assetMetadata[i],&b=after->assetMetadata[i];
  need(a.tileId==b.tileId && a.height==b.height && a.normalXyz==b.normalXyz && a.hasHeight==b.hasHeight && a.hasNormals==b.hasNormals,"authored asset arrays immutable");
 }
 for(size_t i=0;i<prior->tileInstances.size();i++) {
  const auto &a=prior->tileInstances[i],&b=after->tileInstances[i];
  need(a.tileId==b.tileId && a.hFlip==b.hFlip && a.vFlip==b.vFlip && a.placementHeight==b.placementHeight && a.placementNormalXyz==b.placementNormalXyz,"authored placement arrays immutable");
 }
 RemasterLightingFieldCache a,b;a.update(*prior,false);b.update(*after,false);
 size_t visible=0,recovered=0;
 for(auto buffers:{std::make_pair(&prior->mainPixels,&after->mainPixels),std::make_pair(&prior->subPixels,&after->subPixels),std::make_pair(&prior->scenePixels,&after->scenePixels)}) {
  need(buffers.first->size()==buffers.second->size(),"buffer dimensions");
  for(size_t p=0;p<buffers.first->size();p++) {
   const auto &x=(*buffers.first)[p],&y=(*buffers.second)[p];
   need(x.owner==y.owner && x.instanceId==y.instanceId && x.tilePixel==y.tilePixel,"no fabricated or changed receiver ownership");
   if(buffers.first!=&prior->mainPixels || !y.instanceId)continue;
   const auto &i=after->tileInstances[y.instanceId-1];
   if(i.source!=RemasterSourceType::Object || i.placementTileRole!=RemasterTileRole::Obstacle)continue;
   need(i.geometryStatus==RemasterOccurrenceGeometryStatus::Resolved && i.heightOffset==24,"verified assembly floor base24");
   need(b.height[p*2+1] && b.authoredNormals[p],"real authored height/normal field activation");
   visible++;recovered+=!a.height[p*2+1];
  }
 }
 need(visible==std::stoul(argv[3]) && recovered==std::stoul(argv[4]),"exact visible recovery denominator");
 std::cout<<"PASS production visible="<<visible<<" recovered="<<recovered<<" authored arrays/receivers immutable\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
