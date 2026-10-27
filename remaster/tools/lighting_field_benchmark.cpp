// Captured production CPU preparation, no emulator launch and no FPS inference.
#ifndef LIGHTING_FIELD_HEADER
#define LIGHTING_FIELD_HEADER "../runtime/lighting_field_cache.h"
#endif
#include LIGHTING_FIELD_HEADER
#include "../runtime/surface_mesh_cache.h"
#include <chrono>
#include <iostream>
static uint64_t hashBytes(uint64_t h,const void *p,size_t bytes){const auto *b=static_cast<const uint8_t*>(p);while(bytes--)h=(h^*b++)*1099511628211ull;return h;}
template<class T>static void digest(uint64_t &h,const std::vector<T>&v){h=hashBytes(h,v.data(),v.size()*sizeof(T));}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 auto f=std::make_unique<RemasterFrame>();if(!S9xReadRemasterFrame(argv[1],*f))return 2;
 RemasterLightingFieldCache fields;RemasterSurfaceMesh::Cache meshes;
 const bool scene=f->scenePixels.size()==f->mainPixels.size();
 std::cout<<"{\"width\":"<<f->width<<",\"height\":"<<f->height<<",\"instances\":"<<f->tileInstances.size()<<",\"metadata\":"<<f->assetMetadata.size()<<",\"lights\":"<<f->lights.size()<<",\"samples\":[\n";
 for(int i=0;i<201;i++) {
  auto a=std::chrono::steady_clock::now();fields.update(*f,scene);
  auto b=std::chrono::steady_clock::now();
  const auto &mesh=meshes.update(f->width,f->height,fields.samples,f->lightingCoordinateScale/255.f);
  for(size_t k=0;k<mesh.size();k++)if(!fields.authoredNormals[k])std::copy_n(mesh[k].normal,3,fields.surface.begin()+k*4+1);
  auto c=std::chrono::steady_clock::now();
  std::cout<<(i?",":"")<<"{\"frame\":"<<i<<",\"fields_ms\":"<<std::chrono::duration<double,std::milli>(b-a).count()<<",\"mesh_ms\":"<<std::chrono::duration<double,std::milli>(c-b).count()<<",\"updated_pixels\":"<<fields.updatedPixels
#ifndef LIGHTING_BASELINE
  <<",\"selected_pixels\":"<<fields.selectedPixels<<",\"resolved_instances\":"<<fields.resolvedInstances
#endif
  <<",\"mesh_tiles\":"<<meshes.rebuiltTiles<<"}\n";
 }
 uint64_t h=14695981039346656037ull;
 digest(h,fields.occlusion);digest(h,fields.emission);digest(h,fields.height);digest(h,fields.surface);
 digest(h,fields.participation);digest(h,fields.opposite);digest(h,fields.reflectance);digest(h,fields.authoredNormals);
 digest(h,fields.depths);digest(h,fields.omnidirectional);
 std::cout<<"],\"fields_digest\":"<<h<<"}\n";
}
