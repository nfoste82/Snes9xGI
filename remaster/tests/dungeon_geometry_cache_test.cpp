// Production EndFrame, not a classifier-only/cache-key test. Checks stay active in Release.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(255);
 uint8_t pixels[64];std::fill_n(pixels,64,1);
 RemasterTileContentId a={S9xRemasterHashTile(4,pixels),1,4};
 std::fill_n(pixels,64,2);RemasterTileContentId b={S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;
 for(auto id:{a,b}){profile.assets[id].tileId=id;profile.assets[id].hasHeight=true;profile.assetGroups["dungeon_floor"].tileIds.push_back(id);}
 profile.assets[a].hasEmission=profile.assets[b].hasEmission=true;
 profile.assets[a].emissionRgba.fill(1);profile.assets[b].emissionRgba.fill(2);
 for(int y=0;y<8;y++)(*gfx)[y*2]=255;
 for(int p=0;p<2;p++)for(int y=10;y<18;y++)for(int x=10;x<18;x++)(*attr)[p*4096+y*64+x]=0;
 // Same hash in different transformed usages on both BGs.
 (*maps)[(12*64+12)*2+1]=0xc0;(*maps)[8192+(12*64+12)*2+1]=0x40;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=c.groundedEvidence=true;
 c.roomIndex=7;c.romIdentity=123;c.linkX=88;c.linkY=88;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.dungeonTemplateData=tpl->data();
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);auto &s=S9xRemasterState();
 auto f=std::make_unique<RemasterFrame>();uint16_t screen[64]={};
 auto run=[&]{S9xRemasterBeginFrame(64,8,8,8);
  const auto id=(*gfx)[0]?a:b;
  RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;instance.tileId=id;
  s.tileInstances.push_back(instance);s.observedTiles[id.hash].hash=id.hash;s.observedTiles[id.hash].bitDepth=4;
  std::fill_n(s.observedTiles[id.hash].indices,64,id==a?1:2);
  c.backgroundScrollX=10*8;c.backgroundScrollY=10*8;
  for(int i=0;i<64;i++){s.mainOwners[i]=s.subOwners[i]=S9xRemasterOwner(RemasterSourceType::Background,1,0);s.mainInstanceIds[i]=s.subInstanceIds[i]=1;s.mainTilePixels[i]=s.subTilePixels[i]=i;}
  S9xRemasterEndFrame(screen,8,8,8,f.get(),&c);};
 auto differential=[&]{
  auto bg2=dungeonMapInventory(s.runtimeDungeonHeightMap),bg1=dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
  std::vector<uint8_t> bytes;check(S9xSerializeRemasterFrame(*f,bytes),"serialize");
  s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();
  check(bg2==dungeonMapInventory(s.runtimeDungeonHeightMap) && bg1==dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"complete maps/diagnostics differ from full solve");
  std::vector<uint8_t> full;check(S9xSerializeRemasterFrame(*f,full) && bytes==full,"production packet differs");
 };
 run();auto epoch=s.runtimeDungeonHeightEpoch,key=s.runtimeDungeonHeightKey,solveKey=s.runtimeDungeonSolveGeometryKey;
 for(int y=0;y<8;y++){(*gfx)[y*2]=0;(*gfx)[y*2+1]=255;}run();
 check(s.runtimeDungeonHeightEpoch==epoch && s.dungeonGeometryReuseCount==1,"equivalent animation not reused");
 check(s.runtimeDungeonHeightKey!=key && s.runtimeDungeonHeightKey==s.runtimeDungeonObservedInputKey && s.runtimeDungeonSolveGeometryKey==solveKey,"evidence/solve identities conflated");
 check(s.runtimeDungeonHeightMap.producerHashes[0]==b.hash && s.runtimeDungeonBg1HeightMap.producerHashes[0]==b.hash,"stale producer");differential();
  check(f->tileInstances[0].tileId==b,"animated source identity not refreshed");
  // Same-room internal doorway camera transition: no raw-authored reset.
  // Every phase still processes exact input invalidation (including animation).
  for(int phase=0;phase<8;phase++) {
   c.submodule=1;c.constructionPhase=phase;run();
   check(S9xRemasterDungeonHeightInputsPublishable(c,7),"subtile topology suppressed");
   check(s.runtimeDungeonHeightValid && f->tileInstances[0].heightOffset==s.runtimeDungeonHeightMap.offsets[10*64+10],"subtile placement reset");
  }
  c.submodule=1;c.constructionPhase=8;
  check(!S9xRemasterDungeonHeightInputsPublishable(c,7),"unsupported subtile phase accepted");
  c.submodule=2;c.constructionPhase=0;
  check(!S9xRemasterDungeonHeightInputsPublishable(c,7),"partial supertile accepted");
  c.submodule=5;run();
  check(S9xRemasterDungeonHeightInputsPublishable(c,7),"post-subtile shutter reset");
  c.submodule=0;c.constructionPhase=0;run();
  // Default-identical identities are NOT interchangeable when occurrence-local
  // context activation or facing/flip capability differs. Exercise both
  // directions without publishing/changing profile bytes between frames.
  profile.tileContexts[a]["one"]={};profile.tileContexts[a]["two"]={};
  RemasterContextLocation at;at.area=c.roomIndex;at.background=1;at.x=10;at.y=10;
  profile.contextBindings[at].tile=a;profile.contextBindings[at].name="one";
  S9xRemasterSetProfile(profile);run();
  for(int direction=0;direction<4;direction++) {
   auto before=s.runtimeDungeonHeightEpoch;
   for(int y=0;y<8;y++){(*gfx)[y*2]=direction%2?0:255;(*gfx)[y*2+1]=direction%2?255:0;}
   run();check(s.runtimeDungeonHeightEpoch>before,"occurrence-dependent animation reused stale proof");
   const bool active=direction%2==0;
   check((*s.dungeonRetainedBg2)[10*64+10].stairSourceAmbiguous==active,"stale occurrence source applicability");
   auto retained=*s.dungeonRetainedBg2;
   differential();
   for(int i=0;i<4096;i++)check(retained[i].sameGeometry((*s.dungeonRetainedBg2)[i]),"retained classification differs from fresh classification");
  }
  auto profileChange=[&](const char *why) {
   auto key=s.dungeonGeometryProfileKey;
   S9xRemasterSetProfile(profile);run();check(s.dungeonGeometryProfileKey!=key,why);differential();
  };
  at.x++;profile.contextBindings[at]=profile.contextBindings.begin()->second;
  profile.contextBindings.erase(profile.contextBindings.begin());profileChange("binding move must change geometry key");
  profile.tileContexts[a].erase("two");profileChange("two-to-one context activation must change key");
  profile.tileContexts[a]["two"]={};profileChange("one-to-two context activation must change key");
  profile.tileContexts.clear();profile.contextBindings.clear();S9xRemasterSetProfile(profile);run();
  const auto *animatedMetadata=S9xRemasterFrameMetadataForTile(*f,b);
 check(animatedMetadata && animatedMetadata->emissionRgba==profile.assets[b].emissionRgba,"animated emission stale");
 epoch=s.runtimeDungeonHeightEpoch;c.linkX++;c.backgroundScrollX++;c.hasTerrainActors=true;c.terrainActors[0].state=10;run();
 check(s.runtimeDungeonHeightEpoch==epoch,"actor/camera invalidation");
 auto fallback=[&](const char *why){auto e=s.runtimeDungeonHeightEpoch;run();check(s.runtimeDungeonHeightEpoch>e,why);differential();};
 profile.assets[a].height.fill(3);S9xRemasterSetProfile(profile);fallback("profile rebuild");
 for(int y=0;y<8;y++){(*gfx)[y*2]=255;(*gfx)[y*2+1]=0;}fallback("changed relief animation must solve");
 auto stateHits=s.dungeonArtworkStateHits;
 for(int y=0;y<8;y++){(*gfx)[y*2]=0;(*gfx)[y*2+1]=255;}run();
 for(int y=0;y<8;y++){(*gfx)[y*2]=255;(*gfx)[y*2+1]=0;}run();
 check(s.dungeonArtworkStateHits>stateHits,"exact solved animation state reuse");differential();
 (*attr)[12*64+12]=0x70;fallback("pickup/blocker attributes must solve");
 (*maps)[(12*64+12)*2+1]^=0x40;fallback("flip must solve");
 (*maps)[1]^=0x20;fallback("priority word must solve");
 c.linkPlane=1;fallback("plane must solve");
 c.stairCount=1;c.stairs[0]={12,12,1,0,false,true};fallback("stair must solve");
 (*tpl)[12875]^=1;fallback("complete template dependency must solve");
  c.roomIndex++;fallback("room transition must solve");
  check(!S9xRemasterDungeonResearchEnabled() && !S9xRemasterDungeonWritesStorage(),"default allocated/enabled observer");
  // Same input, toggle only: both transitions must revoke all geometry caches.
  for(bool enabled:{true,false,true,false}) {
   auto session=s.dungeonSession;
   S9xRemasterSetDungeonResearchEnabled(enabled);
   check(s.dungeonSession>session && !s.dungeonSnapshotValid && !s.runtimeDungeonHeightValid && s.dungeonArtworkStates.empty(),"research toggle retained cache");
   fallback("research toggle must solve");
   auto &trace=S9xRemasterDungeonWrites();trace.enable(true);trace.room=c.roomIndex;trace.cells[0].owner=42;
   // Reset/defrost/game-switch callers all use this continuity boundary.
   S9xRemasterInvalidateDungeonContinuity();
   check(trace.room==0xffff && !trace.cells[0].owner,"continuity boundary retained stale trace");
   run();
  }
 // Exact activated geometry dependencies: defaults/masks/normals never ignored.
 auto metadata=profile;
 metadata.assets[a]=metadata.assets[b];check(S9xRemasterDungeonArtworkGeometryEquivalent(metadata,a.hash,b.hash),"equal metadata");
 metadata.assets[a].hasNormals=true;metadata.assets[a].normalXyz[0]=9;
 check(!S9xRemasterDungeonArtworkGeometryEquivalent(metadata,a.hash,b.hash),"normals dependency");
 metadata.assets[a]=metadata.assets[b];metadata.assets[a].hasOcclusion=true;metadata.assets[a].occlusion[63]=0;
 check(!S9xRemasterDungeonArtworkGeometryEquivalent(metadata,a.hash,b.hash),"occlusion dependency");
  metadata.assets.erase(a);check(!S9xRemasterDungeonArtworkGeometryEquivalent(metadata,a.hash,b.hash),"missing metadata dependency");
  {uint8_t blank[64]={};RemasterProfile empty;
   check(!S9xRemasterDungeonArtworkGeometryEquivalent(empty,S9xRemasterHashTile(4,blank),a.hash),"blank/nonblank border dependency");}
 // Inventory contract includes all fields, including observer and ownership-only fields.
 RemasterDungeonFloorContext::Placement t,u;
#define DIFFERENT(field,value) u=t;u.field=value;check(!t.sameGeometry(u),#field)
 DIFFERENT(surface,RemasterDungeonFloorContext::Surface::Wall);u=t;u.height[63]=1;check(!t.sameGeometry(u),"height");
 DIFFERENT(groundedFloorMask,1);DIFFERENT(doorframeMask,1);DIFFERENT(hasDoorframeMask,true);
 DIFFERENT(doorTemplateOffset,1);DIFFERENT(doorAssembly,1);DIFFERENT(doorDirection,0);DIFFERENT(splitDoorPart,true);
  DIFFERENT(hasHeight,true);DIFFERENT(stairSourceAmbiguous,true);DIFFERENT(removableSupportPlanes,1);DIFFERENT(hflip,true);DIFFERENT(vflip,true);
 DIFFERENT(floorPlaneMask,1);DIFFERENT(distinctLowerUsage,true);DIFFERENT(structuralFloorPlaneMask,0);
 DIFFERENT(wallTemplateOffset,1);DIFFERENT(supportingTrim,true);DIFFERENT(terminalCorner,true);
 DIFFERENT(highPerimeterFace,true);DIFFERENT(highCornerFace,true);DIFFERENT(lowerDoorWallPart,true);
 DIFFERENT(diagonalHeightPreview,true);DIFFERENT(diagonalPreviewRow,1);DIFFERENT(diagonalPreviewFoot,1);
 DIFFERENT(diagonalIntercept,-1);DIFFERENT(diagonalMinimum,1);DIFFERENT(diagonalMaximum,1);
 DIFFERENT(semanticSoutheastWall,true);DIFFERENT(semanticBoundaryKnown,true);
 DIFFERENT(diagnosticWallTemplateOffset,1);DIFFERENT(diagnosticUphillDirections,1);DIFFERENT(diagnosticNormalUphill,1);
  DIFFERENT(diagnosticAuthoredFloor,true);DIFFERENT(diagnosticRomFloorTrim,true);
  DIFFERENT(authoredObstacle,true);DIFFERENT(authoredObstacleAssembly,1);DIFFERENT(authoredObstacleGroup,1);
  DIFFERENT(authoredObstacleZ,-1);DIFFERENT(authoredObstacleFlip,2);DIFFERENT(authoredObstacleContact,true);
 u=t;u.tileHash=42;check(t.sameGeometry(u),"producer identity separate");
 std::cout<<"PASS production animation equivalent/rebuild, exact two-BG maps/diagnostics/packets, pickup/flips/priority/planes/stairs/templates/profile/transitions/actors; complete Placement contract\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
