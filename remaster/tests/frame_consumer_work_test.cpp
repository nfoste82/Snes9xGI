// Inspection demand never disables continuity/solving, and lightweight packets
// cannot escape into full lighting/capture consumers. Buffer reuse resets every
// optional field while retaining exclusively producer-owned capacities.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main() {
 auto &s=S9xRemasterState();
 s.requestedInspectableFrames=true;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto templates=std::make_unique<std::array<uint8_t,12876>>();templates->fill(255);
 uint8_t zero[64]={};RemasterTileContentId id{S9xRemasterHashTile(4,zero),1,4};
 RemasterProfile p;p.romSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 p.upperFloorHeight=50;p.assets[id].tileId=id;p.assets[id].hasHeight=true;p.assets[id].tileRole=RemasterTileRole::Floor;
 p.assetGroups["dungeon_floor"].tileIds={id};S9xRemasterSetProfile(p);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x61;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.collisionAttributes=attrs->data();c.dungeonTemplateData=templates->data();
 c.module=7;c.linkX=64;c.linkY=64;
 for(int plane=0;plane<2;plane++)for(int y=6;y<16;y++)for(int x=6;x<16;x++)(*attrs)[plane*4096+y*64+x]=0;
 RemasterAreaContext area;area.id=c.roomIndex;
 std::array<int,4> scroll{};uint16_t screen[64]={};
 auto run=[&](bool full){
  S9xRemasterSetLiveFramesEnabled(full);S9xRemasterBeginFrame(64,8,8,8);
  auto &trace=RemasterLiveTrace::refreshFrame();trace={};trace.id=RemasterLightingTrace::nextId();trace.begin=RemasterLightingTrace::now();
  RemasterFrameTileInstance i;i.tileId=id;i.source=RemasterSourceType::Background;i.sourceIndex=1;s.tileInstances.push_back(i);
  for(size_t k=0;k<64;k++){s.mainOwners[k]=0x02010000;s.mainInstanceIds[k]=1;s.mainTilePixels[k]=k;}
  S9xRemasterEndFrame(screen,8,8,8,nullptr,&c,nullptr,&area,0,nullptr,&scroll,&scroll);
  require(s.completedFrameAvailable,"fresh inspection published");
 };
 run(true);require(s.completedFrame.fullGeometry,"lighting requests full");
 const auto offsets=s.runtimeDungeonHeightMap.offsets;
 auto epoch=s.runtimeDungeonHeightEpoch;
 std::vector<uint8_t> original;require(S9xSerializeRemasterFrame(s.completedFrame,original),"full serializes");
 auto originalCopy=s.completedFrame;
 run(false);require(!s.completedFrame.fullGeometry,"ordinary inspection is lightweight");
 require(!s.completedFrame.lightingTrace.calls[RemasterLightingTrace::Visible] && !s.completedFrame.lightingTrace.calls[RemasterLightingTrace::Offscreen],"inspection omits expensive publication stages");
 require(s.runtimeDungeonHeightEpoch==epoch && s.runtimeDungeonHeightMap.offsets==offsets,"inspection retains solve");
 std::vector<uint8_t> bytes;require(!S9xSerializeRemasterFrame(s.completedFrame,bytes),"incomplete packet cannot be captured");
 require(s.completedFrame.contextLocations.size()==1,"light inspection retains locations");
 run(true);require(S9xSerializeRemasterFrame(s.completedFrame,bytes) && bytes==original,"off/on exact full output");
 std::vector<uint8_t> retained;require(S9xSerializeRemasterFrame(originalCopy,retained) && retained==original,"retained immutable copy unchanged");
 (*attrs)[8*64+8]=1;run(false);require(s.runtimeDungeonHeightEpoch>epoch,"inspection maintains collision invalidation");
 epoch=s.runtimeDungeonHeightEpoch;run(true);require(s.runtimeDungeonHeightEpoch==epoch,"re-enable consumes maintained solve");
 s.requestedFullGeometry=true;run(false);require(s.completedFrame.fullGeometry,"paused refresh full");s.requestedFullGeometry=false;
 S9xRemasterSetRoomDiagnosticsEnabled(true);run(false);require(s.completedFrame.fullGeometry,"diagnostic full");S9xRemasterSetRoomDiagnosticsEnabled(false);
 s.frameCaptureActive=true;s.activeFrameCount=2;run(false);require(s.completedFrame.fullGeometry,"capture full even with lighting off");s.frameCaptureActive=false;

 // Finalizer capacity reuse has a fresh-allocation oracle including all bytes.
 auto reused=std::make_unique<RemasterFrame>();
 for(int pass=0;pass<4;pass++) {
  const size_t width=pass%2?5:8,pitch=pass%2?10:8,height=pass%2?3:8;
  S9xRemasterBeginFrame(pitch*height,pitch,width,height);
  s.tileInstances.resize(pass%2?2:1);for(auto &i:s.tileInstances)i.tileId=id;
  if(pass%2){s.sceneRgb.assign(pitch*height,17);s.scenePixels.assign(pitch*height,{});}
  else {s.observedBgPixels[1].clear();s.observedBgDepth[1].clear();}
  reused->contextLocations[999]={};reused->hiddenEmitters.resize(2);reused->lights.resize(3);reused->fullGeometry=false;
  RemasterFrame fresh;
  require(S9xRemasterFinalizeFrame(fresh,screen,pitch,width,height) && S9xRemasterFinalizeFrame(*reused,screen,pitch,width,height),"finalize both");
  std::vector<uint8_t> a,b;require(S9xSerializeRemasterFrame(fresh,a) && S9xSerializeRemasterFrame(*reused,b) && a==b,"reused/fresh complete packet parity");
  require(reused->contextLocations.empty() && reused->lights.empty() && reused->hiddenEmitters.empty(),"no stale optional state");
 }
 std::cout<<"PASS consumer demand, maintained solve/invalidation, off/on exact output, paused/diagnostics, retained copy, reused/fresh packet parity\n";
}
