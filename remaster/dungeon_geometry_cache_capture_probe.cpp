// Complete own-input gameplay packet -> production EndFrame differential artifact.
// No animation synthesis or current-profile substitution in this probe.
#include "remaster.h"
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
 uint64_t digest=UINT64_C(14695981039346656037);for(unsigned char b:serialized)digest=(digest^b)*UINT64_C(1099511628211);
 std::ostringstream hex;hex<<std::hex<<digest;check(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")!=std::string::npos,"profile pin");
 auto captured=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*captured),"packet");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
 c.submodule=number("submodule");c.collisionMode=number("collision_mode");check(!c.submodule && !c.collisionMode,"unsupported/unsettled state");
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
 // Legacy evidence pins ROM SHA256 but predates continuity identity/grounded
 // attestations. Preserve its original conservative no-continuity capability.
 c.romIdentity=text.find("\"rom_identity\":")!=std::string::npos?number("rom_identity"):0;
 c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
 c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
 for(int l=0;l<2;l++){c.ppuMapBase[l]=number("ppu_map_base",l);c.ppuMapSize[l]=number("ppu_map_size",l);}
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
 if(std::ifstream(path+".terrain-input.bin").good())check(S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c),"terrain evidence");
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
  S9xRemasterEndFrame(captured->originalRgb555.data(),captured->width,captured->width,captured->height,frame.get(),&c);
  endframeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  if(std::getenv("ALTTP_CACHE_TIMING"))std::cerr<<"endframe_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
  check(!frame->mainPixels.empty(),"EndFrame");
 };
 run();const auto epoch=s.runtimeDungeonHeightEpoch;run();
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
 std::ofstream inventory(out+".inventory");inventory<<dungeonMapInventory(s.runtimeDungeonBg1HeightMap)<<'\n'<<dungeonMapInventory(s.runtimeDungeonHeightMap)<<'\n';
 std::ofstream report(out+".json");report<<"{\"room\":"<<c.roomIndex<<",\"profile_pin\":\""<<hex.str()<<"\",\"matched_pixels\":"<<matched<<",\"layers\":[";
 for(int bg=0;bg<2;bg++) {if(bg)report<<',';const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  report<<"{\"bg\":"<<bg<<",\"conflicts\":"<<m.structuralConflicts<<",\"unknown_walls\":"<<m.unknownWallCells<<",\"missing\":["<<m.unknownPlaneCells[0]<<','<<m.unknownPlaneCells[1]<<"],\"geometry_conflicts\":[";
  for(size_t k=0;k<m.geometryConflicts.size();k++){if(k)report<<',';const auto &r=m.geometryConflicts[k];report<<'['<<r.node<<','<<r.other<<','<<r.expected<<','<<r.actual<<']';}report<<"]}";
 }report<<"]}\n";
 std::string after;check(S9xRemasterSerializeProfile(s.activeProfile,after,errors) && after==serialized,"own profile changed");
 std::cout<<"PASS room="<<c.roomIndex<<" profile="<<hex.str()<<" matched="<<matched<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
