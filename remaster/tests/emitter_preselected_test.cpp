#include "../runtime/lighting_field_cache.h"
#include "../runtime/emitter_patches.h"
#include "../runtime/field_upload_cache.h"
#include <cstdlib>
#include <iostream>
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
// The renderer already paid for channel selection. Clustering must consume that
// exact result, not resolve every occurrence and select all channels twice more.
int main(){
 { // Lagging in-flight slots cannot share an upload baseline. Exercise exact
   // dirty records, growth/shrink and switchback without a Metal dependency.
  std::array<RemasterFieldUploadCache,3> slots;
  std::array<std::vector<uint8_t>,3> gpu;
  std::vector<uint8_t> data(64*20,0);
  for(int step=0;step<30;step++){
   int slot=step%3;if(step==5)data[64*7]=9;if(step==8)data[64*7]=0;
   if(step==11)data.resize(64*35,1);if(step==17)data.resize(64*3);
   if(step==20)data[1]=7;
   gpu[slot].resize(data.size());
   slots[slot].update(data.data(),64,data.size()/64,[&](size_t first,size_t rows){
    std::copy_n(data.begin()+first*64,rows*64,gpu[slot].begin()+first*64);
   });
   need(data==gpu[slot],"per-slot dirty record uploads preserve motion/growth/shrink/switchback");
  }
 }
 for(int flip=0;flip<4;flip++)for(int order=0;order<2;order++)for(int mutation=0;mutation<10;mutation++){
  RemasterFrame f;f.width=16;f.height=8;f.representativeEmitters=2;
  f.mainPixels.resize(128);f.subPixels.resize(128);f.scenePixels.resize(128);f.originalRgb555.resize(128,32767);
  for(int n=0;n<2;n++){
   RemasterFrameAssetMetadata m;m.tileId={uint64_t(100+n),1,4};m.hasHeight=m.hasEmission=true;m.height.fill(0);
   m.emissionRgba.fill(255);m.combineEmitters.push_back({uint64_t(101-n),1,4});f.assetMetadata.push_back(m);
   RemasterFrameTileInstance inst;inst.tileId=m.tileId;inst.source=RemasterSourceType::Background;inst.sourceIndex=n^order;
   inst.hFlip=flip&1;inst.vFlip=flip&2;f.tileInstances.push_back(inst);
  }
  for(int y=0;y<8;y++)for(int x=0;x<16;x++){
   int n=x/8,k=(flip&2?7-y:y)*8+(flip&1?7-x%8:x%8);
   RemasterFramePixel p{(uint32_t(RemasterSourceType::Background)<<24)|uint32_t((n^order)<<16),uint32_t(n+1),uint8_t(k)};
   f.mainPixels[y*16+x]=f.scenePixels[y*16+x]=p;
  }
  if(mutation==1)f.assetMetadata[0].hasEmission=false;
  if(mutation==2)f.assetMetadata[0].emissionRgba.fill(0); // present zero
  if(mutation==3)f.assetMetadata[0].combineEmitters.clear();
  if(mutation==4)for(int i=0;i<128;i++) {f.subPixels[i]=f.mainPixels[i];f.mainPixels[i]={};}
  if(mutation==5)for(int i=0;i<128;i+=3)f.scenePixels[i]={}; // clipped/overwritten
  if(mutation==6)f.assetMetadata.clear(); // explicit source-only ABI
  if(mutation==7)f.tileInstances[0].geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;
  if(mutation==8)f.tileInstances[0].hFlip=!f.tileInstances[0].hFlip;
  if(mutation==9)f.assetMetadata[0].emissionDepth=8;
  RemasterLightingFieldCache fields;fields.update(f,true);
  auto mesh=RemasterSurfaceMesh::build(f.width,f.height,fields.samples,f.lightingCoordinateScale/255.f);
  for(size_t i=0;i<mesh.size();i++){mesh[i].emissionDepth=fields.depths[i];mesh[i].emissionOmnidirectional=fields.omnidirectional[i];}
  auto expected=S9xRemasterEmitterPatches(f,fields.emission.data(),fields.participation.data(),mesh);
  size_t selections=128; // baseline performs at least the entire first raster
#ifdef REMASTER_PRESELECTED_EMITTERS
  RemasterEmitterSelectionWork work;
  auto actual=S9xRemasterEmitterPatches(f,fields.emission.data(),fields.participation.data(),mesh,&fields.channelSources,&work);
  selections=work.selectedPixels;
#else
  auto actual=S9xRemasterEmitterPatches(f,fields.emission.data(),fields.participation.data(),mesh);
#endif
  need(actual.size()==expected.size(),"patch count parity");
  for(size_t i=0;i<actual.size();i++)need(actual[i].count==expected[i].count && actual[i].mode==expected[i].mode && actual[i].members==expected[i].members,"exact cluster/membership/layout parity");
  need(selections==0,"FAIL renderer preselected donors must avoid redundant full-frame channel selections");
 }
 std::cout<<"PASS preselected emitter donors: 80 transformed/BG/mutation cases and 30 lagging-slot updates\n";
}
