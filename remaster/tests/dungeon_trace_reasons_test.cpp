// Diagnostic reasons must observe the existing dependency path, not rescan it.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto state=std::make_unique<RemasterState>();
 RemasterDungeonFloorContext c;c.constructedMaps=maps->data();c.graphics=gfx->data();
 c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 state->dungeonRetainedBg1=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 state->dungeonRetainedBg2=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterLightingTrace::Frame trace;trace.id=1;
 RemasterLightingTrace::Scope scope(&trace,RemasterLightingTrace::Room);
 auto check=[&](bool expected,uint64_t reasons){
  trace.classificationReasons=0;
  need(S9xRemasterDungeonClassificationMatches(c,*state)==expected,"classification result changed");
#if S9X_REMASTER_PROFILING
  need(trace.classificationReasons==reasons,"wrong observed classification reason");
#else
  need(trace.classificationReasons==0,"disabled instrumentation wrote reasons");
#endif
 };
 using namespace RemasterLightingTrace;
 check(false,MissingClassification);
 S9xRemasterRememberDungeonClassification(c,*state);check(true,0);
 // Plane and entry are not classification dependencies; authored zeros are valid.
 c.linkPlane=1;c.entrySupport.base=37;check(true,0);
 (*maps)[1]=0xc0;(*attr)[0]=1;check(false,MapChange); // short circuit masks later attribute change
 (*maps)[1]=0;check(false,AttributeChange);(*attr)[0]=0;
 (*tpl)[12875]=1;check(false,TemplateChange);(*tpl)[12875]=0;
 (*gfx)[64000]=1;check(true,0); // unreferenced graphics must stay unexamined
  (*gfx)[0]=1;check(false,GraphicsDependenciesChange);(*gfx)[0]=0;
  c.replacementEvidence.available=true;c.replacementEvidence.room=7;check(false,ReplacementEvidenceChange);
  S9xRemasterRememberDungeonClassification(c,*state);check(true,0);
  c.replacementEvidence.slots[0].words[0]=100;check(false,ReplacementEvidenceChange);
  S9xRemasterRememberDungeonClassification(c,*state);
  (*gfx)[3200]^=1;check(false,GraphicsDependenciesChange);(*gfx)[3200]^=1;
  c.replacementEvidence={};S9xRemasterRememberDungeonClassification(c,*state);
 c.bg1GraphicsBase=32;check(false,GraphicsBaseChange);c.bg1GraphicsBase=0xffff;
  c.placementBackgroundIndex=0;check(false,BackgroundChange);c.placementBackgroundIndex=1;
 c.verifiedAlttpRom=true;check(false,VerifiedChange);c.verifiedAlttpRom=false;
 state->dungeonGeometryProfileKey++;check(false,ProfileChange);state->dungeonGeometryProfileKey--;
 c.roomIndex++;check(false,RoomChange);c.roomIndex--;
 c.graphics=nullptr;check(false,MissingClassification);c.graphics=gfx->data();
 c.dungeonTemplateData=nullptr;check(false,TemplateChange);c.dungeonTemplateData=tpl->data();
 check(true,0);
 scope.stop();
 // Exercise the real EndFrame consumer without an emulator, files or trace worker.
 auto &live=S9xRemasterState();
 attr->fill(1);tpl->fill(255);gfx->fill(0);maps->fill(0);
 for(int p=0;p<2;p++)for(int y=10;y<16;y++)for(int x=10;x<16;x++)(*attr)[p*4096+y*64+x]=0;
 uint8_t zero[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,zero),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].hasHeight=true;
 profile.assets[id].tileRole=RemasterTileRole::Floor;profile.assets[id].height.fill(0);
 c={};c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=c.linkY=88;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);
 uint16_t screen[64]={};
 auto run=[&]{
  auto &f=RemasterLiveTrace::refreshFrame();f={};f.id=1;
  S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);
  return live.completedFrame.lightingTrace;
 };
 auto first=run();
#if S9X_REMASTER_PROFILING
 need(first.roomReasons&RoomChange,"initial room reason");
 need(first.classificationOutcome==2 && first.classificationReasons==MissingClassification,"initial classifier rebuild");
 need(first.calls[ContextBindings]==1 && first.calls[Terrain]==1,"missing EndFrame stages");
 need(first.calls[DoorRecognition]==4,"door recognition scopes");
 need(first.instanceCount==live.completedFrame.tileInstances.size(),"instance count");
 need(first.instanceBytes==first.instanceCount*sizeof(RemasterFrameTileInstance),"instance bytes");
#endif
  auto same=run();
#if S9X_REMASTER_PROFILING
 need(same.roomReasons==0 && same.classificationOutcome==0,"unchanged frame considered classifier");
 need(same.projectionOutcome==1,"settled projection not retained");
#endif
  c.replacementEvidence.available=true;c.replacementEvidence.room=c.roomIndex;
  auto replacement=run();
#if S9X_REMASTER_PROFILING
  need(replacement.roomReasons==ReplacementEvidenceChange && replacement.classificationReasons==ReplacementEvidenceChange,"replacement evidence reasons missing");
#endif
  c.replacementEvidence.slots[0].words={100,101,102,103};run();
  (*gfx)[100*32]=1;auto prospective=run();
#if S9X_REMASTER_PROFILING
  need(prospective.roomReasons==GraphicsDependenciesChange && (prospective.classificationReasons&GraphicsDependenciesChange),"prospective graphics reasons missing"); // older resident probes may add evidence misses
#endif
  (*gfx)[64000]^=1;auto irrelevant=run();
#if S9X_REMASTER_PROFILING
  need(!irrelevant.roomReasons && !irrelevant.classificationReasons,"irrelevant graphics invalidated");
#endif
 c.linkPlane=1;auto plane=run();
#if S9X_REMASTER_PROFILING
 need(plane.roomReasons==PlaneChange && plane.classificationOutcome==1 && !plane.classificationReasons,"plane reuse reason");
#endif
  c.linkX++;auto movement=run();
#if S9X_REMASTER_PROFILING
   need(!movement.roomReasons && !movement.classificationOutcome && !movement.classificationReasons,"certified movement rebuilt");
#else
  need(!movement.roomReasons,"profiling-off seed reason write");
#endif
   c.linkX=300;auto disconnected=run();
#if S9X_REMASTER_PROFILING
   need(disconnected.roomReasons==MovementSeedChange && disconnected.classificationOutcome==1 && !disconnected.classificationReasons,"uncertified movement seed reason");
#endif
  (*maps)[1]=0xc0;auto changed=run();
#if S9X_REMASTER_PROFILING
  need(changed.roomReasons==MapChange && changed.classificationOutcome==2 && (changed.classificationReasons&MapChange),"map rebuild reason"); // failed older resident probes can add replacement-evidence reasons
#endif
 c.submodule=2;auto transition=run();
#if S9X_REMASTER_PROFILING
 need(transition.projectionOutcome==2,"transition projection not disabled");
#endif
 std::cout<<"PASS observed classification reasons and EndFrame outcomes/stages, short circuits, irrelevant plane/entry/VRAM, missing inputs, valid zeros\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
