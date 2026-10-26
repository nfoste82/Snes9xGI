// Detection-only complete-room source transport inventory after own-input replay.
#define main ownInputReplayMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(ownInputReplayMain(argc,argv))return 1;
 const auto &s=S9xRemasterState();
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  std::ofstream o(std::string(argv[2])+"-sources-bg"+std::to_string(bg+1)+".jsonl");
  for(int k=0;k<4096;k++) {
   const auto &t=tiles[k];int previews=0;for(const auto &p:m.localHeightPreviews)previews+=p.cell==k;
   o<<"{\"bg\":"<<bg+1<<",\"x\":"<<k%64<<",\"y\":"<<k/64<<",\"diagonal\":"<<t.diagonalHeightPreview
    <<",\"preserved\":"<<t.diagonalAuthoredPreserved<<",\"semantic\":"<<t.semanticSoutheastWall
    <<",\"intercept\":"<<t.diagonalIntercept<<",\"minimum\":"<<t.diagonalMinimum<<",\"maximum\":"<<t.diagonalMaximum
    <<",\"base\":"<<m.offsets[k]<<",\"obstacle\":"<<m.producerObstacle[k]<<",\"previews\":"<<previews<<"}\n";
  }
 }
 return 0;
}
