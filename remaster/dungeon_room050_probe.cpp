// Complete OWN evidence classification inventory; no profile substitution.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;
 c.dungeonTemplateData=templates.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*tiles);
 const auto &m=S9xRemasterState().runtimeDungeonHeightMap;
 auto bg1Tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto bg1Maps=input;std::rotate(bg1Maps.begin()+8,bg1Maps.begin()+8200,bg1Maps.begin()+16392);
 auto lower=c;lower.constructedMaps=bg1Maps.data()+8;lower.placementBackgroundIndex=0;
 S9xRemasterDungeonPlacements(lower,S9xRemasterState().activeProfile,*bg1Tiles);
 for(int i=0;i<4096;i++)if((*bg1Tiles)[i].obstacleAssembly || (*bg1Tiles)[i].tileHash==UINT64_C(0x8044813840be1f26)) {
  const auto &t=(*bg1Tiles)[i];const auto &bm=S9xRemasterState().runtimeDungeonBg1HeightMap;
  std::cout<<"BG1 obstacle cell="<<i%64<<','<<i/64<<" assembly="<<t.obstacleAssembly<<" local="<<int(*std::min_element(t.height.begin(),t.height.end()))<<".."<<int(*std::max_element(t.height.begin(),t.height.end()))<<" base="<<bm.offsets[i]<<" planes="<<bm.planeOffsets[0][i]<<','<<bm.planeOffsets[1][i]<<'\n';
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_PART_SUPPORT
  check(bm.offsets[i]==0 && !bm.fallbackDirections[i] && bm.producerObstacle[i],"actual statue full assembly support/local composition");
#endif
 }
 {auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"actual produced packet");size_t topPixels=0;
  for(const auto *pixels:{&packet->mainPixels,&packet->subPixels})for(const auto &px:*pixels)if(px.instanceId) {
   const auto &i=packet->tileInstances[px.instanceId-1];if(i.source==RemasterSourceType::Background && i.sourceIndex==0 && i.tileId.hash==UINT64_C(0x8044813840be1f26)) {
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_PART_SUPPORT
    check(i.heightOffset==0 && !i.hasPlacementHeight,"own statue effective top offset/local array wrong");
#endif
    topPixels++;
   }
  }std::cout<<"actual_top_visible_usages="<<topPixels<<'\n';
 }
 check(m.roomIndex==0x50 && m.requiredPlaneCells[0]==0 && m.requiredPlaneCells[1]==3564 &&
  !m.structuralConflicts,"own room050 support/conflict scope changed");
 std::cout<<"actual walls="<<m.solvedWallCells<<" unknown="<<m.unknownWallCells<<" shared="<<m.sharedStructuralFaces<<'\n';
 check(m.solvedWallCells==419 && m.unknownWallCells==489 && m.sharedStructuralFaces==16,"own shared-shell failure class regression");
 std::ofstream out(std::string(argv[2])+"-placements.json");out<<'[';
 for(int i=0;i<4096;i++) {if(i)out<<',';const auto &t=(*tiles)[i];
  if(t.tileHash==UINT64_C(0x8044813840be1f26))std::cout<<"statue target cell="<<i%64<<','<<i/64<<" surface="<<int(t.surface)<<" base="<<m.offsets[i]<<'\n';
  out<<"{\"cell\":"<<i<<",\"surface\":"<<int(t.surface)<<",\"height\":"<<t.hasHeight<<",\"template\":"<<t.wallTemplateOffset
   <<",\"attrs\":["<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<"],\"base\":"<<m.offsets[i]
   <<",\"hash\":"<<t.tileHash<<",\"distinct\":"<<t.distinctLowerUsage<<",\"word\":"<<(c.constructedMaps[2*i]|c.constructedMaps[2*i+1]<<8)
   <<",\"structural_floor_mask\":"<<int(t.structuralFloorPlaneMask)<<",\"floor_mask\":"<<int(t.floorPlaneMask)
   <<",\"hflip\":"<<t.hflip<<",\"vflip\":"<<t.vflip<<",\"samples\":[";
   for(int k=0;k<64;k++){if(k)out<<',';out<<int(t.height[k]);}out<<"]"
   <<",\"span\":["<<int(*std::min_element(t.height.begin(),t.height.end()))<<','<<int(*std::max_element(t.height.begin(),t.height.end()))<<"]}";
 }out<<"]\n";
 // Counterfactual observed-plane scope, identical immutable complete inputs.
 // NOT historical Link/plane evidence and not a production anchor.
 c.roomIndex=0x50;c.linkX=402;c.linkY=2693;c.linkPlane=0;c.placements=tiles->data();
 auto other=std::make_unique<RemasterDungeonHeightMap>();
 check(S9xRemasterBuildDungeonHeightMap(c,50,*other),"independent plane audit");
 std::cout<<"counterfactual_plane0 walls="<<other->solvedWallCells<<" unknown="<<other->unknownWallCells<<" conflicts="<<other->structuralConflicts<<" support="<<other->requiredPlaneCells[0]<<'\n';
 std::ofstream audit(std::string(argv[2])+"-plane0.json");audit<<"{\"walls\":[";
 bool first=true;for(int i=0;i<4096;i++)if((*tiles)[i].surface==RemasterDungeonFloorContext::Surface::Wall){if(!first)audit<<',';first=false;audit<<'['<<i<<','<<m.offsets[i]<<','<<other->offsets[i]<<']';}
 audit<<"],\"anchors\":[";first=true;for(auto a:other->relativeAnchors){if(!first)audit<<',';first=false;audit<<'['<<a.node<<','<<a.minimum<<','<<a.supportNodes<<','<<a.isolatedFlat<<']';}audit<<"]}\n";
 std::ofstream witness(std::string(argv[2])+"-shared-witnesses.json");witness<<'[';first=true;
 for(int i=0;i<4096;i++)if((*tiles)[i].surface==RemasterDungeonFloorContext::Surface::Wall &&
  m.structuralPlaneOffsets[0][i]!=255 && m.structuralPlaneOffsets[1][i]!=255) {
  if(!first)witness<<',';first=false;witness<<'['<<i<<','<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]<<','<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<','<<(*tiles)[i].wallTemplateOffset<<']';
 }witness<<"]\n";
}
