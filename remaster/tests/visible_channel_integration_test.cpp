#include "../runtime/lighting_field_cache.h"
#include <iostream>
#include <cstdlib>
static void need(bool b,const char*s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 RemasterFrame f;f.width=f.height=1;f.originalRgb555={7};f.sceneRgb555={9};
 for(unsigned j=0;j<2;j++){
  RemasterFrameTileInstance i;i.tileId={17+j,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=j;i.heightOffset=j?32:0;f.tileInstances.push_back(i);
  RemasterFrameAssetMetadata m;m.tileId=i.tileId;
  if(j){m.hasHeight=m.hasNormals=m.hasOcclusion=m.hasEmission=m.hasDoorframeMask=true;m.height.fill(7);m.tileRole=RemasterTileRole::Doorway;m.normalXyz.fill(128);m.occlusion.fill(0);m.emissionRgba.fill(25);m.doorframeMask=0;}
  f.assetMetadata.push_back(m);
 }
 RemasterFramePixel a{uint32_t(RemasterSourceType::Background)<<24,1,13},b{uint32_t(RemasterSourceType::Background)<<24|1<<16,2,42};
 f.mainPixels={a};f.subPixels={b};f.scenePixels={a};
 auto range=S9xRemasterFrameHeightRange(f);need(range.first==39,"height preview range must follow effective fallback donor and contextual base");
 auto lights=S9xRemasterFrameEmissionLights(f);need(!lights.empty(),"emission extraction must use common fallback donor");
 std::vector<uint8_t> bytes;need(S9xSerializeRemasterFrame(f,bytes),"serialize");RemasterFrame replay;need(S9xDeserializeRemasterFrame(bytes,replay),"replay");
 RemasterLightingFieldCache cpu;cpu.update(replay,true);need(cpu.height[0]==39/255.f,"replay actual field parity");
 for(unsigned order=0;order<2;order++)for(unsigned hv=0;hv<4;hv++) {
  auto n=f;n.tileInstances[0].sourceIndex=order;n.tileInstances[1].sourceIndex=1-order;
  n.tileInstances[0].hFlip=n.tileInstances[1].hFlip=hv&1;n.tileInstances[0].vFlip=n.tileInstances[1].vFlip=hv&2;
  n.tileInstances[1].heightOffset=0;auto solved=n.tileInstances[1];solved.heightOffset=52;n.tileInstances.push_back(solved);
  n.mainPixels[0].owner=uint32_t(RemasterSourceType::Background)<<24|order<<16;
  n.subPixels[0].owner=uint32_t(RemasterSourceType::Background)<<24|(1-order)<<16;
  n.scenePixels[0]=n.subPixels[0];n.scenePixels[0].instanceId=3;
  cpu.update(n,true);need(cpu.channelSources[0].donors[0].instanceId==3 && cpu.height[0]==59/255.f,"alternate donor conserves solved occurrence datum, both orders/HV");
  n.tileInstances[2].tileId.hash++;cpu.update(n,true);
  need(cpu.channelSources[0].donors[0].instanceId==2 && cpu.height[0]==7/255.f,"changed-art alternate rejects solved datum borrowing");
 }
 // Main-only hidden BG ink is a real fallback even when it never won main/sub.
 auto observed=f;observed.subPixels[0]={};observed.scenePixels[0]=a;
 observed.observedBgPixels[1]={b};observed.observedBgDepth[1]={17};
 cpu.update(observed,true);need(cpu.height[0]==39/255.f,"independent raster ink donor");
 need(S9xSerializeRemasterFrame(observed,bytes)&&S9xDeserializeRemasterFrame(bytes,replay),"observed BG replay appendix");
 cpu.update(replay,true);need(cpu.height[0]==39/255.f&&replay.observedBgDepth[1][0]==17,"observed source/depth roundtrip parity");
 // Generic BG3 is not ALTTP HUD: it remains a presentation boundary.
 auto third=f;third.mainPixels[0].owner=uint32_t(RemasterSourceType::Background)<<24|2<<16;
 third.tileInstances[0].sourceIndex=2;cpu.update(third,true);
 need(!cpu.channelSources[0].donors[0].instanceId,"other games BG3 is not globally HUD");
 // Visual-claim provenance follows each independent donor, never channel
 // presence. All-zero authored arrays remain present through conflict/replay.
 for(unsigned order=0;order<2;order++)for(unsigned hv=0;hv<4;hv++) {
  auto n=f;n.tileInstances[0].sourceIndex=order;n.tileInstances[1].sourceIndex=1-order;
  n.mainPixels[0].owner=n.scenePixels[0].owner=uint32_t(RemasterSourceType::Background)<<24|order<<16;
  n.subPixels[0].owner=uint32_t(RemasterSourceType::Background)<<24|(1-order)<<16;
  for(auto &i:n.tileInstances){i.hFlip=hv&1;i.vFlip=hv&2;}
  n.tileInstances[0].visualClaim={RemasterVisualClaimStatus::Unsupported,UINT64_MAX,9,-1,false};
  n.tileInstances[1].visualClaim={RemasterVisualClaimStatus::Conflict,UINT64_C(1)<<42,19,7,false};
  n.assetMetadata[0].hasNormals=true;n.assetMetadata[0].normalXyz.fill(0);
  n.assetMetadata[1].height.fill(0);n.assetMetadata[1].occlusion.fill(0);n.assetMetadata[1].emissionRgba.fill(0);
  cpu.update(n,true);
  need(cpu.visualClaims[0][0].status==RemasterVisualClaimStatus::Conflict && cpu.height[0]==32/255.f && cpu.height[1]==1,
   "known authored zero height and conflicted alternate donor coexist");
  need(cpu.visualClaims[0][1].status==RemasterVisualClaimStatus::Unsupported && cpu.authoredNormals[0],
   "normal donor keeps own independent unsupported claim and valid zero array");
  need(cpu.emission[0]==0 && cpu.occlusion[0]==0,"conflict preserves zero emission and occlusion");
  need(S9xSerializeRemasterFrame(n,bytes)&&S9xDeserializeRemasterFrame(bytes,replay),"claim serialization");
  RemasterLightingFieldCache decoded;decoded.update(replay,true);
  need(decoded.visualClaims==cpu.visualClaims && decoded.height==cpu.height && decoded.surface==cpu.surface,"actual field claim and geometry replay parity");
  n.tileInstances[1].visualClaim.status=RemasterVisualClaimStatus::Unanchored;cpu.update(n,true);
  need(cpu.visualClaims[0][0].status==RemasterVisualClaimStatus::Unanchored && cpu.updatedPixels==1,"claim-only mutation invalidates cache");
  n.tileInstances[1].visualClaim.mask=1;cpu.update(n,true);
  need(cpu.visualClaims[0][0].status==RemasterVisualClaimStatus::NotEvaluated,"canonical mask cannot leak across pixels");
  n.assetMetadata[1].hasHeight=false;cpu.update(n,true);
  need(!cpu.channelSources[0].donors[0].instanceId && cpu.visualClaims[0][0].status==RemasterVisualClaimStatus::NotEvaluated,
   "absent height is not a conflict height donor or authored zero");
  need(cpu.authoredNormals[0],"supported normals survive absent height");
  n.tileInstances[1].visualClaim.published=true;
  need(!S9xSerializeRemasterFrame(n,bytes),"unanchored claim cannot serialize as published solved evidence");
 }
 std::cout<<"PASS common consumers/replay\n";
}
