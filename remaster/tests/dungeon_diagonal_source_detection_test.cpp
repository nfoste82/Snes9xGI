// Captured regression of derived-local transport through production EndFrame.
// Historical packet/profile remain immutable; inspect the newly produced packet.
#define main ownInputReplayMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {try {
 check(ownInputReplayMain(argc,argv)==0,"own capture replay");
 const auto &s=S9xRemasterState();const auto &m=s.runtimeDungeonHeightMap;
  size_t incomplete=0,derived=0;
 for(int cell=0;cell<4096;cell++) {
  const auto &t=s.dungeonRetainedBg2->at(cell);
  const auto a=s.activeProfile.assets.find({t.tileHash,1,4});
  if(!t.diagonalHeightPreview || a==s.activeProfile.assets.end() || t.height==a->second.height)continue;
  bool transported=false;for(const auto &p:m.localHeightPreviews)transported|=p.cell==cell;
   derived++;if(t.diagonalAuthoredPreserved || !transported)incomplete++;
 }
  check(derived>=54 && incomplete==0,"all derived diagonal sources must be transported");
  auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*f),"repaired packet");
  size_t mismatches=0,observed=0;std::set<std::pair<int,int>> cells;
  // EndFrame applies physical geometry to scene ownership when present and
  // restores raw stock-main ownership for capture. Inspect the real consumer.
  const auto &pixels=f->scenePixels.empty()?f->mainPixels:f->scenePixels;
  for(size_t at=0;at<pixels.size();at++) {
   const auto &px=pixels[at];if(!px.instanceId)continue;const auto &i=f->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1)continue;
  const int x=((768+at%f->width)&511)/8,y=((3240+at/f->width)&511)/8;
   const auto &t=s.dungeonRetainedBg2->at(y*64+x);if(!t.diagonalHeightPreview)continue;
   observed++;cells.insert({x,y});
  const auto *a=S9xRemasterFrameMetadataForTile(*f,i.tileId);check(a,"metadata");
  const auto g=S9xRemasterResolveFrameGeometry(*a,*f,px.instanceId-1);
  if(i.heightOffset+g.height[px.tilePixel]!=m.offsets[y*64+x]+t.height[px.tilePixel]) {
    mismatches++;
  }
 }
  std::cout<<"diagonal observed="<<observed<<" cells="<<cells.size()<<" mismatches="<<mismatches<<'\n';
  check(mismatches==0 && observed>=1728 && cells.size()>=27,"capture014 selected diagonal geometry");
  std::cout<<"PASS derived diagonal transport: complete-room sources and observed SE selected pixels match; original capture/profile unchanged\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
