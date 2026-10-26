// Exact own-input EndFrame replay plus complete stair-member/base audit.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){try {
 check(captureFixtureMain(argc,argv)==0,"capture replay");auto &s=S9xRemasterState();
 check(s.runtimeDungeonBg1HeightMap.solvedWallCells==288 && !s.runtimeDungeonBg1HeightMap.unknownWallCells && !s.runtimeDungeonBg1HeightMap.structuralConflicts,"cap-region repair preserved");
 for(int bg=0;bg<2;bg++) {
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  for(int y=39;y<43;y++)for(int x=30;x<34;x++) {
   const int i=y*64+x;const auto &t=tiles[i];
   if(!bg)check(m.producerOwnedStair[i] && m.offsets[i]==0 && m.structuralPlaneOffsets[0][i]==0 && m.structuralPlaneOffsets[1][i]==0,"complete own071 staircase datum");
   else check(!m.producerOwnedStair[i],"overwritten BG2 staircase accepted");
  }
 }
 auto original=std::make_unique<RemasterFrame>(),replayed=std::make_unique<RemasterFrame>();
 check(S9xReadRemasterFrame(argv[1],*original) && S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*replayed),"comparison packets");
 for(uint64_t hash:{UINT64_C(0x99863ac4c1bb8e68),UINT64_C(0x76a7931735f4b282)}) {
  auto source=std::find_if(original->assetMetadata.begin(),original->assetMetadata.end(),[&](const auto &m){return m.tileId.hash==hash;});
  auto result=std::find_if(replayed->assetMetadata.begin(),replayed->assetMetadata.end(),[&](const auto &m){return m.tileId.hash==hash;});
  check(source!=original->assetMetadata.end() && result!=replayed->assetMetadata.end() && source->height==result->height,"captured authored stair arrays invariant");
 }
 std::cout<<"PASS own071 complete BG1 staircase16 members base0; BG2 overwrite rejected; authored arrays and288 solved cap-region walls preserved\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
