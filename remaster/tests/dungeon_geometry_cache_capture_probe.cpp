// Complete own-input gameplay packet -> production EndFrame differential artifact.
// No animation synthesis or current-profile substitution in this probe.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);check(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 check(argc==3,"usage: complete-own-input-capture output-prefix");const std::string path=argv[1],out=argv[2];
 auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 check(input.size()==90120 && !std::memcmp(input.data(),"ALTPDI1\0",8),"invalid complete input");
 check(templates.size()==12884 && !std::memcmp(templates.data(),"ALTPDT1\0",8),"invalid complete templates");
 const std::string text(meta.begin(),meta.end());
 check(text.find("66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")!=std::string::npos,"ROM pin");
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"missing state key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;check(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"profile");
 std::string serialized;check(S9xRemasterSerializeProfile(profile,serialized,errors),"serialization");
 const std::string migratedSerialized=serialized;
 // Versioned legacy evidence hash, not a substitute current-profile pin. The
 // parser migrated Door->Doorway; reproduce only the old serialized grammar
 // when checking an unchanged historical companion profile (<34).
 if(profile.schemaVersion<34) {
  const auto lineEnd=serialized.find('\n');serialized.replace(0,lineEnd,"schema_version = "+std::to_string(profile.schemaVersion));
  for(size_t at=0;(at=serialized.find("tile_role = \"doorway\"",at))!=std::string::npos;)serialized.replace(at,21,"tile_role = \"door\"");
  for(size_t at=0;(at=serialized.find("doorway_support = false\n",at))!=std::string::npos;)serialized.erase(at,std::string("doorway_support = false\n").size());
 }
 uint64_t digest=UINT64_C(14695981039346656037);for(unsigned char b:serialized)digest=(digest^b)*UINT64_C(1099511628211);
 std::ostringstream hex;hex<<std::hex<<digest;check(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")!=std::string::npos,"profile pin");
 auto captured=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*captured),"packet");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
  c.submodule=number("submodule");c.collisionMode=number("collision_mode");
  // Additive lifecycle attestations; historical sidecars did not retain these.
   if(text.find("\"module\":")!=std::string::npos)c.module=number("module");
   if(text.find("\"saved_module\":")!=std::string::npos)c.savedModule=number("saved_module");
   const bool dialogAblation=std::getenv("ALTTP_DIALOG_HEIGHT_REGRESSION")!=nullptr;
   check(!c.collisionMode && (!c.submodule || (dialogAblation && c.module==14 && c.submodule==2)),"unsupported/unsettled state");
   if(dialogAblation && c.module==14 && c.savedModule==255) {
    // Old room002 evidence omitted $010c. This is an explicit controlled
    // lifecycle ablation, NOT a newly attested historical saved-module pin.
    c.savedModule=7;std::cout<<"Legacy dialog saved_module=7 supplied for controlled ablation; not captured evidence\n";
   }
  if(text.find("\"construction_phase\":")!=std::string::npos)c.constructionPhase=number("construction_phase");
  if(text.find("\"straight_stair_plane\":")!=std::string::npos)c.straightStairPlane=number("straight_stair_plane");
  if(text.find("\"room_transition_flags\":")!=std::string::npos)c.roomTransitionFlags=number("room_transition_flags");
   if(text.find("\"attribute_construction_state\":")!=std::string::npos)c.attributeConstructionState=number("attribute_construction_state");
	 c.hasCompletedTransitionAttributes=text.find("\"completed_transition_attributes\":true")!=std::string::npos;
  if(text.find("\"scroll_direction\":")!=std::string::npos)c.scrollDirection=number("scroll_direction");
  if(text.find("\"destination_origin\":")!=std::string::npos){c.destinationOriginX=number("destination_origin");c.destinationOriginY=number("destination_origin",1);}
  c.hasDestinationOrigin=text.find("\"destination_origin_attested\":true")!=std::string::npos;
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
 // Legacy evidence pins ROM SHA256 but predates continuity identity/grounded
 // attestations. Preserve its original conservative no-continuity capability.
 c.romIdentity=text.find("\"rom_identity\":")!=std::string::npos?number("rom_identity"):0;
 c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
 c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
 for(int l=0;l<2;l++){c.ppuMapBase[l]=number("ppu_map_base",l);c.ppuMapSize[l]=number("ppu_map_size",l);}
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
  if(std::ifstream(path+".terrain-input.bin").good())check(S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c),"terrain evidence");
  if(std::ifstream(path+".sprite-input.bin").good())check(S9xRemasterReadSpriteObstacleEvidence(path+".sprite-input.bin",c),"sprite evidence");
  if(const char *crop=std::getenv("ALTTP_MANTLE_CAMERA_CROP")) {
   // Controlled camera ablation, never a claim that the input capture displayed
   // this camera. Crop existing observations only: no hidden receivers/ink added.
   const int delta=std::stoi(crop);check(delta>0 && delta<int(captured->height),"camera crop range");
   check(c.hasObstacleActors && c.hasTerrainActors,"camera crop producer evidence");
   const auto &actor=c.obstacleActors[0];check(actor.type==0xee && actor.state==9,"camera crop mantle actor");
   const int sy=int16_t(uint16_t(actor.y-c.actorScrollY));
   const int sx=int16_t(uint16_t(actor.x-c.actorScrollX));
   const int dx[]={32,16,0,32,16,0},dy[]={16,16,16,0,0,0};
   const uint8_t names[]={0x2c,0x2e,0x2c,0x0c,0x0e,0x0c},flags[]={0x71,0x31,0x31,0x71,0x31,0x31};
   for(int part=0;part<6;part++) {
    auto &o=c.terrainOam[116+part];
    check(o.x==((sx+dx[part])&511) && o.y==uint8_t(sy+dy[part]) && o.name==names[part] && o.flags==flags[part] && o.size==2,"camera crop complete original dispatch");
    // Independent reference-engine Sprite_CorrectOamEntries arithmetic.
    o.y=uint16_t(sy+dy[part]-delta+16)>=256?0xf0:uint8_t(sy+dy[part]-delta);
   }
   auto shift=[&](auto &buffer) {
    if(buffer.empty())return;
    const size_t n=size_t(delta)*captured->width;
    std::move(buffer.begin()+n,buffer.end(),buffer.begin());
    std::fill(buffer.end()-n,buffer.end(),typename std::decay_t<decltype(buffer)>::value_type{});
   };
   shift(captured->mainPixels);shift(captured->subPixels);shift(captured->scenePixels);
   shift(captured->originalRgb555);shift(captured->sceneRgb555);
   c.backgroundScrollY+=delta;c.actorScrollY+=delta;
   std::cout<<"CONTROLLED camera crop delta="<<delta<<" logical_mantle_y="<<sy-delta<<" (not captured camera)\n";
  }
 size_t matched=0;
 for(auto *pixels:{&captured->mainPixels,&captured->subPixels,&captured->scenePixels})for(size_t at=0;at<pixels->size();at++) {
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=captured->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
  const int x=(c.backgroundScrollX+at%captured->width)&511,y=(c.backgroundScrollY+at/captured->width)&511;
  const int address=(y/8*64+x/8)*2+(i.sourceIndex?0:8192),word=c.constructedMaps[address]|c.constructedMaps[address+1]<<8;
  uint8_t decoded[64];const int base=i.sourceIndex?c.graphicsBase:(c.bg1GraphicsBase==0xffff?c.graphicsBase:c.bg1GraphicsBase);
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++) {int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(base+(word&1023)*32+b/2*16+yy*2+b%2)]>>(7-xx))&1)<<b;decoded[yy*8+xx]=v;}
  if(!(i.tileId.hash==S9xRemasterHashTile(4,decoded) && i.hFlip==bool(word&0x4000) && i.vFlip==bool(word&0x8000) &&
   px.tilePixel==((word&0x8000?7-y%8:y%8)*8+(word&0x4000?7-x%8:x%8)))) {
   std::cerr<<"mismatch buffer="<<(pixels==&captured->mainPixels?"main":pixels==&captured->subPixels?"sub":"scene")
    <<" pixel="<<at<<" bg="<<int(i.sourceIndex)<<" cell="<<x/8<<','<<y/8<<" word="<<word
    <<" captured_hash="<<i.tileId.hash<<" input_hash="<<S9xRemasterHashTile(4,decoded)<<'\n';
   throw std::runtime_error("stale packet/maps/camera");
  }matched++;
 }
 S9xRemasterDungeonStairsFromAttributes(c);
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);S9xRemasterInvalidateDungeonContinuity();auto &s=S9xRemasterState();
 auto frame=std::make_unique<RemasterFrame>();
 double endframeMs=0;
 auto run=[&]{
#ifdef ALTTP_TEST_DISABLE_GEOMETRY_REUSE
  // Bounded baseline: ablate cache only, retaining identical classifier/solver.
  s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;
#endif
  S9xRemasterBeginFrame(captured->width*captured->height,captured->width,captured->width,captured->height);
  s.tileInstances=captured->tileInstances;for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
  for(const auto &a:captured->assets){auto &t=s.observedTiles[a.tileId.hash];t.hash=a.tileId.hash;t.bitDepth=a.tileId.bitDepth;std::copy_n(a.indices,64,t.indices);}
  for(size_t at=0;at<captured->mainPixels.size();at++) {
   s.mainOwners[at]=captured->mainPixels[at].owner;s.subOwners[at]=captured->subPixels[at].owner;
   s.mainInstanceIds[at]=captured->mainPixels[at].instanceId;s.subInstanceIds[at]=captured->subPixels[at].instanceId;
   s.mainTilePixels[at]=captured->mainPixels[at].tilePixel;s.subTilePixels[at]=captured->subPixels[at].tilePixel;
  }
  s.scenePixels=captured->scenePixels;s.sceneRgb=captured->sceneRgb555;
   const auto start=std::chrono::steady_clock::now();
   RemasterHeldLampState controlledLamp;
   const bool lampAblation=std::getenv("ALTTP_LINK_THRESHOLD_REGRESSION")!=nullptr;
   if(lampAblation){controlledLamp.verifiedRom=controlledLamp.darkWithLantern=true;controlledLamp.facing=0;
    controlledLamp.linkScreenX=int(c.linkX)-int(c.backgroundScrollX);controlledLamp.linkScreenY=int(c.linkY)-int(c.backgroundScrollY);}
   S9xRemasterEndFrame(captured->originalRgb555.data(),captured->width,captured->width,captured->height,frame.get(),&c,lampAblation?&controlledLamp:nullptr);
  endframeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  if(std::getenv("ALTTP_CACHE_TIMING"))std::cerr<<"endframe_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
   check(!frame->mainPixels.empty(),"EndFrame");
   if(std::getenv("ALTTP_THRESHOLD_ART_REGRESSION")) {
    check(c.roomIndex==0x61,"061083 art fixture required");
    size_t samples=0;
    for(size_t at=0;at<frame->scenePixels.size();at++) {
     const auto &px=frame->scenePixels[at];if(!px.instanceId)continue;
     const auto &i=frame->tileInstances[px.instanceId-1];
     const int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8;
     if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || x>1 || (y!=16 && y!=17))continue;
     check(i.tileId.hash==UINT64_C(0xc40518b112d0814e),"061083 selected art identity");
     check(i.heightOffset==0 && !i.hasPlacementHeight,"actor traversal must not move legacy BG art");
     samples++;
    }
    check(samples==256,"061083 selected art denominator");
    std::cout<<"PASS 061083 selected BG2 legacy art: 256 samples retain base0/local60\n";
   }
  if(std::getenv("ALTTP_LINK_THRESHOLD_REGRESSION")) {
   check(c.roomIndex==0x55 && c.linkX==2680 && (c.linkY==3021 || c.linkY==3001) && c.linkPlane==0,"threshold capture state pin");
   const auto base=S9xRemasterLinkFloorHeight(c,s.runtimeDungeonHeightMap);
   std::cout<<"link_support="<<base<<" feet_cell="<<((c.linkX+8)&511)/8<<','<<((c.linkY+20)&511)/8<<'\n';
   check(base==24,"captured Link threshold base must be 24");
   size_t members=0;
   for(const auto &i:frame->tileInstances)if(i.source==RemasterSourceType::Object && i.sourceIndex>=100 && i.sourceIndex<112) {
    check(i.heightOffset==24 && !i.hasPlacementHeight,"body/equipment/shadow shared base, intrinsic relief unchanged");members++;
   }
   check(members>0,"reserved Link OAM denominator");
   check(frame->heldLampLightIndex<frame->lights.size(),"controlled lamp emitted through actual EndFrame");
   check(std::abs(frame->lights[frame->heldLampLightIndex].z-(12+24*frame->lightingCoordinateScale/255.0f))<0.0001f,"held lamp uses same solved support");
  }
  // Captured production regression, not synthetic actor geometry. Own019 pins
  // the four missing intrinsic sources independently from the repaired base.
   if(!dialogAblation && path.find("frame_room062_019.s9xrmf")!=std::string::npos) {
   const auto &a=c.terrainActors[15];const auto &o=c.terrainOam[12];
   check(a.state==10 && a.kind==2 && a.z==16 && o.flags==0x6c && o.size==2,"captured carried producer pin");
   size_t usages=0;std::set<uint64_t> children;
   for(const auto &px:frame->mainPixels)if(px.instanceId) {
    const auto &i=frame->tileInstances[px.instanceId-1];if(i.source!=RemasterSourceType::Object || i.sourceIndex!=12)continue;
    check(i.palette==6 && i.hFlip && !i.vFlip && i.heightOffset==43 && !i.hasPlacementHeight,"captured owned carried base 24+16+3");
    children.insert(i.tileId.hash);usages++;
    for(const auto &m:frame->assetMetadata)if(m.tileId==i.tileId)check(!m.hasHeight,"missing child source must not be fabricated");
   }
   check(usages>=100 && children.size()==4,"captured complete four-child visible denominator");
   std::cout<<"carried_visible_pixels="<<usages<<" children="<<children.size()<<'\n';
  }
   if(path.find("frame_room061_048.s9xrmf")!=std::string::npos) {
   size_t checked=0;
   for(size_t at=0;at<frame->mainPixels.size();at++) {
    const auto &px=frame->mainPixels[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
    const int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8,row=y-x-30;
    if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || x<2 || x>11 || y<38 || row<0 || row>6)continue;
    check(i.hasPlacementHeight && i.heightOffset==0,"captured southwest continued source");
    check(i.placementHeight[px.tilePixel]==std::max(0,std::min(48,7+8*row-int(px.tilePixel%8)-int(px.tilePixel/8))),"captured southwest exact gradient");checked++;
   }
    check(checked>1000,"captured southwest pixel denominator");
   }
   if(path.find("frame_room061_066.s9xrmf")!=std::string::npos) {
    size_t checked=0;
    for(size_t at=0;at<frame->mainPixels.size();at++) {
     const auto &px=frame->mainPixels[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
     const int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8,row=y+x-15;
     if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || x<6 || x>11 || row<0 || row>3)continue;
     check(i.hasPlacementHeight && i.heightOffset==0,"captured northwest independently solved base");
     check(i.placementHeight[px.tilePixel]==24+std::max(0,std::min(24,31-row*8-int(px.tilePixel%8)-int(px.tilePixel/8))),"captured northwest actual effective gradient");checked++;
    }
    check(checked>500,"captured northwest pixel denominator");
   }
 };
   run();auto epoch=s.runtimeDungeonHeightEpoch;run();
   if(std::getenv("ALTTP_PROFILE_RESUME_REGRESSION")) {
    std::vector<uint8_t> before;check(S9xSerializeRemasterFrame(*frame,before),"resume baseline");
    const auto maps=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
    for(int repeat=0;repeat<3;repeat++) {
     // S9xEngine.resume publishes editor metadata even without any edits.
     S9xRemasterSetProfile(profile);run();
     check(s.runtimeDungeonHeightValid,"profile resume leaves equal-key solve invalid");
     std::vector<uint8_t> after;check(S9xSerializeRemasterFrame(*frame,after),"resume packet");
     check(before==after,"profile resume drops effective frame geometry");
     check(maps==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"resume complete-map invariance");
    }
    std::cout<<"PASS repeated unchanged-profile resume: exact packets and complete BG maps\n";
    epoch=s.runtimeDungeonHeightEpoch;
   }
  if(const char *samples=std::getenv("ALTTP_PERF_GEOMETRY_MODES")) {
   const int count=std::max(10,std::atoi(samples));
   const auto original=c;
   for(int mode=0;mode<5;mode++) {
    std::vector<double> times;uint64_t classifications=0,solves=0,packetDigest=UINT64_C(14695981039346656037),mapDigest=packetDigest;
    std::ofstream packets,maps;const bool exact=std::getenv("ALTTP_PERF_EXACT_MODES");
    if(exact){packets.open(out+"-mode"+std::to_string(mode)+".packets",std::ios::binary);maps.open(out+"-mode"+std::to_string(mode)+".maps");}
    for(int n=0;n<count+5;n++) {
     c=original;
     if(mode==1){s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;s.dungeonClassificationInputs->valid=false;}
     if(mode==2){c.linkX+=n%8;c.backgroundScrollY+=n%8;for(auto &a:c.terrainActors)if(a.state){a.x+=n%8;a.y+=n%4;}}
     if(mode==3)c.linkPlane=n%2;
     if(mode==4)c.hasTerrainActors=false; // actor evidence ablation, not a historical follower-free scene
#ifdef ALTTP_TEST_CACHE_STAGES
     S9xRemasterDungeonCacheStages()={};
#endif
     run();if(n>=5)times.push_back(endframeMs);
     if(n>=5 && exact) {
      std::vector<uint8_t> bytes;check(S9xSerializeRemasterFrame(*frame,bytes),"mode packet serialization");
      packets.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
      for(auto b:bytes)packetDigest=(packetDigest^b)*UINT64_C(1099511628211);
      const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
      maps<<inventory<<'\n';
      for(unsigned char b:inventory)mapDigest=(mapDigest^b)*UINT64_C(1099511628211);
     }
#ifdef ALTTP_TEST_CACHE_STAGES
     classifications+=S9xRemasterDungeonCacheStages().classifications;solves+=S9xRemasterDungeonCacheStages().solves;
#endif
    }
    std::sort(times.begin(),times.end());std::cout<<"geometry_mode="<<mode<<" measured="<<times.size()<<" median_ms="<<times[times.size()/2]<<" p95_ms="<<times[times.size()*95/100]<<" classifications="<<classifications<<" solves="<<solves<<" packet_digest="<<std::hex<<packetDigest<<" map_digest="<<mapDigest<<std::dec<<'\n';
   }
   return 0;
  }
  if(std::getenv("ALTTP_STATIC_STAIR_REGRESSION")) {
   check(c.roomIndex==0x71,"room071 fixture required");
   check(s.runtimeDungeonHeightMap.configuredStairFallbacks==0 && s.runtimeDungeonBg1HeightMap.authoredSupportRelations.size()==2,"both signed authored stair relations");
   check(s.runtimeDungeonBg1HeightMap.structuralConflicts==0 && s.runtimeDungeonBg1HeightMap.solvedWallCells>=564,"northern BG1 conflict not repaired");
   for(int y:{11,12,13})check(s.runtimeDungeonBg1HeightMap.offsets[y*64+12]==(13-y)*8,"north stairwell wall effective base");
   for(int x:{12,19})for(int y:{36,37,38})check(s.runtimeDungeonHeightMap.offsets[y*64+x]==24+(38-y)*8,"southern far wall effective base");
   size_t adjusted=0,otherUnapplied=0;
   for(const auto *pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(size_t at=0;at<pixels->size();at++) {
    const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
    if(i.source!=RemasterSourceType::Background || i.sourceIndex!=0)continue;
    const int cell=((c.backgroundScrollY+at/frame->width)&511)/8*64+((c.backgroundScrollX+at%frame->width)&511)/8;
    const auto &m=s.runtimeDungeonBg1HeightMap;
    if(m.offsets[cell]!=255 && m.offsets[cell]>0 && m.producerHashes[cell]==i.tileId.hash &&
      s.dungeonRetainedBg1->at(cell).surface==RemasterDungeonFloorContext::Surface::Wall) {
     check(i.heightOffset==m.offsets[cell],"solved BG1 actual wall instance base not applied");
     adjusted+=i.heightOffset==m.offsets[cell];otherUnapplied+=i.heightOffset!=m.offsets[cell];
    }
   }
   std::cout<<"actual adjusted BG1 wall pixel usages="<<adjusted<<" other unmatched wall usages="<<otherUnapplied<<'\n';
   std::cout<<"PASS actual room071 static solve: north BG1 base16/8/0, south far walls40/32/24; two authored24 stair relations, configured50 unchanged\n";
  }
  if(const char *phasesPath=std::getenv("ALTTP_TORCH_PHASE_REGRESSION")) {
   const auto phases=read(phasesPath);check(phases.size()==3072,"three ROM DMA phases");
   const auto originalInstances=captured->tileInstances;auto animated=input;
   for(int n=0;n<6;n++) {
    std::copy_n(phases.data()+(n%3)*1024,1024,animated.data()+24584+0x7600);c.graphics=animated.data()+24584;
    captured->tileInstances=originalInstances;captured->assets.clear();std::set<uint64_t> ids;
    for(auto &i:captured->tileInstances)if(i.source==RemasterSourceType::Background && i.sourceIndex<2) {
     const int base=i.sourceIndex?c.graphicsBase:c.bg1GraphicsBase;const uint16_t address=uint16_t(base+(i.tileNumber&1023)*32);RemasterFrameAsset a;
     for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;a.indices[y*8+x]=v;}
     a.tileId={S9xRemasterHashTile(4,a.indices),1,4};i.tileId=a.tileId;if(ids.insert(a.tileId.hash).second)captured->assets.push_back(a);
    }
    run();
    for(int x:{44,45,50,51})for(int y:{54,55})check(s.runtimeDungeonHeightMap.offsets[y*64+x]==24,"actual torch animation base lost");
    std::vector<uint8_t> cached;check(S9xSerializeRemasterFrame(*frame,cached),"phase packet");
    s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;s.dungeonClassificationInputs->valid=false;run();
    std::vector<uint8_t> full;check(S9xSerializeRemasterFrame(*frame,full) && full==cached,"phase cached/full effective packet differs");
   }
   std::cout<<"PASS three ROM-proven DMA phases repeated: eight torch members base24, exact cached/full packets; own metadata unchanged\n";
   return 0;
  }
   if(dialogAblation) {
    // Compare placed room geometry, not module-specific actor/emitter policy.
    auto geometry=[&]{std::ostringstream out;for(auto pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(const auto &px:*pixels)if(px.instanceId) {
     const auto &i=frame->tileInstances[px.instanceId-1];if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
     out<<i.tileId.hash<<':'<<i.heightOffset<<':'<<i.hasPlacementHeight<<':';if(i.hasPlacementHeight)out<<i.placementHeight[px.tilePixel];out<<';';
    }return out.str();};
    const auto dialog=geometry();
    const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
    const auto epoch=s.runtimeDungeonHeightEpoch;
    c.module=7;c.submodule=0;c.constructionPhase=0;run();
    check(geometry()==dialog,"dialog/settled effective BG geometry differs");
    check(inventory==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"dialog/settled complete maps differ");
    c.module=14;c.submodule=2;c.savedModule=7;run();
    check(geometry()==dialog,"dialog reentry BG geometry differs");
    check(s.runtimeDungeonHeightEpoch==epoch,"dialog re-solved exact input");
    size_t adjusted=0;
    for(auto pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(const auto &px:*pixels)if(px.instanceId) {
     const auto &i=frame->tileInstances[px.instanceId-1];adjusted+=i.source==RemasterSourceType::Background && i.sourceIndex<2 && (i.heightOffset!=0 || i.hasPlacementHeight);
    }
    check(adjusted>0,"no actual placement heights applied");
    std::cout<<"PASS dialog/settled/reentry exact effective BG geometry and complete two-BG maps; adjusted pixel usages="<<adjusted<<'\n';
   }
   if(std::getenv("ALTTP_DEPARTURE_HEIGHT_REGRESSION")) {
    auto geometry=[&]{std::ostringstream out;for(auto pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(const auto &px:*pixels)if(px.instanceId){const auto &tile=frame->tileInstances.at(px.instanceId-1);if(tile.source!=RemasterSourceType::Background || tile.sourceIndex>1)continue;out<<tile.sourceIndex<<':'<<tile.tileId.hash<<':'<<tile.heightOffset<<':'<<tile.hasPlacementHeight<<':'<<(tile.hasPlacementHeight && px.tilePixel<64?tile.placementHeight[px.tilePixel]:0)<<';';}return out.str();};
    const auto settled=geometry();
    const auto bg2=dungeonMapInventory(s.runtimeDungeonHeightMap),bg1=dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
    c.submodule=2;c.constructionPhase=0;c.linkPlane^=1;
    // Collision starts being overwritten before room/header bookkeeping settles.
    std::fill(input.begin()+16392,input.begin()+24584,1);
    for(int phase=0;phase<8;phase++) {
     c.constructionPhase=phase;
     if(phase>=2) {
      const auto source=s.outgoingGeometry.room;
      c.roomIndex=source+16;c.previousRoomIndex=c.roomIndex;
      c.hasDestinationOrigin=true;c.scrollDirection=0;
      c.destinationOriginX=c.backgroundScrollX&~511;
      c.destinationOriginY=(c.backgroundScrollY&~511)+512;
     }
     run();
     if(geometry()!=settled){std::ofstream(out+"-expected.txt")<<settled;std::ofstream(out+"-actual.txt")<<geometry();std::cerr<<"departure phase="<<phase<<'\n';}
     check(geometry()==settled,"captured outgoing effective packet changed");
     check(s.outgoingGeometry.valid,"captured outgoing snapshot expired");
     check(bg2==dungeonMapInventory(s.previousRuntimeDungeonHeightMap) && bg1==dungeonMapInventory(s.previousRuntimeDungeonBg1HeightMap),"captured frozen complete maps changed");
    }
    std::cout<<"PASS own captured outgoing: exact per-pixel BG bases/local heights and complete both-BG geometry through phases0..7, plane/partial collision/A2 overwrite (controlled lifecycle ablation)\n";
    return 0;
   }
   if(std::getenv("ALTTP_SUBTILE_HEIGHT_REGRESSION")) {
   std::vector<uint8_t> settled;check(S9xSerializeRemasterFrame(*frame,settled),"settled packet");
   const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
   for(int phase=0;phase<9;phase++) {
    // Lifecycle ablation on exact recorded inputs, NOT a captured traversal.
    c.submodule=phase==8?5:1;c.constructionPhase=phase==8?0:phase;run();
    std::vector<uint8_t> current;check(S9xSerializeRemasterFrame(*frame,current) && current==settled,"subtile/shutter effective packet reset");
    check(inventory==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"subtile/shutter map changed");
   }
   c.submodule=c.constructionPhase=0;run();
   std::cout<<"PASS exact captured packet invariant through subtile phases0..7 and shutter handler5 (lifecycle ablation only)\n";
  }
  if(const char *benchmark=std::getenv("ALTTP_PLANE_CACHE_BENCHMARK")) {
   const int count=std::max(4,std::atoi(benchmark));
   double cachedMs=0,fullMs=0;uint64_t cachedClassify=0,fullClassify=0,cachedSolves=0,fullSolves=0;
   std::array<std::string,2> inventories;
   std::array<std::vector<uint8_t>,2> packets;
   for(int n=0;n<count;n++) {
    c.linkPlane=n%2;
    // Only the first sample needs a forced boundary. Subsequent samples must
    // invalidate naturally on the plane change, after the oracle's same plane.
    if(!n){s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;}
#ifdef ALTTP_TEST_CACHE_STAGES
    S9xRemasterDungeonCacheStages()={};
#endif
    const auto before=s.runtimeDungeonHeightEpoch;run();cachedMs+=endframeMs;
    check(s.runtimeDungeonHeightEpoch>before,"plane solve invalidation lost");
    const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
    std::vector<uint8_t> cachedPacket;check(S9xSerializeRemasterFrame(*frame,cachedPacket),"cached plane packet");
#ifdef ALTTP_TEST_CACHE_STAGES
    cachedClassify+=S9xRemasterDungeonCacheStages().classifications;cachedSolves+=S9xRemasterDungeonCacheStages().solves;
#ifndef ALTTP_TEST_DISABLE_CLASSIFICATION_REUSE
    check(!S9xRemasterDungeonCacheStages().classifications,"plane-only classified again");
#endif
#endif
    s.dungeonClassificationInputs->valid=false;s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;
#ifdef ALTTP_TEST_CACHE_STAGES
    S9xRemasterDungeonCacheStages()={};
#endif
    run();fullMs+=endframeMs;
    check(inventory==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"plane cached/full complete inventory differs");
    std::vector<uint8_t> fullPacket;check(S9xSerializeRemasterFrame(*frame,fullPacket) && cachedPacket==fullPacket,"plane cached/full packet differs");
    inventories[c.linkPlane]=inventory;
    packets[c.linkPlane]=std::move(fullPacket);
#ifdef ALTTP_TEST_CACHE_STAGES
    fullClassify+=S9xRemasterDungeonCacheStages().classifications;fullSolves+=S9xRemasterDungeonCacheStages().solves;
#endif
   }
   for(int p=0;p<2;p++){
    std::ofstream file(out+"-plane"+std::to_string(p)+".inventory");file<<inventories[p];
    std::ofstream packet(out+"-plane"+std::to_string(p)+".s9xrmf",std::ios::binary);
    packet.write(reinterpret_cast<const char *>(packets[p].data()),packets[p].size());
   }
   std::cout<<"plane_alternating frames="<<count<<" cached_mean_ms="<<cachedMs/count<<" full_mean_ms="<<fullMs/count
    <<" cached_classifications="<<cachedClassify<<" full_classifications="<<fullClassify
    <<" cached_solves="<<cachedSolves<<" full_solves="<<fullSolves<<" exact_inventory_planes=2\n";
   return 0;
  }
  if(const char *samples=std::getenv("ALTTP_STAIR_RECURRING_SAMPLES")) {
   std::vector<std::vector<uint8_t>> sequence;std::stringstream names(samples);std::string name;
   while(std::getline(names,name,';')){auto sample=read(name+".dungeon-input.bin");check(sample.size()==input.size() && std::equal(input.begin()+8,input.begin()+24584,sample.begin()+8),"recurring topology differs");sequence.push_back(std::move(sample));}
   check(sequence.size()==5,"recurring five real states required");
   const auto originals=captured->tileInstances;
   for(int stair=0;stair<2;stair++) {
    c.submodule=stair?8:0;c.constructionPhase=stair?1:0;
    // Reset only at workload boundary; no full solve interleaved in measured frames.
    s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;s.dungeonArtworkStates.clear();
    std::vector<double> times;uint64_t publications=0,reuses=0,hits=0;std::map<int,std::string> inventories;
    for(int n=0;n<105;n++) {
     c.graphics=sequence[n%5].data()+24584;captured->tileInstances=originals;
     // Refresh exact visible BG identity observations for this recorded VRAM.
     captured->assets.clear();std::set<uint64_t> ids;
     for(auto &i:captured->tileInstances)if(i.source==RemasterSourceType::Background && i.sourceIndex<2) {
      const int base=i.sourceIndex?c.graphicsBase:c.bg1GraphicsBase;const uint16_t address=uint16_t(base+(i.tileNumber&1023)*32);RemasterFrameAsset a;
      for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;a.indices[y*8+x]=v;}
      a.tileId={S9xRemasterHashTile(4,a.indices),1,4};i.tileId=a.tileId;if(ids.insert(a.tileId.hash).second)captured->assets.push_back(a);
     }
     const auto e=s.runtimeDungeonHeightEpoch,r=s.dungeonGeometryReuseCount,h=s.dungeonArtworkStateHits;
     run();const auto inv=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
     if(n<5)inventories[n%5]=inv;else check(inventories[n%5]==inv,"recurring exact state inventory differs");
     if(n>=5){times.push_back(endframeMs);publications+=s.runtimeDungeonHeightEpoch-e;reuses+=s.dungeonGeometryReuseCount-r;hits+=s.dungeonArtworkStateHits-h;}
    }
    std::sort(times.begin(),times.end());
    std::cout<<"recurring stair="<<stair<<" warmup=5 measured=100 median_ms="<<times[50]<<" p95_ms="<<times[95]<<" publications="<<publications<<" reuses="<<reuses<<" exact_hits="<<hits<<" retained_states="<<s.dungeonArtworkStates.size()<<'\n';
   }
   return 0;
  }
  if(const char *samples=std::getenv("ALTTP_STAIR_ANIMATION_SAMPLES")) {
   std::stringstream names(samples);std::string name;int index=0,mismatches=0;double totalMs=0;
   while(std::getline(names,name,';')) {
    auto sample=read(name+".dungeon-input.bin");check(sample.size()==input.size() && std::equal(input.begin()+8,input.begin()+24584,sample.begin()+8),"animation topology differs");
    c.graphics=sample.data()+24584;c.submodule=8;c.constructionPhase=1;
    run();totalMs+=endframeMs;const auto sources=frame->lights;
    double area=0;for(const auto &l:frame->lights)area+=4*3.141592653589793*l.radius*l.radius;
    std::cout<<"animation sample="<<index++<<" sources="<<frame->lights.size()<<" area="<<area<<" epoch="<<s.runtimeDungeonHeightEpoch<<'\n';
    // Independent exact current-state production oracle; no stale producer or
    // animation-equivalent default. Same own profile, complete current inputs.
    c.submodule=0;c.constructionPhase=0;s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();
    bool equal=sources.size()==frame->lights.size();for(size_t n=0;equal && n<sources.size();n++) {
     const auto &a=sources[n],&b=frame->lights[n];equal=a.x==b.x && a.y==b.y && a.z==b.z && a.radius==b.radius && a.intensity==b.intensity && a.red==b.red && a.green==b.green && a.blue==b.blue;
    }
    mismatches+=!equal;
#ifndef ALTTP_TEST_PRIOR_STAIR_ARTWORK_GATE
    check(equal,"stair animation source differs exact settled full solve");
#endif
   }
   std::cout<<"animation_complete_samples="<<index<<" per_source_oracle_mismatches="<<mismatches<<" stair_endframe_total_ms="<<totalMs<<'\n';
   return 0;
  }
  if(std::getenv("ALTTP_STAIR_SOURCE_EVIDENCE")) {
   for(int bg=0;bg<2;bg++)for(int cell=0;cell<4096;cell++) {
    const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
    const uint16_t word=c.constructedMaps[(bg?0:8192)+cell*2]|c.constructedMaps[(bg?0:8192)+cell*2+1]<<8;
    RemasterTileContentId id={m.producerHashes[cell],1,4};auto found=profile.assets.find(id);
    if(found==profile.assets.end() || !found->second.hasEmission)continue;
    const auto &a=found->second;int count=0;double local=0;int previewCount=0;double previewHeight=0;
    for(int pixel=0;pixel<64;pixel++)if(a.emissionRgba[pixel*4+3] && (a.emissionRgba[pixel*4] || a.emissionRgba[pixel*4+1] || a.emissionRgba[pixel*4+2])) {
     count++;local+=a.hasHeight?a.height[pixel]:0;
     for(auto &v:m.localHeightPreviews)if(v.cell==cell){previewCount++;previewHeight+=v.height[pixel];}
    }
    if(!count)continue;
    std::cout<<"emitter bg="<<bg<<" cell="<<cell%64<<','<<cell/64<<" hash="<<std::hex<<id.hash<<std::dec<<" word="<<word<<" base="<<m.offsets[cell]<<" planes="<<m.planeOffsets[0][cell]<<','<<m.planeOffsets[1][cell]<<" authored_mean="<<local/count<<" preview_mean="<<(previewCount?previewHeight/previewCount:-1)<<" depth="<<a.emissionDepth<<" pixels="<<count<<'\n';
   }
  }
 if(const char *benchmark=std::getenv("ALTTP_CACHE_BENCHMARK")) {
  const int count=std::max(20,std::atoi(benchmark));
  std::vector<std::vector<uint8_t>> sequence;
  if(const char *samples=std::getenv("ALTTP_CACHE_GRAPHICS_SAMPLES")) {
   std::stringstream names(samples);std::string name;
   while(std::getline(names,name,';')) {
    auto sample=read(name+".dungeon-input.bin");check(sample.size()==input.size() && std::equal(input.begin()+8,input.begin()+24584,sample.begin()+8),"sample topology differs");
    sequence.push_back(std::move(sample));
   }
  }
  const auto originalInstances=captured->tileInstances;
  for(int mode=0;mode<3;mode++) {
   if(const char *only=std::getenv("ALTTP_CACHE_ONLY_MODE"))if(mode!=std::atoi(only))continue;
#ifdef ALTTP_TEST_CACHE_STAGES
   S9xRemasterDungeonCacheStages()={};
#endif
   std::vector<double> times;uint64_t epochs=0,reuses=0,hits=0;std::map<int,std::string> inventories;
   for(int n=0;n<count+10;n++) {
    if(mode && !sequence.empty()) {
     c.graphics=sequence[n%sequence.size()].data()+24584;
     captured->tileInstances=originalInstances;
     // Real recorded BG artwork identities at each sampled complete VRAM state.
     // RGB/actors remain stock replay; this is CPU replay, not a gameplay trace.
     captured->assets.clear();std::set<uint64_t> ids;
     for(auto &i:captured->tileInstances)if(i.source==RemasterSourceType::Background && i.sourceIndex<2) {
      const int base=i.sourceIndex?c.graphicsBase:c.bg1GraphicsBase;const uint16_t address=uint16_t(base+(i.tileNumber&1023)*32);
      RemasterFrameAsset a;for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;a.indices[y*8+x]=v;}
      a.tileId={S9xRemasterHashTile(4,a.indices),1,4};i.tileId=a.tileId;
      if(ids.insert(a.tileId.hash).second)captured->assets.push_back(a);
     }
    }
    if(mode==2){s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;}
    auto e=s.runtimeDungeonHeightEpoch,r=s.dungeonGeometryReuseCount,h=s.dungeonArtworkStateHits;
    run();const double ms=endframeMs;
    epochs+=s.runtimeDungeonHeightEpoch-e;reuses+=s.dungeonGeometryReuseCount-r;
    hits+=s.dungeonArtworkStateHits-h;
    if(mode && !sequence.empty())inventories[n%sequence.size()]=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
    if(n>=10)times.push_back(ms);
   }
   std::sort(times.begin(),times.end());
   std::cout<<"benchmark mode="<<mode<<" frames="<<count<<" median_ms="<<times[count/2]<<" p95_ms="<<times[count*95/100]<<" epochs="<<epochs<<" reuses="<<reuses<<" exact_state_hits="<<hits<<" sampled_states="<<sequence.size()<<'\n';
#ifdef ALTTP_TEST_CACHE_STAGES
   const auto &stage=S9xRemasterDungeonCacheStages();
   std::cout<<"stage frames="<<count+10<<" classify_calls="<<stage.classifications<<" classify_ms="<<stage.classifyMs
    <<" solve_calls="<<stage.solves<<" solve_ms="<<stage.solveMs<<" proof_calls="<<stage.proofs<<" proof_ms="<<stage.proofMs
    <<" stores="<<stage.stores<<" store_ms="<<stage.storeMs<<" restores="<<stage.restores<<" restore_ms="<<stage.restoreMs<<'\n';
#endif
   for(const auto &entry:inventories){std::ofstream inv(out+"-mode"+std::to_string(mode)+"-state"+std::to_string(entry.first)+".inventory");inv<<entry.second;}
  }
  return 0;
 }
#ifndef ALTTP_TEST_DISABLE_GEOMETRY_REUSE
 check(s.runtimeDungeonHeightEpoch==epoch,"own-input cache invalidation");
#else
 check(s.runtimeDungeonHeightEpoch>epoch,"full baseline not exercised");
#endif
 check(S9xWriteRemasterFrame(*frame,out+".s9xrmf"),"packet write");
 if(std::getenv("ALTTP_PLACEMENT_DIAGNOSTICS")) {
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
  std::ofstream diagnostic(out+"-placements.jsonl");
  auto maps=std::make_unique<std::array<uint8_t,16384>>();
  for(int bg=0;bg<2;bg++) {
   auto dc=c;dc.placementBackgroundIndex=bg;
   if(!bg){std::copy_n(c.constructedMaps+8192,8192,maps->begin());std::copy_n(c.constructedMaps,8192,maps->begin()+8192);dc.constructedMaps=maps->data();dc.graphicsBase=c.bg1GraphicsBase;dc.bg1GraphicsBase=c.graphicsBase;}
   S9xRemasterDungeonPlacements(dc,profile,*tiles);
   const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
   for(int cell=0;cell<4096;cell++) {
    const auto &t=tiles->at(cell);const int address=(bg?0:8192)+cell*2;
    const int word=c.constructedMaps[address]|c.constructedMaps[address+1]<<8;
    diagnostic<<"{\"bg\":"<<bg<<",\"x\":"<<cell%64<<",\"y\":"<<cell/64<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec
     <<"\",\"word\":"<<word<<",\"surface\":"<<int(t.surface)<<",\"template\":"<<t.wallTemplateOffset
      <<",\"has_height\":"<<t.hasHeight<<",\"authored_floor\":"<<t.diagnosticAuthoredFloor
      <<",\"terminal\":"<<t.terminalCorner<<",\"high_corner\":"<<t.highCornerFace<<",\"base\":"<<m.offsets[cell]
     <<",\"planes\":["<<m.structuralPlaneOffsets[0][cell]<<','<<m.structuralPlaneOffsets[1][cell]
     <<"],\"attributes\":["<<int(c.collisionAttributes[cell])<<','<<int(c.collisionAttributes[4096+cell])<<"],\"height\":[";
    for(int p=0;p<64;p++){if(p)diagnostic<<',';diagnostic<<t.height[p];}diagnostic<<"]}\n";
   }
  }
  std::ofstream pixels(out+"-effective.jsonl");
  for(int which=0;which<2;which++) {
   const auto &f=which?*frame:*captured;
   const auto &geometryPixels=f.scenePixels.empty()?f.mainPixels:f.scenePixels;
   for(size_t at=0;at<geometryPixels.size();at++) {
    const auto &px=geometryPixels[at];if(!px.instanceId)continue;const auto &i=f.tileInstances[px.instanceId-1];
    if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
    const auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId);
    const int local=m && m->hasHeight?m->height[px.tilePixel]:0;
    pixels<<"{\"recomputed\":"<<which<<",\"bg\":"<<int(i.sourceIndex)<<",\"x\":"<<((c.backgroundScrollX+at%f.width)&511)/8
     <<",\"y\":"<<((c.backgroundScrollY+at/f.width)&511)/8<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec
     <<"\",\"base\":"<<i.heightOffset<<",\"pixel\":"<<int(px.tilePixel)<<",\"effective\":"<<i.heightOffset+(i.hasPlacementHeight?i.placementHeight[px.tilePixel]:local)<<"}\n";
   }
  }
 }
  std::ofstream inventory(out+".inventory");inventory<<dungeonMapInventory(s.runtimeDungeonBg1HeightMap)<<'\n'<<dungeonMapInventory(s.runtimeDungeonHeightMap)<<'\n';
  std::ofstream actors(out+".actors");
  actors<<"link_support "<<S9xRemasterLinkFloorHeight(c,s.runtimeDungeonHeightMap)<<'\n';
  for(const auto *m:{&s.runtimeDungeonBg1HeightMap,&s.runtimeDungeonHeightMap}) {
   for(int p=0;p<2;p++)for(int i=0;i<4096;i++)actors<<m->actorThresholdOwners[p][i]<<':'<<m->actorThresholdBases[p][i]<<' ';
   actors<<'\n';
  }
 std::ofstream report(out+".json");report<<"{\"room\":"<<c.roomIndex<<",\"profile_pin\":\""<<hex.str()<<"\",\"matched_pixels\":"<<matched<<",\"layers\":[";
 for(int bg=0;bg<2;bg++) {if(bg)report<<',';const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
    report<<"{\"bg\":"<<bg<<",\"conflicts\":"<<m.structuralConflicts<<",\"unknown_walls\":"<<m.unknownWallCells
     <<",\"solved_walls\":"<<m.solvedWallCells<<",\"required_planes\":["<<m.requiredPlaneCells[0]<<','<<m.requiredPlaneCells[1]<<']'
    <<",\"authored_obstacle_cells\":"<<m.authoredObstacleCells<<",\"solved_authored_obstacle_cells\":"<<m.solvedAuthoredObstacleCells
    <<",\"unmatched_authored_obstacle_cells\":"<<m.unmatchedAuthoredObstacleCells
    <<",\"missing\":["<<m.unknownPlaneCells[0]<<','<<m.unknownPlaneCells[1]<<"],\"geometry_conflicts\":[";
  for(size_t k=0;k<m.geometryConflicts.size();k++){if(k)report<<',';const auto &r=m.geometryConflicts[k];report<<'['<<r.node<<','<<r.other<<','<<r.expected<<','<<r.actual<<']';}report<<"]}";
 }report<<"]}\n";
 std::string after;check(S9xRemasterSerializeProfile(s.activeProfile,after,errors) && after==migratedSerialized,"own migrated profile changed");
  std::cout<<"PASS room="<<c.roomIndex<<" profile="<<hex.str()<<" matched="<<matched<<'\n';
  return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
