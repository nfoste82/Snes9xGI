// Offline captured CPU source preparation only, NOT a total CPU/GPU benchmark.
#include "../runtime/lighting_field_cache.h"
#include "../runtime/emitter_patches.h"
#include <chrono>
#include <iostream>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 RemasterFrame f;if(!S9xReadRemasterFrame(argv[1],f))return 2;
 RemasterLightingFieldCache fields;fields.update(f,!f.scenePixels.empty());
 auto mesh=RemasterSurfaceMesh::build(f.width,f.height,fields.samples,f.lightingCoordinateScale/255.f);
 for(size_t i=0;i<mesh.size();i++){mesh[i].emissionDepth=fields.depths[i];mesh[i].emissionOmnidirectional=fields.omnidirectional[i];}
 std::cout<<"{\"width\":"<<f.width<<",\"height\":"<<f.height<<",\"lights\":"<<f.lights.size()<<",\"representatives\":"<<int(f.representativeEmitters)<<",\"samples\":[";
 for(int i=0;i<101;i++){
  auto a=std::chrono::steady_clock::now();
#ifdef REMASTER_PRESELECTED_EMITTERS
  RemasterEmitterSelectionWork work;
  auto p=S9xRemasterEmitterPatches(f,fields.emission.data(),fields.participation.data(),mesh,&fields.channelSources,&work);
#else
  auto p=S9xRemasterEmitterPatches(f,fields.emission.data(),fields.participation.data(),mesh);
#endif
  auto b=std::chrono::steady_clock::now();uint64_t digest=14695981039346656037ull;
  for(const auto &patch:p){auto bytes=reinterpret_cast<const uint8_t*>(&patch);for(size_t j=0;j<sizeof(patch);j++)digest=(digest^bytes[j])*1099511628211ull;}
  std::cout<<(i?",":"")<<"{\"ms\":"<<std::chrono::duration<double,std::milli>(b-a).count()<<",\"patches\":"<<p.size()<<",\"digest\":"<<digest<<"}";
 }
 std::cout<<"]}\n";
}
