// Complete own-input production replay and placement-level reported-source audit.
#define main productionReplayMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(productionReplayMain(argc,argv))return 1;
 const auto &s=S9xRemasterState();
 std::ofstream out(std::string(argv[2])+"-placements.tsv");
 out<<"bg\tx\ty\thash\trole\tlocalMin\tlocalMax\tbase\tp0\tp1\twallTemplate\tstack\tdoorAssembly\tdoorTemplate\tframe\n";
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?s.dungeonRetainedBg2:s.dungeonRetainedBg1;
  const auto &map=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  std::cout<<"BG "<<bg<<" walls "<<map.solvedWallCells<<" unknown "<<map.unknownWallCells<<" conflicts "<<map.structuralConflicts<<'\n';
  for(const auto &v:map.geometryConflicts)std::cout<<"edge "<<v.node<<' '<<v.other<<' '<<v.expected<<' '<<v.actual<<'\n';
  for(int i=0;i<4096;i++) {const auto &t=tiles->at(i);
   out<<bg<<'\t'<<i%64<<'\t'<<i/64<<'\t'<<std::hex<<t.tileHash<<std::dec<<'\t'<<int(t.surface)<<'\t';
   out<<(t.hasHeight?*std::min_element(t.height.begin(),t.height.end()):-999)<<'\t'<<(t.hasHeight?*std::max_element(t.height.begin(),t.height.end()):-999)
    <<'\t'<<map.offsets[i]<<'\t'<<map.planeOffsets[0][i]<<'\t'<<map.planeOffsets[1][i]<<'\t'<<t.wallTemplateOffset<<'\t'<<t.wallStackAssembly
    <<'\t'<<t.doorAssembly<<'\t'<<t.doorTemplateOffset<<'\t'<<t.doorframeMask<<'\n';
  }
 }
}
