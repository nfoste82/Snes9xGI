// Own-input production replay plus source-only audit. No profile substitution.
#define main dungeonCaptureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(dungeonCaptureMain(argc,argv))return 1;
 try {
  const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),meta=read(path+".dungeon.json");
  const std::string text(meta.begin(),meta.end());
  auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"missing state key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
  RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");
  if(text.find("\"module\":")!=std::string::npos)c.module=number("module");
  if(text.find("\"construction_phase\":")!=std::string::npos)c.constructionPhase=number("construction_phase");
  c.submodule=number("submodule");c.collisionMode=number("collision_mode");c.linkPlane=number("plane");
  c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
  c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;
  auto &s=S9xRemasterState();RemasterFrame frame;check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",frame),"replayed frame");
  RemasterFrame original;check(S9xReadRemasterFrame(path,original),"original packet");size_t emissionRecords=0;
  for(const auto &a:original.assetMetadata) {
   auto found=std::find_if(frame.assetMetadata.begin(),frame.assetMetadata.end(),[&](const auto &b){return b.tileId==a.tileId;});
   check(found!=frame.assetMetadata.end(),"metadata identity disappeared");
   check(a.hasEmission==found->hasEmission && a.emissionRgba==found->emissionRgba && a.emissionDepth==found->emissionDepth &&
    a.emissionOmnidirectional==found->emissionOmnidirectional,"authored emission changed");emissionRecords++;
  }
  std::cout<<"immutable_emission_records="<<emissionRecords<<'\n';
  std::cout<<"published spheres="<<frame.lights.size()<<" hidden="<<frame.hiddenEmitters.size()<<'\n';
  frame.lights.clear();frame.hiddenEmitters.clear();
  const auto d=S9xRemasterAppendOffscreenRoomLights(frame,c,s.activeProfile,&s.runtimeDungeonHeightMap,&s.runtimeDungeonBg1HeightMap,S9xRemasterHashTile);
  std::cout<<"sources placements="<<d.placements<<" unknown="<<d.unknownBases<<" spheres="<<d.lights<<" hidden="<<d.hiddenPixels<<" visible="<<d.visiblePixels<<'\n';
  unsigned emitting=0,unknown=0,supportOnly=0,structuralOnly=0;
  for(int bg=0;bg<2;bg++)for(int cell=0;cell<4096;cell++) {
   const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
   auto a=s.activeProfile.assets.find({m.producerHashes[cell],1,4});if(a==s.activeProfile.assets.end() || !a->second.hasEmission)continue;
   bool emits=false;for(int i=0;i<64;i++){const auto *e=a->second.emissionRgba.data()+i*4;emits|=e[3] && (e[0] || e[1] || e[2]);}if(!emits)continue;
   const int at=(bg?0:8192)+cell*2,word=c.constructedMaps[at]|c.constructedMaps[at+1]<<8;
   if(!bg && c.bg1GraphicsBase==c.graphicsBase && word==(c.constructedMaps[cell*2]|c.constructedMaps[cell*2+1]<<8))continue;
   emitting++;if(m.offsets[cell]==255){unknown++;supportOnly+=m.planeOffsets[0][cell]!=255 || m.planeOffsets[1][cell]!=255;structuralOnly+=m.structuralPlaneOffsets[0][cell]!=255 || m.structuralPlaneOffsets[1][cell]!=255;}
   if(!std::getenv("ALTTP_EMITTER_PLACEMENTS"))continue;
   std::cout<<"placement bg="<<bg<<" xy="<<cell%64<<','<<cell/64<<" base="<<m.offsets[cell]<<" support="<<m.planeOffsets[0][cell]<<','<<m.planeOffsets[1][cell]<<" structural="<<m.structuralPlaneOffsets[0][cell]<<','<<m.structuralPlaneOffsets[1][cell];
   if(m.roomDiagnostics)for(int p=0;p<2;p++){const auto &v=m.roomDiagnostics->cells[p*4096+cell];std::cout<<" p"<<p<<"=(floor="<<v.floorBase<<",id="<<v.floorId<<",wall="<<v.wall<<",conflict="<<v.heightConflict<<")";}
   std::cout<<'\n';
  }
  std::cout<<"effective emitting="<<emitting<<" unknown="<<unknown<<" support_only="<<supportOnly<<" structural_only="<<structuralOnly<<'\n';
  for(int scroll:{0,128,256,384}) {
   auto shifted=c;shifted.backgroundScrollY=scroll;RemasterFrame review=frame;review.lights.clear();review.hiddenEmitters.clear();
   const auto v=S9xRemasterAppendOffscreenRoomLights(review,shifted,s.activeProfile,&s.runtimeDungeonHeightMap,&s.runtimeDungeonBg1HeightMap,S9xRemasterHashTile);
   std::cout<<"camera_source_only scroll_y="<<scroll<<" spheres="<<v.lights<<" unknown="<<v.unknownBases<<" visible="<<v.visiblePixels<<'\n';
  }
  if(std::getenv("ALTTP_EMITTER_NEIGHBORS")) {
   auto templates=read(path+".dungeon-templates.bin");c.dungeonTemplateData=templates.data()+8;
   auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
   S9xRemasterDungeonPlacements(c,s.activeProfile,*tiles);
    const bool north=std::getenv("ALTTP_EMITTER_NORTH_STACK")!=nullptr;
    for(int y=north?4:15;y<=(north?13:18);y++)for(int x=north?47:58;x<=(north?51:61);x++) {
    const int i=y*64+x;const auto &t=(*tiles)[i];const auto &m=s.runtimeDungeonHeightMap;
    const int word=c.constructedMaps[i*2]|c.constructedMaps[i*2+1]<<8;
    std::cout<<"neighbor "<<x<<','<<y<<" word="<<std::hex<<word<<" hash="<<t.tileHash<<" template="<<t.wallTemplateOffset<<" diagnostic="<<t.diagnosticWallTemplateOffset<<std::dec
     <<" surface="<<int(t.surface)<<" attrs="<<int(c.collisionAttributes[i])<<','<<int(c.collisionAttributes[4096+i])<<" base="<<m.offsets[i]<<" height=";
    for(int k=0;k<64;k++)std::cout<<int(t.height[k])<<',';
    std::cout<<" matches=";for(int off=0;off<12876;off+=2)if(((c.dungeonTemplateData[off]|c.dungeonTemplateData[off+1]<<8)&0xc3ff)==(word&0xc3ff))std::cout<<std::hex<<off<<',';std::cout<<std::dec<<'\n';
   }
   for(int off:{0xa34,0xa44}){std::cout<<"torch "<<std::hex<<off<<" words=";for(int k=0;k<8;k++)std::cout<<(c.dungeonTemplateData[off+k*2]|c.dungeonTemplateData[off+k*2+1]<<8)<<',';std::cout<<std::dec<<'\n';}
  }
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
