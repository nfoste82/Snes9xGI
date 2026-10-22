// Pinned production frame plus complete placement classification for height audits.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[1],*packet),"packet");
 std::ofstream image(std::string(argv[2])+".ppm",std::ios::binary);image<<"P6\n"<<packet->width<<' '<<packet->height<<"\n255\n";
 for(auto color:packet->originalRgb555){const char rgb[]={char((color&31)*255/31),char(((color>>5)&31)*255/31),char(((color>>10)&31)*255/31)};image.write(rgb,3);}
 const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;
 c.dungeonTemplateData=templates.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 for(int bg=0;bg<2;bg++) {
  auto maps=input;if(!bg)std::rotate(maps.begin()+8,maps.begin()+8200,maps.begin()+16392);
  c.constructedMaps=maps.data()+8;c.placementBackgroundIndex=bg;
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
  S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*tiles);
  const auto &m=bg?S9xRemasterState().runtimeDungeonHeightMap:S9xRemasterState().runtimeDungeonBg1HeightMap;
  std::cout<<"BG="<<bg<<" fallback="<<m.configuredStairFallbacks<<" relations="<<m.authoredSupportRelations.size()<<" walls="<<m.solvedWallCells<<" missing="<<m.unknownWallCells<<" conflicts="<<m.structuralConflicts<<'\n';
  for(const auto &v:m.geometryConflicts)std::cout<<"conflict "<<v.node<<' '<<v.other<<' '<<v.expected<<' '<<v.actual<<'\n';
  if(bg==1) {
   check(m.configuredStairFallbacks==0 && m.planeOffsets[0][38*64+31]==24 && m.planeOffsets[1][43*64+31]==0,"authored rise24 support");
   size_t pillarMembers=0,wallPillarMembers=0;
   for(int i=0;i<4096;i++)if((*tiles)[i].authoredObstacleAssembly==2202 || (*tiles)[i].authoredObstacleAssembly==2214) {
    check(m.offsets[i]==24 && !m.fallbackDirections[i],"stair-side pillar base");pillarMembers++;
   }check(pillarMembers==16,"complete stair-side pillar denominator");
   for(int i=0;i<4096;i++)if((*tiles)[i].authoredObstacleGroup &&
    S9xRemasterState().activeProfile.obstacleGroups[(*tiles)[i].authoredObstacleGroup-1].name=="Wall_Pillar") {
    check(m.offsets[i]==24 && (*tiles)[i].authoredObstacleAssembly,"oriented wall pillar support");
    check((*tiles)[i].authoredObstacleDirection==(i%64<32?3:2),"west/east support direction");wallPillarMembers++;
   }check(wallPillarMembers==64,"complete west/east pillar denominator");
   auto produced=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*produced),"production output");
   const auto metadata=read(path+".dungeon.json");const std::string text(metadata.begin(),metadata.end());
   auto at=text.find("\"scroll\":[")+10;const int sx=std::stoi(text.substr(at)),sy=std::stoi(text.substr(text.find(',',at)+1));
   size_t pixels=0,pillarPixels=0;
   for(auto buffer:{&produced->mainPixels,&produced->subPixels,&produced->scenePixels})for(size_t k=0;k<buffer->size();k++) {
    const auto &px=(*buffer)[k];if(!px.instanceId)continue;const auto &inst=produced->tileInstances.at(px.instanceId-1);
    if(inst.source!=RemasterSourceType::Background || inst.sourceIndex!=1)continue;
    const int cell=(((sy+k/produced->width)&511)/8)*64+(((sx+k%produced->width)&511)/8);
    if((*tiles)[cell].authoredObstacleAssembly==2202 || (*tiles)[cell].authoredObstacleAssembly==2214) {
     check(inst.heightOffset==24 && !inst.hasPlacementHeight,"actual pillar pixel base");pillarPixels++;
    }
    if(m.planeOffsets[0][cell]!=24 || (*tiles)[cell].surface!=RemasterDungeonFloorContext::Surface::Floor)continue;
    check(inst.heightOffset==24 && !inst.hasPlacementHeight,"actual upper floor pixel base");pixels++;
   }check(pixels>0,"upper floor pixel denominator");
   check(pillarPixels>0,"pillar pixel denominator");
   std::cout<<"PASS upper24/lower0 no configured fallback, pillar_members="<<pillarMembers<<" oriented_wall_pillar_members="<<wallPillarMembers<<" actual_pillar_pixel_usages="<<pillarPixels<<" actual_upper_floor_pixel_usages="<<pixels<<'\n';
  }
  std::ofstream out(std::string(argv[2])+"-bg"+std::to_string(bg)+".json");out<<'[';
  for(int i=0;i<4096;i++) {
   if(i)out<<',';const auto &t=(*tiles)[i];const int word=c.constructedMaps[2*i]|c.constructedMaps[2*i+1]<<8;
   out<<"{\"cell\":"<<i<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"word\":"<<word<<",\"surface\":"<<int(t.surface)
    <<",\"height\":"<<t.hasHeight<<",\"base\":"<<m.offsets[i]<<",\"planes\":["<<m.planeOffsets[0][i]<<','<<m.planeOffsets[1][i]
    <<"],\"attrs\":["<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<"],\"assembly\":"<<t.obstacleAssembly
    <<",\"authored\":"<<t.authoredObstacle<<",\"authored_assembly\":"<<t.authoredObstacleAssembly<<",\"floor_mask\":"<<int(t.floorPlaneMask)<<",\"distinct\":"<<t.distinctLowerUsage<<",\"floor_id\":"<<(m.roomDiagnostics?m.roomDiagnostics->cells[i].floorId:-999)<<",\"template\":"<<t.wallTemplateOffset<<'}';
  }out<<"]\n";
 }
}
