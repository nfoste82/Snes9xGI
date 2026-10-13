#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 auto input=read(std::string(argv[1])+".dungeon-input.bin"),rom=read(std::string(argv[1])+".dungeon-templates.bin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=rom.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*tiles);
 const auto &m=S9xRemasterState().runtimeDungeonHeightMap;
 auto swapped=input;std::copy_n(input.data()+8200,8192,swapped.data()+8);std::copy_n(input.data()+8,8192,swapped.data()+8200);auto lower=c;lower.constructedMaps=swapped.data()+8;lower.placementBackgroundIndex=0;
 auto lt=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(lower,S9xRemasterState().activeProfile,*lt);
 for(int y:{54,55,56,58,59,60,61}) {auto &t=(*lt)[y*64+44];std::cout<<"lower cell44,"<<y<<" role="<<int(t.surface)<<" hash="<<std::hex<<t.tileHash<<std::dec<<" local="<<int(t.height[0])<<" height="<<t.hasHeight<<'\n';}
 for(int y=36;y<63;y++)for(int x=32;x<62;x++) {
  int cell=y*64+x;const auto &t=(*tiles)[cell];
  if(t.surface!=RemasterDungeonFloorContext::Surface::Obstacle && !t.authoredObstacle)continue;
  std::cout<<"cell="<<x<<','<<y<<" hash="<<std::hex<<t.tileHash<<std::dec<<" group="<<t.authoredObstacleGroup<<" assembly="<<t.authoredObstacleAssembly<<" contact="<<t.authoredObstacleContact<<" z="<<t.authoredObstacleZ<<" distinct="<<t.distinctLowerUsage<<" floor_mask="<<int(t.floorPlaneMask)<<" base="<<m.offsets[cell]<<" planes="<<m.planeOffsets[0][cell]<<','<<m.planeOffsets[1][cell]<<" attrs="<<int(c.collisionAttributes[cell])<<','<<int(c.collisionAttributes[4096+cell])<<" local="<<int(*std::min_element(t.height.begin(),t.height.end()))<<".."<<int(*std::max_element(t.height.begin(),t.height.end()))<<'\n';
 }
 for(const auto &e:m.authoredObstacleEvidence)std::cout<<"assembly="<<e.assembly<<" group="<<e.group<<" plane="<<e.plane<<" component="<<e.component<<" base="<<e.floorBase<<" members="<<e.members<<" contacts="<<e.contacts<<" witnessed="<<e.witnessedContacts<<" conflict="<<e.conflict<<'\n';
 auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"produced packet");
 size_t usages=0,raised=0,overrides=0;std::set<int> seen;
 for(const auto *pixels:{&packet->mainPixels,&packet->subPixels,&packet->scenePixels})for(size_t at=0;at<pixels->size();at++) {
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=packet->tileInstances[px.instanceId-1];
  const int x=((256+at%packet->width)&511)/8,y=((3345+at/packet->width)&511)/8,cell=y*64+x;
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || !(*tiles)[cell].authoredObstacle)continue;
  usages++;raised+=i.heightOffset==24;overrides+=i.hasPlacementHeight;seen.insert(cell);
  auto asset=S9xRemasterState().activeProfile.assets.find(i.tileId);check(asset!=S9xRemasterState().activeProfile.assets.end(),"own local source");
  for(const auto &a:packet->assetMetadata)if(a.tileId==i.tileId)check(a.hasHeight && a.height==asset->second.height,"actual packet authored pixels changed");
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_CONTACT_PLANES
  check(i.heightOffset==24 && !i.hasPlacementHeight,"actual own058 obstacle effective base/relief");
#endif
 }
 check(usages>1000 && seen.size()==24,"own058 actual pixel denominator");
 std::ofstream effective(std::string(argv[2])+"-effective.json");effective<<"{\"pixel_usages\":"<<usages<<",\"raised_pixel_usages\":"<<raised<<",\"local_overrides\":"<<overrides<<",\"visible_cells\":"<<seen.size()<<"}\n";
 std::cout<<"actual obstacle usages="<<usages<<" raised="<<raised<<" overrides="<<overrides<<" cells="<<seen.size()<<'\n';
}
