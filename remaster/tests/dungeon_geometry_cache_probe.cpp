// Complete-room production-cache differential corpus/performance probe.
// Artwork mutation is a synthetic palette-index animation, NOT gameplay evidence.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);check(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 check(argc==8,"usage: snapshot attr profile ROM x y plane");
 auto raw=read(argv[1]),attr=read(argv[2]),rom=read(argv[4]);
 const size_t vo=14+224*(30+256*6);
 check(raw.size()==vo+65536+512 && attr.size()==8202 && rom.size()==1048576,"complete input sizes");
 auto word=[&](int i){return raw[i]|raw[i+1]<<8;};
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();std::copy_n(raw.data()+vo,65536,gfx->begin());
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=word(8);c.romIdentity=1;
 c.linkX=std::stoi(argv[5]);c.linkY=std::stoi(argv[6]);c.linkPlane=std::stoi(argv[7]);
 c.graphicsBase=word(30)*2;c.bg1GraphicsBase=word(20)*2;c.graphics=gfx->data();
 c.collisionAttributes=attr.data()+10;c.dungeonTemplateData=rom.data()+0x1b52;c.constructedMaps=maps->data();
 for(int l=0;l<2;l++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
  int at=(word(18+l*10)+y/32*2048+x/32*1024+y%32*32+x%32)&32767;
  int target=(l?0:8192)+(y*64+x)*2;(*maps)[target]=(*gfx)[at*2];(*maps)[target+1]=(*gfx)[at*2+1];
 }
 S9xRemasterDungeonStairsFromAttributes(c);
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 check(S9xRemasterLoadProfile(argv[3],profile,errors),"profile");
 // Extend metadata canonically to synthetic animation identities, never assign
 // placement bases by hash. Preserve existing variant metadata if present.
 const auto original=*gfx;
 std::set<uint16_t> addresses;
 for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
  int tile=((*maps)[bg*8192+2*i]|(*maps)[bg*8192+2*i+1]<<8)&1023;
  addresses.insert(uint16_t((bg?c.bg1GraphicsBase:c.graphicsBase)+tile*32));
 }
 for(auto address:addresses) {
  uint8_t before[64]={},after[64]={};
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   int index=0;for(int b=0;b<4;b++)index|=((original[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;
   before[y*8+x]=index;after[y*8+x]=index?index%15+1:0;
  }
  RemasterTileContentId a={S9xRemasterHashTile(4,before),1,4},b={S9xRemasterHashTile(4,after),1,4};
  auto found=profile.assets.find(a);if(found!=profile.assets.end() && !profile.assets.count(b)){profile.assets[b]=found->second;profile.assets[b].tileId=b;}
  for(auto &g:profile.assetGroups)if(std::find(g.second.tileIds.begin(),g.second.tileIds.end(),a)!=g.second.tileIds.end() &&
   std::find(g.second.tileIds.begin(),g.second.tileIds.end(),b)==g.second.tileIds.end())g.second.tileIds.push_back(b);
 }
 S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);S9xRemasterInvalidateDungeonContinuity();
 auto &s=S9xRemasterState();uint16_t screen[64]={};auto frame=std::make_unique<RemasterFrame>();double ms=0;
 auto run=[&]{S9xRemasterBeginFrame(64,8,8,8);auto start=std::chrono::steady_clock::now();
  S9xRemasterEndFrame(screen,8,8,8,frame.get(),&c);
  ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
  run();auto epoch=s.runtimeDungeonHeightEpoch,reuses=s.dungeonGeometryReuseCount;
  size_t authoredRequired=0,authoredSolved=0,authoredUnmatched=0,authoredContacts=0,authoredWitnessed=0,authoredConflicts=0;
  for(const auto *m:{&s.runtimeDungeonHeightMap,&s.runtimeDungeonBg1HeightMap}) {
   authoredRequired+=m->authoredObstacleCells;authoredSolved+=m->solvedAuthoredObstacleCells;authoredUnmatched+=m->unmatchedAuthoredObstacleCells;
   for(const auto &r:m->authoredObstacleEvidence){authoredContacts+=r.contacts;authoredWitnessed+=r.witnessedContacts;authoredConflicts+=r.conflict;}
  }
  auto originalBg2=dungeonMapInventory(s.runtimeDungeonHeightMap),originalBg1=dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
  s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();
  check(originalBg2==dungeonMapInventory(s.runtimeDungeonHeightMap) && originalBg1==dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"original authored group cache/full evidence mismatch");
 for(auto address:addresses) {
  for(int k=0;k<32;k++)(*gfx)[uint16_t(address+k)]=0;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   int index=0;for(int b=0;b<4;b++)index|=((original[uint16_t(address+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;
   index=index?index%15+1:0;
   for(int b=0;b<4;b++)(*gfx)[uint16_t(address+b/2*16+y*2+b%2)]|=((index>>b)&1)<<(7-x);
  }
 }
 run();double reuseMs=ms;bool reused=s.dungeonGeometryReuseCount>reuses;
 auto changed=s.dungeonClassifiedChangedCells;
 auto bg2=dungeonMapInventory(s.runtimeDungeonHeightMap),bg1=dungeonMapInventory(s.runtimeDungeonBg1HeightMap);
 std::vector<uint8_t> packet;check(S9xSerializeRemasterFrame(*frame,packet),"packet");
 s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;run();double fullMs=ms;
 check(bg2==dungeonMapInventory(s.runtimeDungeonHeightMap) && bg1==dungeonMapInventory(s.runtimeDungeonBg1HeightMap),"complete map/diagnostic inventory mismatch");
 std::vector<uint8_t> full;check(S9xSerializeRemasterFrame(*frame,full) && packet==full,"full packet mismatch");
 check(s.runtimeDungeonHeightEpoch>epoch,"full solve not exercised");
  size_t unobservedSolidWalls=0,unobservedRomWalls=0;
  size_t obstacleCells=0,obstacleSolved=0,obstacleAssemblies=0,obstacleConflictingBases=0;
  for(int bg=0;bg<2;bg++) {
   const auto &ts=bg?s.dungeonRetainedBg2:s.dungeonRetainedBg1;const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
   if(!ts)continue;std::map<int,int> bases;
   for(int i=0;i<4096;i++)if((*ts)[i].obstacleAssembly) {
    obstacleCells++;obstacleSolved+=m.offsets[i]!=255;const int a=(*ts)[i].obstacleAssembly;
    if(!bases.count(a))bases[a]=m.offsets[i];else if(bases[a]!=m.offsets[i])obstacleConflictingBases++;
   }obstacleAssemblies+=bases.size();
  }
  for(int i=0;i<4096;i++)if(s.dungeonRetainedBg2 && s.runtimeDungeonHeightMap.offsets[i]==255 &&
   (*s.dungeonRetainedBg2)[i].surface==RemasterDungeonFloorContext::Surface::Wall) {
   const int unobserved=(c.linkPlane&1)^1;
   if(!c.stairCount && c.collisionAttributes[(c.linkPlane&1)*4096+i]==0 && c.collisionAttributes[unobserved*4096+i]!=0) {
    unobservedSolidWalls++;
    unobservedRomWalls+=(*s.dungeonRetainedBg2)[i].wallTemplateOffset!=0;
   }
  }
  std::cout<<"{\"room\":"<<c.roomIndex<<",\"obstacle_assembly_cells\":"<<obstacleCells<<",\"obstacle_assembly_solved\":"<<obstacleSolved
   <<",\"obstacle_assemblies\":"<<obstacleAssemblies<<",\"obstacle_part_base_conflicts\":"<<obstacleConflictingBases<<",\"unobserved_solid_wall_omissions\":"<<unobservedSolidWalls
   <<",\"original_authored_required\":"<<authoredRequired<<",\"original_authored_solved\":"<<authoredSolved
   <<",\"original_authored_unmatched\":"<<authoredUnmatched<<",\"original_authored_contacts\":"<<authoredContacts
   <<",\"original_authored_witnessed\":"<<authoredWitnessed<<",\"original_authored_conflicts\":"<<authoredConflicts
   <<",\"original_authored_cache_full_equal\":true"
   <<",\"unobserved_rom_wall_omissions\":"<<unobservedRomWalls<<",\"reused\":"<<(reused?"true":"false")<<",\"changed_classified_cells\":"<<changed
  <<",\"reuse_ms\":"<<reuseMs<<",\"full_ms\":"<<fullMs<<",\"complete_maps_diagnostics_packet_equal\":true,\"missing\":["
  <<s.runtimeDungeonHeightMap.unknownPlaneCells[0]<<','<<s.runtimeDungeonHeightMap.unknownPlaneCells[1]<<"],\"unknown_walls\":"
  <<s.runtimeDungeonHeightMap.unknownWallCells<<",\"conflicts\":"<<s.runtimeDungeonHeightMap.structuralConflicts<<"}\n";
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
