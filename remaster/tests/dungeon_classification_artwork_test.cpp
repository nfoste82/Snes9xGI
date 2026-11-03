// Failure class: artwork states must retain classification, NOT solved support,
// across plane/entry boundaries. This exercises the production EndFrame consumer.
#define ALTTP_TEST_CACHE_STAGES
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(255);
 for(int p=0;p<2;p++)for(int y=10;y<16+p;y++)for(int x=10;x<16;x++)(*attr)[p*4096+y*64+x]=0;
 RemasterProfile profile;profile.upperFloorHeight=50;
 std::array<RemasterTileContentId,3> ids;
 for(int phase=0;phase<3;phase++) {
  uint8_t pixels[64];std::fill_n(pixels,64,phase);
  ids[phase]={S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[ids[phase]];
  a.tileId=ids[phase];a.hasHeight=true;a.tileRole=RemasterTileRole::Floor;
  a.height.fill(phase==0?-256:phase==1?0:256); // valid authored zero is not missing
 }
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=c.linkY=88;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 (*maps)[(12*64+12)*2+1]=0xc0;(*maps)[8192+(12*64+12)*2+1]=0x60;
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);
 auto &s=S9xRemasterState();uint16_t screen[64]={};auto frame=std::make_unique<RemasterFrame>();
 auto art=[&](int phase){for(int y=0;y<8;y++){(*gfx)[y*2]=phase&1?255:0;(*gfx)[y*2+1]=phase&2?255:0;}};
 auto run=[&]{S9xRemasterDungeonCacheStages()={};S9xRemasterBeginFrame(64,8,8,8);
  RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
  const int phase=(*gfx)[0]?1:(*gfx)[1]?2:0;instance.tileId=ids[phase];s.tileInstances.push_back(instance);
  auto &observed=s.observedTiles[instance.tileId.hash];observed.hash=instance.tileId.hash;observed.bitDepth=4;
  std::fill_n(observed.indices,64,phase);c.backgroundScrollX=c.backgroundScrollY=88;
  for(int i=0;i<64;i++){s.mainOwners[i]=s.subOwners[i]=S9xRemasterOwner(RemasterSourceType::Background,1,0);
   s.mainInstanceIds[i]=s.subInstanceIds[i]=1;s.mainTilePixels[i]=s.subTilePixels[i]=i;}
  S9xRemasterEndFrame(screen,8,8,8,frame.get(),&c);};
 auto parity=[&]{
  const auto inventory=dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
  auto bg1=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>(*s.dungeonRetainedBg1);
  auto bg2=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>(*s.dungeonRetainedBg2);
  std::vector<uint8_t> before,after;need(S9xSerializeRemasterFrame(*frame,before),"serialize cached");
  // Independent cold invocation: explicitly revoke all classification/solve
  // state. Never use this oracle inside the warm-cache count/timing loop.
  s.dungeonClassificationInputs->valid=false;s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();
  need(S9xRemasterDungeonCacheStages().classifications==2,"oracle did not classify");
  need(inventory==dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"full map inventory differs");
  need(S9xSerializeRemasterFrame(*frame,after) && before==after,"serialized EndFrame packet differs");
  for(int i=0;i<4096;i++)for(int bg=0;bg<2;bg++) {
   const auto &a=bg?(*bg2)[i]:(*bg1)[i],&b=bg?(*s.dungeonRetainedBg2)[i]:(*s.dungeonRetainedBg1)[i];
   need(a.sameGeometry(b) && a.tileHash==b.tileHash,"complete placement differs");
  }
 };
 // Prime each exact art state under its own independently solved plane.
 for(int phase=0;phase<3;phase++){art(phase);c.linkPlane=phase%2;run();}
 for(int n=0;n<12;n++) {
  const int phase=n%3;art(phase);c.linkPlane=1-c.linkPlane;
  const auto epoch=s.runtimeDungeonHeightEpoch;run();
  need(S9xRemasterDungeonCacheStages().solves>=2 && s.runtimeDungeonHeightEpoch>epoch,"plane/support solve skipped");
  need(S9xRemasterDungeonCacheStages().classifications==0,"recurring artwork reclassified after plane boundary");
  need((*s.dungeonRetainedBg2)[0].height[0]==(phase==0?-256:phase==1?0:256),"stale art classification");
 }
 parity();
 // Differential every warmed artwork phase (oracle is outside the count loop).
 for(int target=0;target<3;target++) {
  for(int phase=0;phase<3;phase++){art(phase);c.linkPlane^=1;run();}
  art(target);c.linkPlane^=1;run();parity();
 }
 // Signed support/transition evidence changes still solve; classification is
 // independent of the destination datum and the ordered movement seeds.
 c.entrySupport.cell=12*64+12;c.entrySupport.base=-256;c.entrySupport.predecessorRoom=6;c.entrySupport.transitionEvidence=321;
 run();need(!S9xRemasterDungeonCacheStages().classifications && S9xRemasterDungeonCacheStages().solves>=2,"support invalidation lost");parity();
 c.entrySupport={};run();parity();
 (*gfx)[64000]^=1;c.linkPlane^=1;run();need(!S9xRemasterDungeonCacheStages().classifications,"unreferenced VRAM retained dependency");
 auto rejects=[&]{s.dungeonSnapshotValid=false;run();need(S9xRemasterDungeonCacheStages().classifications==2,"changed dependency reused");parity();};
 (*maps)[1]^=0x40;rejects();(*maps)[1]^=0x80;rejects();(*maps)[1]^=0x20;rejects();(*maps)[2]=1;rejects();
 (*attr)[12*64+12]=0x70;rejects();(*tpl)[12875]^=1;rejects();(*gfx)[32]^=1;rejects();
 c.graphicsBase=32;rejects();c.bg1GraphicsBase=64;rejects();
 c.dungeonTemplateData=nullptr;rejects();c.dungeonTemplateData=tpl->data();rejects();
 c.roomIndex++;rejects();c.romIdentity++;rejects();
 // The classifier can also be called independently of publishability. These
 // exact negative keys are checked without pretending unsupported BG/ROM inputs
 // are publishable EndFrame geometry.
 need(S9xRemasterDungeonClassificationMatches(c,s),"settled classification key");
 c.verifiedAlttpRom=false;need(!S9xRemasterDungeonClassificationMatches(c,s),"verified flag ignored");c.verifiedAlttpRom=true;
 c.placementBackgroundIndex=0;need(!S9xRemasterDungeonClassificationMatches(c,s),"background ignored");c.placementBackgroundIndex=1;
 auto graphics=c.graphics;c.graphics=nullptr;need(!S9xRemasterDungeonClassificationMatches(c,s),"missing channel reused");c.graphics=graphics;
 // Profile publication is the exact boundary for ALL profile fields, including
 // context metadata/groups/bindings and unchanged editor resume publication.
 auto publish=[&]{S9xRemasterSetProfile(profile);rejects();};
 publish();profile.tileContexts[ids[0]]["one"]={};publish();
 profile.tileContexts[ids[0]]["two"]={};publish();
 RemasterContextLocation at;at.area=c.roomIndex;at.background=1;at.x=10;at.y=10;
 profile.contextBindings[at].tile=ids[0];profile.contextBindings[at].name="one";publish();
 profile.assetGroups["obstacles"].tileIds.push_back(ids[0]);publish();
 for(bool research:{true,false}){S9xRemasterSetDungeonResearchEnabled(research);rejects();}
 S9xRemasterSetDungeonResearchEnabled(true);
 auto &writes=S9xRemasterDungeonWrites();writes.enable(true);writes.room=c.roomIndex;c.producerWrites=&writes;
 run();c.linkPlane^=1;run();need(S9xRemasterDungeonCacheStages().classifications==2,"producer provenance reused");
 c.producerWrites=nullptr;S9xRemasterSetDungeonResearchEnabled(false);run();
 S9xRemasterInvalidateDungeonContinuity();need(s.dungeonClassifiedArtworkStates.empty(),"defrost/session retained classification");run();
 // Fixed-state bound and LRU eviction, including a distinct fourth state.
 c.graphicsBase=0;c.bg1GraphicsBase=0xffff;maps->fill(0);
 for(int phase=0;phase<4;phase++){art(phase);c.linkPlane^=1;run();}
 need(s.dungeonClassifiedArtworkStates.size()==RemasterState::dungeonClassificationCapacity,"cache bound");
 art(0);c.linkPlane^=1;run();need(S9xRemasterDungeonCacheStages().classifications==2,"oldest state not evicted");
 std::cout<<"classification_capacity="<<RemasterState::dungeonClassificationCapacity<<" entry_bytes="<<sizeof(RemasterState::DungeonClassifiedArtworkState)<<" max_bytes="<<RemasterState::dungeonClassificationCapacity*sizeof(RemasterState::DungeonClassifiedArtworkState)<<'\n';
 std::cout<<"PASS production recurring-art classification across plane changes; independent solves and signed/zero relief\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
