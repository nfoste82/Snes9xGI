#include "../runtime/lighting_field_cache.h"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
 RemasterFrame f;f.width=8;f.height=8;
 f.mainPixels.resize(64);f.subPixels.resize(64);f.originalRgb555.resize(64);
 RemasterFrameTileInstance in;in.tileId.hash=1;in.source=RemasterSourceType::Background;
 f.tileInstances.push_back(in);
 RemasterFrameAssetMetadata m;m.tileId=in.tileId;m.hasHeight=true;m.height.fill(0);
 f.assetMetadata.push_back(m);
 for(size_t i=0;i<64;i++){f.mainPixels[i].instanceId=1;f.mainPixels[i].tilePixel=i;f.mainPixels[i].owner=uint32_t(RemasterSourceType::Background)<<24;}
 RemasterLightingFieldCache off,on;
 RemasterLightingTrace::Frame trace;trace.id=RemasterLightingTrace::nextId();
 off.update(f,false);
 { RemasterLightingTrace::Scope scope(&trace,RemasterLightingTrace::Fields);on.update(f,false); }
 assert(on.height==off.height && on.surface==off.surface && on.emission==off.emission && on.participation==off.participation);
 { RemasterLightingTrace::Scope scope(&trace,RemasterLightingTrace::Fields);on.update(f,false); }
 assert(!on.reasons && !on.updatedPixels);
 // Independent simultaneous changes must all survive short-circuit cache logic.
 f.area.id++;f.tileInstances[0].hFlip=true;f.tileInstances[0].hasPaletteArtwork=true;
 f.tileInstances[0].paletteRgb555.fill(17);f.assetMetadata[0].hasEmission=true;
 f.assetMetadata[0].emissionRgba.fill(0); // authored zero is presence, not absence
 f.assetMetadata[0].height[0]=4;
 f.mainPixels[0].tilePixel=1;f.observedBgDepth[0].resize(64,3);
 f.contextGeometry[1].hasHeight=true;f.contextGeometry[1].height.fill(0);
 off.update(f,false);
 { RemasterLightingTrace::Scope scope(&trace,RemasterLightingTrace::Fields);on.update(f,false); }
 const auto expected=RemasterLightingFieldCache::SceneMetadata|RemasterLightingFieldCache::InstanceIdentity|
  RemasterLightingFieldCache::Palette|RemasterLightingFieldCache::EmissionMetadata|RemasterLightingFieldCache::ContentGeometry|
  RemasterLightingFieldCache::Raster|RemasterLightingFieldCache::CompositionDepth|RemasterLightingFieldCache::Context;
 assert((on.reasons&expected)==expected);
 assert(on.height==off.height && on.surface==off.surface && on.emission==off.emission && on.occlusion==off.occlusion);
 // Deterministic full viewport workload; no claim about the live room's cost.
 f.width=256;f.height=224;f.mainPixels.resize(256*224,f.mainPixels[1]);
 f.subPixels.resize(256*224);f.originalRgb555.resize(256*224);
 for(auto &r:f.observedBgPixels)r=f.mainPixels;
 for(auto &d:f.observedBgDepth)d.resize(256*224);
 on.update(f,false);
 for(bool enabled:{false,true}) {
  const auto start=std::chrono::steady_clock::now();
  for(int i=0;i<300;i++) {
   RemasterLightingTrace::Scope scope(enabled?&trace:nullptr,RemasterLightingTrace::Fields);
   on.update(f,false);
  }
  const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"trace="<<enabled<<" iterations=300 pixels=57344 elapsed_ms="<<ms<<" per_frame_us="<<ms*1000/300<<'\n';
 }
}
