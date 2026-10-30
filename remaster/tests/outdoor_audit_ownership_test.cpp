// Independent observed-ink contract for the read-only outdoor audit consumer.
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 RemasterFrame f;f.width=f.height=1;f.originalRgb555={7};
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 // Same canonical artwork, independent occurrences and support datums.
 for(int bg=0;bg<2;++bg){
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;
  i.tileId={77,1,4};i.heightOffset=bg?32:7;f.tileInstances.push_back(i);
 }
 RemasterFrameAssetMetadata m;m.tileId={77,1,4};m.hasHeight=true;m.height.fill(0);
 m.hasNormals=true;m.normalXyz.fill(0);f.assetMetadata.push_back(m);
 RemasterFramePixel lower{uint32_t(RemasterSourceType::Background)<<24,1,0};
 RemasterFramePixel upper{uint32_t(RemasterSourceType::Background)<<24|1<<16,2,63};
 f.subPixels={lower};f.mainPixels={upper};
 for(int b=0;b<4;++b){f.observedBgPixels[b]={};f.observedBgDepth[b]={};}
 f.observedBgPixels[0]={lower};f.observedBgDepth[0]={20};
 f.observedBgPixels[1]={upper};f.observedBgDepth[1]={30};
 RemasterLightingFieldCache cpu;cpu.update(f,false);
 need(cpu.channelSources[0].donors[0].instanceId==2,"opaque upper owner masks lower despite same artwork");
 need(cpu.height[0]==32/255.f,"upper authored zero retains its own base");
 need(cpu.authoredNormals[0],"all-zero normal remains authored present");
 // Transparent upper ink has no observation, not a zero-valued geometry owner.
 f.mainPixels={lower};f.observedBgPixels[1]={RemasterFramePixel{}};f.observedBgDepth[1]={0};cpu.update(f,false);
 need(cpu.channelSources[0].donors[0].instanceId==1 && cpu.height[0]==7/255.f,"transparent upper exposes lower base, no hash-global datum");
 // A later overwritten word is a different owner, not permission to resurrect
 // the old upper placement. Conflict excludes its own numeric channel; the
 // established per-channel policy may independently select the lower donor.
 f.mainPixels={upper};f.tileInstances[1].geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;
 cpu.update(f,false);need(cpu.channelSources[0].donors[0].instanceId==1 && cpu.height[0]==7/255.f,"conflict cannot lend upper geometry; fallback keeps lower base");
 std::cout<<"PASS outdoor opaque/transparent/zero/reused-hash/multiple-base/conflict ownership\n";
}
