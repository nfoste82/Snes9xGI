// Read-only projection audit: run the exact CPU lighting-field producer.
#include "../../runtime/lighting_field_cache.h"
#include <iostream>
#include <memory>
int main(int argc, char **argv) {
 if(argc!=2)return 2;
 auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(argv[1],*f))return 2;
 RemasterLightingFieldCache fields;
 const bool scene=!f->scenePixels.empty(); fields.update(*f,scene,true);
 for(size_t at=0;at<f->mainPixels.size();at++) {
  std::cout<<"{\"sx\":"<<at%f->width<<",\"sy\":"<<at/f->width<<",\"use_scene\":"<<scene
   <<",\"field_known\":"<<(fields.height[at*2+1]>.5)<<",\"participation\":"<<int(fields.participation[at*2])<<",\"height\":"<<fields.height[at*2]*255;
  for(int b=0;b<3;b++) {
   const auto &v=b==0?f->mainPixels:b==1?f->subPixels:f->scenePixels;
   if(at>=v.size())continue;const auto &p=v[at];
   std::cout<<",\""<<(b==0?"main":b==1?"sub":"scene")<<"\":{\"owner\":"<<p.owner<<",\"id\":"<<p.instanceId;
   if(p.instanceId && p.instanceId<=f->tileInstances.size() && p.tilePixel<64) {
    const auto &i=f->tileInstances[p.instanceId-1];const auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);
    auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},*f,p.instanceId-1);
    int ink=-1;for(const auto &a:f->assets)if(a.tileId==i.tileId){ink=a.indices[p.tilePixel];break;}
    std::cout<<",\"source\":"<<int(i.source)<<",\"bg\":"<<int(i.sourceIndex)+1<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec
     <<"\",\"k\":"<<int(p.tilePixel)<<",\"ink\":"<<ink<<",\"base\":"<<i.heightOffset<<",\"has_height\":"<<g.hasHeight
     <<",\"status\":"<<int(i.geometryStatus)<<",\"role\":"<<int(g.tileRole)<<",\"has_normals\":"<<g.hasNormals<<",\"palette_artwork\":"<<i.hasPaletteArtwork<<",\"placement\":"<<i.hasPlacementHeight<<",\"local\":"<<g.height[p.tilePixel];
   }std::cout<<'}';
  }std::cout<<"}\n";
 }
}
