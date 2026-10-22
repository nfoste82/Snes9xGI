// Reproduce complete pinned Room051 inputs and inventory both background layers.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;
 c.dungeonTemplateData=templates.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 for(int bg=0;bg<2;bg++) {
  auto maps=input;if(!bg)std::rotate(maps.begin()+8,maps.begin()+8200,maps.begin()+16392);
  c.constructedMaps=maps.data()+8;c.placementBackgroundIndex=bg;
  auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
  S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*tiles);
  const auto &m=bg?S9xRemasterState().runtimeDungeonHeightMap:S9xRemasterState().runtimeDungeonBg1HeightMap;
  for(const auto &e:m.authoredObstacleEvidence)std::cout<<"BG="<<bg<<" assembly="<<e.assembly<<" base="<<e.floorBase<<" witnessed="<<e.witnessedContacts<<'/'<<e.contacts<<" conflict="<<e.conflict<<'\n';
  if(bg==1) {
   size_t borders=0,thrones=0;
   const std::set<uint64_t> hashes={UINT64_C(0x16d59beb17fb4789),UINT64_C(0x960a306ed15fb74e),UINT64_C(0x254d835ec568d509),UINT64_C(0x592759243a477dd1)};
   for(int i=0;i<4096;i++) {
    if(hashes.count((*tiles)[i].tileHash)){check((*tiles)[i].surface==RemasterDungeonFloorContext::Surface::Obstacle && m.offsets[i]==50 && !m.fallbackDirections[i],"platform border support");borders++;}
    if((*tiles)[i].authoredObstacleGroup && S9xRemasterState().activeProfile.obstacleGroups[(*tiles)[i].authoredObstacleGroup-1].name=="Throne") {
     check(m.offsets[i]==50 && !m.fallbackDirections[i],"throne ground support");thrones++;
    }
   }check(borders==34 && thrones==32,"complete room obstacle denominator");
   auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"produced packet");
   const auto meta=read(path+".dungeon.json");const std::string text(meta.begin(),meta.end());
   auto pos=text.find("\"scroll\":[")+10;const int sx=std::stoi(text.substr(pos)),sy=std::stoi(text.substr(text.find(',',pos)+1));
   size_t pixels=0;
   for(const auto *buffer:{&packet->mainPixels,&packet->subPixels,&packet->scenePixels})for(size_t at=0;at<buffer->size();at++) {
    const auto &px=(*buffer)[at];if(!px.instanceId)continue;const auto &instance=packet->tileInstances.at(px.instanceId-1);
    if(instance.source!=RemasterSourceType::Background || instance.sourceIndex!=1)continue;
    const int cell=(((sy+at/packet->width)&511)/8)*64+(((sx+at%packet->width)&511)/8);
    const auto &t=(*tiles)[cell];
    if(!hashes.count(t.tileHash) && (!t.authoredObstacleGroup || S9xRemasterState().activeProfile.obstacleGroups[t.authoredObstacleGroup-1].name!="Throne"))continue;
    check(instance.heightOffset==50 && !instance.hasPlacementHeight,"actual pixel floor base/local relief composition");pixels++;
   }check(pixels>0,"no affected visible pixels");
   // A damaged platform perimeter cannot classify isolated reused art.
   auto damaged=maps;damaged[2*(17*64+24)+8]^=1;c.constructedMaps=damaged.data()+8;
   auto rejected=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
   S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*rejected);
   check((*rejected)[7*64+24].surface==RemasterDungeonFloorContext::Surface::Unknown,"partial perimeter accepted");
   c.constructedMaps=maps.data()+8;
   std::cout<<"PASS borders="<<borders<<" throne_members="<<thrones<<" visible_pixel_usages="<<pixels<<" base=50 partial_perimeter_rejected\n";
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
