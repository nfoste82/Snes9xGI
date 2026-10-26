// Captured regression of a detector miss, NOT an acceptance test for geometry.
// Source arrays remain immutable and the runtime defect must remain visible.
#define main ownInputReplayMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {try {
 check(ownInputReplayMain(argc,argv)==0,"own capture replay");
 const auto &s=S9xRemasterState();const auto &m=s.runtimeDungeonHeightMap;
 size_t incomplete=0;
 for(int cell=0;cell<4096;cell++) {
  const auto &t=s.dungeonRetainedBg2->at(cell);
  const auto a=s.activeProfile.assets.find({t.tileHash,1,4});
  if(!t.diagonalHeightPreview || a==s.activeProfile.assets.end() || t.height==a->second.height)continue;
  bool transported=false;for(const auto &p:m.localHeightPreviews)transported|=p.cell==cell;
  if(t.diagonalAuthoredPreserved && !transported)incomplete++;
 }
 check(incomplete==54,"fixture must expose both high diagonal source transport omissions");
 auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[1],*f),"packet");
 size_t mismatches=0;std::set<std::pair<int,int>> cells;
 for(size_t at=0;at<f->mainPixels.size();at++) {
  const auto &px=f->mainPixels[at];if(!px.instanceId)continue;const auto &i=f->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1)continue;
  const int x=((768+at%f->width)&511)/8,y=((3240+at/f->width)&511)/8;
  const auto &t=s.dungeonRetainedBg2->at(y*64+x);if(!t.diagonalHeightPreview)continue;
  const auto *a=S9xRemasterFrameMetadataForTile(*f,i.tileId);check(a,"metadata");
  const auto g=S9xRemasterResolveFrameGeometry(*a,*f,px.instanceId-1);
  if(i.heightOffset+g.height[px.tilePixel]!=m.offsets[y*64+x]+t.height[px.tilePixel]) {
   check(!i.hasPlacementHeight && !f->contextGeometry.count(px.instanceId),"missing transport source");
   mismatches++;cells.insert({x,y});
  }
 }
 check(mismatches==1728 && cells.size()==27,"exact capture014 SE defect denominator");
 std::cout<<"PASS detection: 54 complete-room source omissions; 27 observed SE members / 1728 main pixels; no geometry repair\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
