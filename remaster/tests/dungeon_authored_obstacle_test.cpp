#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterProfile p;p.gameTitle="test";p.romSha256=std::string(64,'0');p.upperFloorHeight=50;
 RemasterTileContentId ids[3];
 for(int n=0;n<3;n++) {
  uint8_t pixels[64];std::fill_n(pixels,64,n);
  for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=p.assets[ids[n]];a.tileId=ids[n];a.hasHeight=true;
  for(int k=0;k<64;k++)a.height[k]=n?17+k/8:0;
 }
 p.assetGroups["dungeon_floor"].tileIds={ids[0]};
 // Sparse arbitrary width3/height3 example, repeated identity at DIFFERENT Z.
 // A Z-zero head is explicitly NOT a contact; artwork flips are per member.
 p.obstacleGroups={{"example",{{ids[1],0,0,0,false,true,false},
  {ids[1],0,1,-1,false,false,false},{ids[2],0,2,2,true,false,true},
  {ids[2],2,2,-2,false,false,true}}}};
 std::vector<RemasterProfileDiagnostic> errors;std::string text;RemasterProfile decoded;
 need(S9xRemasterSerializeProfile(p,text,errors),"serialize schema22");
 need(S9xRemasterParseProfile(text,decoded,errors) && decoded.schemaVersion==22,"parse schema22");
 need(decoded.obstacleGroups[0].members[1].z==-1 && decoded.obstacleGroups[0].members[0].vflip,"signed/flip roundtrip");
 auto bad=p;bad.obstacleGroups.push_back({"second",{{ids[1],0,0,0,false,false,true}}});
  need(S9xRemasterSerializeProfile(bad,text,errors),"shared group identity rejected");
 bad=p;bad.obstacleGroups[0].members.push_back(bad.obstacleGroups[0].members[0]);
 need(!S9xRemasterSerializeProfile(bad,text,errors),"duplicate position accepted");
 bad=p;for(auto &m:bad.obstacleGroups[0].members)m.supportContact=false;
 need(!S9xRemasterSerializeProfile(bad,text,errors),"implicit Z0 contacts accepted");
 need(S9xRemasterSerializeProfile(p,text,errors),"reserialize");
 auto old=text;old.replace(old.find("schema_version = 22"),19,"schema_version = 21");
 need(!S9xRemasterParseProfile(old,decoded,errors),"old schema accepted groups");
 old=text;old.erase(old.find("support_contact = false"),22);
 need(!S9xRemasterParseProfile(old,decoded,errors),"missing explicit contact key accepted");
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;
 c.constructedMaps=words->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();
 c.stairCount=1;c.stairs[0]={20,20,0,0,true,false};
 auto put=[&](int cell,int n,bool hf,bool vf){int w=n|(hf?0x4000:0)|(vf?0x8000:0);(*words)[cell*2]=w;(*words)[cell*2+1]=w>>8;};
 auto build=[&](int flip) {
  words->fill(0);attrs->fill(1);
  for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int y=24;y<36;y++)for(int x=8;x<30;x++)(*attrs)[y*64+x]=0;
  for(int oy:{14,28})for(const auto &m:p.obstacleGroups[0].members) {
   int cell=(oy+((flip&2)?-m.y:m.y))*64+14+((flip&1)?-m.x:m.x);
   put(cell,m.tileId==ids[1]?1:2,m.hflip!=bool(flip&1),m.vflip!=bool(flip&2));(*attrs)[cell]=1;
  }
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
 };
 // Even exact shared artwork must never turn a movable OBJ group into a BG
 // room footprint or floor constraint.
 p.obstacleGroups[0].spriteAssembly=true;build(0);
 for(const auto &t:*tiles)need(!t.authoredObstacle && !t.authoredObstacleAssembly,"sprite assembly claimed background cells");
 p.obstacleGroups[0].spriteAssembly=false;
 for(int flip=0;flip<4;flip++) {
  build(flip);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  need(!map->structuralConflicts && map->solvedAuthoredObstacleCells==8,"terminal arbitrary footprints");
  for(int oy:{14,28})for(const auto &m:p.obstacleGroups[0].members) {
   int cell=(oy+((flip&2)?-m.y:m.y))*64+14+((flip&1)?-m.x:m.x);
   const auto &t=(*tiles)[cell];need(t.authoredObstacleAssembly && t.authoredObstacleZ==m.z,"match slots");
   need(map->offsets[cell]==(oy==14?50:0),"authored members shared floor only");
   need(t.height==p.assets.at(m.tileId).height && !map->fallbackDirections[cell],"preserve pixels");
  }
  need(map->authoredObstacleEvidence.size()==2 && map->authoredObstacleEvidence[0].witnessedContacts==2,"contact evidence");
  // Whole footprint transforms expire exact entry/geometry evidence.
  auto original=(*tiles)[14*64+14],changed=original;changed.authoredObstacleZ++;
  need(!original.sameGeometry(changed),"Z geometry comparison");
  uint64_t key=S9xRemasterDungeonEntryStateKey(c,50);(*tiles)[14*64+14]=changed;
  need(key!=S9xRemasterDungeonEntryStateKey(c,50),"Z entry cache key");
  build(flip);put(14*64+14,0,false,false);S9xRemasterDungeonPlacements(c,p,*tiles);
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->solvedAuthoredObstacleCells==4 && map->unmatchedAuthoredObstacleCells==3,"partial reject, independent copy survives");
 }
 // Runtime pixel application uses member offset once, retains authored samples.
 build(0);S9xRemasterBuildDungeonHeightMap(c,50,*map);
 {RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId=ids[1];
  i.assetGroup="stair_treads";f.tileInstances={i};
  RemasterFrameAssetMetadata metadata;metadata.tileId=ids[1];metadata.tileRole=RemasterTileRole::StairTop;metadata.hasHeight=true;metadata.height=p.assets.at(ids[1]).height;
  f.assetMetadata.push_back(metadata);
  for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
  c.backgroundScrollX=112;c.backgroundScrollY=120;S9xRemasterApplyDungeonHeightMap(f,c,*map);
  const auto &applied=f.tileInstances[f.mainPixels[0].instanceId-1];
  need(applied.heightOffset==50 && !applied.hasPlacementHeight,"member runtime composition");
 }
 // Both BGs, main/sub and independent lamp-free scene path use the same signed
 // member offset without clamping, border ramps or local-preview substitution.
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int base:{-24,0,42,83}) {
  auto f=std::make_unique<RemasterFrame>();f->width=f->height=8;f->mainPixels.resize(64);f->subPixels.resize(64);
  RemasterFrameAssetMetadata a;a.tileId=ids[1];a.hasHeight=true;a.height=p.assets.at(ids[1]).height;a.tileRole=RemasterTileRole::Door;
  f->assetMetadata.push_back(a);RemasterFrameTileInstance i;i.tileId=ids[1];i.source=RemasterSourceType::Background;i.sourceIndex=bg;
  i.hFlip=flip&1;i.vFlip=flip&2;i.assetGroup="stair_treads";f->tileInstances={i};
  for(int k=0;k<64;k++){f->mainPixels[k].instanceId=f->subPixels[k].instanceId=1;f->mainPixels[k].tilePixel=f->subPixels[k].tilePixel=k;}
  auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=true;m->offsets.fill(255);m->offsets[0]=base;
  m->producerHashes[0]=ids[1].hash;m->producerFlips[0]=flip;m->producerObstacle[0]=m->producerAuthoredObstacle[0]=true;
  m->fallbackDirections[0]=15;m->mixedFloorBases[0][0]=base+12;
  RemasterDungeonHeightMap::LocalHeightPreview preview;preview.cell=0;preview.height.fill(5);m->localHeightPreviews.push_back(preview);
  c.backgroundScrollX=c.backgroundScrollY=0;S9xRemasterApplyDungeonHeightMap(*f,c,*m,nullptr,nullptr,bg);
  for(const auto *pixels:{&f->mainPixels,&f->subPixels})for(const auto &px:*pixels) {
   const auto &applied=f->tileInstances[px.instanceId-1];need(applied.heightOffset==base && !applied.hasPlacementHeight,"both BG/buffer composition");
  }
  need(f->assetMetadata[0].height==a.height,"authored local array changed");
  auto scene=std::make_unique<RemasterFrame>();scene->width=scene->height=8;scene->mainPixels=f->mainPixels;scene->tileInstances={i};scene->assetMetadata={a};
  S9xRemasterApplyDungeonHeightMap(*scene,c,*m,nullptr,nullptr,bg);
  need(scene->tileInstances[0].heightOffset==base && !scene->tileInstances[0].hasPlacementHeight,"scene composition");
 }
 // Missing one contact rejects ALL parts; a nearby noncontact floor cannot rescue.
	build(0);c.previousRoomIndex=8;
	c.entrySupport={17*64+14,-16,8,123,456,S9xRemasterDungeonEntryStateKey(c,50)};
	need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->entrySupportConsumed && map->offsets[15*64+14]==-16,"negative evidenced floor clipped");
	c.entrySupport={};
 // Mixed members preserve authored locals and generate only placement fallback.
 for(int flip=0;flip<4;flip++) {
  p.assets[ids[2]].hasHeight=false;build(flip);S9xRemasterBuildDungeonHeightMap(c,50,*map);
  for(const auto &member:p.obstacleGroups[0].members) {
   const int cell=(14+((flip&2)?-member.y:member.y))*64+14+((flip&1)?-member.x:member.x);
   const bool authored=member.tileId==ids[1];const auto &t=(*tiles)[cell];
   need(t.authoredObstacleAssembly && t.hasHeight==authored,"missing local matching/source");
   need(map->offsets[cell]==50+(authored?0:member.z*8),"mixed authored/fallback offset");
   if(!authored) {
    RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
    RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId=member.tileId;i.hFlip=t.hflip;i.vFlip=t.vflip;
    f.tileInstances={i};for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=(t.vflip?7-k/8:k/8)*8+(t.hflip?7-k%8:k%8);}
    c.backgroundScrollX=cell%64*8;c.backgroundScrollY=cell/64*8;S9xRemasterApplyDungeonHeightMap(f,c,*map);
    for(int k=0;k<64;k++){const auto &a=f.tileInstances[f.mainPixels[k].instanceId-1];need(a.hasPlacementHeight && a.placementHeight[f.mainPixels[k].tilePixel]==7-k/8,"world vertical fallback flip");}
   }
  }
  need(!p.assets[ids[2]].hasHeight,"fallback wrote metadata");
 }
 p.assets[ids[2]].hasHeight=true;
 build(0);for(int y=17;y<20;y++)(*attrs)[y*64+16]=1;
 S9xRemasterBuildDungeonHeightMap(c,50,*map);
 need(map->offsets[14*64+14]==255,"unsupported contact guessed");
 // Both planes are active; equal floor values do not prove identical ownership.
 build(0);c.stairs[0].changesPlane=true;
 for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[4096+y*64+x]=(*attrs)[y*64+x];
 S9xRemasterBuildDungeonHeightMap(c,50,*map);
  need(map->offsets[14*64+14]==255 && map->structuralConflicts,"plane ambiguity accepted");
  // Exact distinct lower floor beneath ALL members identifies the non-owning
  // underpass. Both contacts still must agree; neither highest nor actor plane
  // selects ownership. Lower explicit Floor role need not be in an asset group.
  for(int flip=0;flip<4;flip++) {
   build(flip);c.stairs[0].changesPlane=true;p.assetGroups.erase("dungeon_floor");p.assets[ids[0]].tileRole=RemasterTileRole::Floor;
   for(int y=8;y<20;y++)for(int x=8;x<30;x++)(*attrs)[4096+y*64+x]=0;
   S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
   S9xRemasterBuildDungeonHeightMap(c,50,*map);
   need(map->offsets[(14+(flip&2?-1:1))*64+14]!=255,"proved upper body lost to lower occupancy");
   const auto &t=(*tiles)[(14+(flip&2?-1:1))*64+14];need(t.distinctLowerUsage && (t.floorPlaneMask&2),"group reset lost explicit lower-floor proof");
   p.assets[ids[0]].height[0]=1;S9xRemasterDungeonPlacements(c,p,*tiles);S9xRemasterBuildDungeonHeightMap(c,50,*map);
   need(map->offsets[(14+(flip&2?-1:1))*64+14]==255,"nonflat lower metadata falsely proves ownership");
   p.assets[ids[0]].height[0]=0;
  }
  p.assetGroups["dungeon_floor"].tileIds={ids[0]};
   c.stairs[0].changesPlane=false;
  // Interior contacts share the exact assembly's ground edge. A one-cell-deep
  // floor strip below furniture is solved geometry, not full-body occupancy.
  // Its explicit zero-local floor path must reach ONE independent component.
  p.obstacleGroups[0].members[1].supportContact=true;
  build(0);
  for(int y=18;y<20;y++)for(int x=12;x<=17;x++)(*attrs)[y*64+x]=1;
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();
  S9xRemasterBuildDungeonHeightMap(c,50,*map);
  need(map->offsets[14*64+14]==50 && map->authoredObstacleEvidence[0].witnessedContacts==3,"interior/narrow floor contact lost");
  // A missing contact cannot be rescued through unknown artwork at the edge.
  (*tiles)[17*64+14].surface=C::Surface::Unknown;
  S9xRemasterBuildDungeonHeightMap(c,50,*map);
  need(map->offsets[14*64+14]==255,"unknown strip promoted to floor support");
  p.obstacleGroups[0].members[1].supportContact=false;
  // Declared pillar feet may contact ordinary floor on a zero-height SIDE,
  // even when the outward bottom is covered by a platform border. Body/head
  // proximity and raised/nonzero edges are not support evidence.
  const auto savedGroups=p.obstacleGroups;const auto savedHeight=p.assets[ids[1]].height;
  p.obstacleGroups[0].members={{ids[1],0,0,0,false,false,true}};
  for(int flip=0;flip<4;flip++) {
   build(flip);const int cell=14*64+14;
   auto &t=(*tiles)[cell];t.height.fill(7);
   for(int y=0;y<8;y++)t.height[y*8+7]=0;
   const int step=flip&2?-1:1;(*attrs)[(14+step)*64+14]=1;
   need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[cell]==50,"declared zero-edge side foot");
   t.height.fill(7);
   need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[cell]==255,"nonzero side edge borrowed floor");
  }
  p.obstacleGroups=savedGroups;p.assets[ids[1]].height=savedHeight;
  // Shared motif, different complete footprints: all mirrors identify the
  // two variants independently. A shorter complete subset vetoes BOTH owners.
  for(int flip=0;flip<4;flip++) {
   p.obstacleGroups={{"east",{{ids[1],0,0,0,false,false,false},{ids[2],1,0,0,false,false,true}}},
    {"west",{{ids[1],0,0,0,false,false,false},{ids[1],1,0,0,false,true,true}}}};
   words->fill(0);
   for(int gi=0;gi<2;gi++)for(const auto &m:p.obstacleGroups[gi].members)
    put(14*64+14+gi*10+((flip&1)?-m.x:m.x),m.tileId==ids[1]?1:2,m.hflip!=bool(flip&1),m.vflip!=bool(flip&2));
   S9xRemasterDungeonPlacements(c,p,*tiles);
   for(int gi=0;gi<2;gi++)need((*tiles)[14*64+14+gi*10].authoredObstacleGroup==gi+1 &&
    (*tiles)[14*64+14+gi*10].authoredObstacleAssembly,"shared motif complete footprint lost");
   p.obstacleGroups.push_back({"subset",{{ids[1],0,0,0,false,false,true}}});
   S9xRemasterDungeonPlacements(c,p,*tiles);
   for(int gi=0;gi<2;gi++)for(int x=0;x<2;x++)need(!(*tiles)[14*64+14+gi*10+((flip&1)?-x:x)].authoredObstacleAssembly,"cross-group overlap accepted");
  }
  p.obstacleGroups.resize(1);
  // Overlapping exact A,A,A runs imply two owners for a two-member example.
 p.obstacleGroups[0].members={{ids[1],0,0,0,false,false,true},{ids[1],1,0,1,false,false,false}};
 words->fill(0);attrs->fill(1);for(int x=10;x<13;x++)put(14*64+x,1,false,false);
 S9xRemasterDungeonPlacements(c,p,*tiles);
 for(int x=10;x<13;x++)need(!(*tiles)[14*64+x].authoredObstacleAssembly,"overlap accepted");
 // Exact bilateral symmetry is safe ONLY when every repeated slot has identical
 // member semantics. H swaps the two identities and expected artwork flips.
 p.obstacleGroups[0].members={{ids[1],0,0,1,false,false,true},{ids[1],1,0,1,true,false,true}};
 words->fill(0);put(14*64+10,1,false,false);put(14*64+11,1,true,false);
 S9xRemasterDungeonPlacements(c,p,*tiles);
 need((*tiles)[14*64+10].authoredObstacleAssembly && (*tiles)[14*64+11].authoredObstacleAssembly,"safe H symmetry rejected");
 p.obstacleGroups[0].members[1].z=2;S9xRemasterDungeonPlacements(c,p,*tiles);
 need(!(*tiles)[14*64+10].authoredObstacleAssembly,"different Z symmetric ownership guessed");
 // Exact runtime profile identity invalidation on every group property.
 p.obstacleGroups[0].members={{ids[1],0,0,0,false,false,true}};
 c.groundedEvidence=true;S9xRemasterSetLiveFramesEnabled(true);uint16_t screen[64]={};auto &s=S9xRemasterState();
 auto frame=[&]{S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 S9xRemasterSetProfile(p);frame();auto key=s.dungeonGeometryProfileKey;auto epoch=s.runtimeDungeonHeightEpoch;
 for(int change=0;change<8;change++) {
  auto &m=p.obstacleGroups[0].members[0];
  if(change==0)m.z=-3;if(change==1)m.x=1;if(change==2)m.y=-1;
  if(change==3)m.hflip=true;if(change==4)m.vflip=true;
  if(change==5){m.supportContact=false;p.obstacleGroups[0].members.push_back({ids[2],2,2,0,false,false,true});}
  if(change==6)p.obstacleGroups[0].name="renamed";
  if(change==7)p.assets[ids[1]].hasHeight=false;
  S9xRemasterSetProfile(p);frame();need(s.dungeonGeometryProfileKey!=key && s.runtimeDungeonHeightEpoch>epoch,"group edit cache stale");
  key=s.dungeonGeometryProfileKey;epoch=s.runtimeDungeonHeightEpoch;frame();need(s.runtimeDungeonHeightEpoch==epoch,"unchanged cache rebuilt");
 }
 need(!S9xRemasterDungeonArtworkGeometryEquivalent(p,ids[1].hash,ids[2].hash),"identity proof erased footprint");
 // Eight exact animation states retain full authored-group evidence/map fields.
 // State changes are a test-only graphics perturbation, never authored metadata.
 const uint8_t original=(*gfx)[32];std::vector<std::string> inventories;
 for(int state=0;state<8;state++) {
  (*gfx)[32]=original^state;frame();
  inventories.push_back(dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap));
 }
 for(int state=0;state<8;state++) {
  (*gfx)[32]=original^state;frame();
  need(inventories[state]==dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"eight-state exact evidence reuse");
  const auto before=inventories[state];s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;frame();
  need(before==dungeonMapInventory(s.runtimeDungeonHeightMap)+dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"cached/full authored evidence differs");
 }
 std::cout<<"PASS schema22 strict roundtrip/ownership, sparse repeated identities/different signed Z, all mirrors/two copies, explicit contacts, partial/overlap/missing/plane-conflict rejection, authored pixels/runtime composition, exact cache invalidation\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
