// Production own-input EndFrame sequence. Actor/submodule sequence is a
// handler-backed controlled replay, not a recorded gameplay trace.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
#include "../runtime/lighting_field_cache.h"
int main(int argc,char **argv) {try {
 check(argc==3,"usage: own-capture output-prefix");
 check(captureFixtureMain(argc,argv)==0,"immutable fixture replay");
 const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 const std::string text(meta.begin(),meta.end());
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"state key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
 c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");c.romIdentity=number("rom_identity");c.groundedEvidence=true;
 for(int l=0;l<2;l++){c.ppuMapBase[l]=number("ppu_map_base",l);c.ppuMapSize[l]=number("ppu_map_size",l);}
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
 check(S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c),"terrain");S9xRemasterDungeonStairsFromAttributes(c);
 auto captured=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*captured),"packet");
 auto result=std::make_unique<RemasterFrame>();auto &s=S9xRemasterState();
 auto run=[&]{
  S9xRemasterBeginFrame(captured->width*captured->height,captured->width,captured->width,captured->height);
  s.tileInstances=captured->tileInstances;for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
  for(size_t at=0;at<captured->mainPixels.size();at++) {
   s.mainOwners[at]=captured->mainPixels[at].owner;s.subOwners[at]=captured->subPixels[at].owner;
   s.mainInstanceIds[at]=captured->mainPixels[at].instanceId;s.subInstanceIds[at]=captured->subPixels[at].instanceId;
   s.mainTilePixels[at]=captured->mainPixels[at].tilePixel;s.subTilePixels[at]=captured->subPixels[at].tilePixel;
  }
  s.scenePixels=captured->scenePixels;s.sceneRgb=captured->sceneRgb555;
  S9xRemasterEndFrame(captured->originalRgb555.data(),captured->width,captured->width,captured->height,result.get(),&c);
 };
 run();const auto lights=result->lights;check(!lights.empty(),"actual offscreen fixture denominator");
 if(const char *trace=std::getenv("S9X_REMASTER_STAIR_LIGHT_TRACE"))
  check(!std::ifstream(trace).good(),"trace window spent before first stair (use fresh path)");
 const auto epoch=s.runtimeDungeonHeightEpoch;
 const auto inventory=dungeonMapInventory(s.runtimeDungeonHeightMap);
 RemasterLightingFieldCache fields;
 const auto start=std::chrono::steady_clock::now();int steps=0;
 for(int stair=0;stair<c.stairCount;stair++)for(int handler:{8,16})for(int step=0;step<12;step++) {
  c.submodule=handler;c.constructionPhase=step?1:0;
  c.linkX=c.stairs[stair].x*8+8;c.linkY=c.stairs[stair].y*8-24+step*4;
  c.linkPlane=c.stairs[stair].changesPlane?(step>=6):0;run();steps++;
#ifdef ALTTP_TEST_PRIOR_STAIR_PHASE_GATE
  if(step) {check(result->lights.empty(),"prior climb gate baseline");continue;}
#endif
  if(steps==1)if(const char *trace=std::getenv("S9X_REMASTER_STAIR_LIGHT_TRACE")) {
   std::ifstream file(trace);std::string first;std::getline(file,first);
   check(first.find("\"event\":\"stair_trace_start\",\"sample\":0")!=std::string::npos,"first stair did not arm explicit start");
  }
  check(result->lights.size()==lights.size(),"stair removed physical sources");
  for(size_t n=0;n<lights.size();n++) {
   const auto &a=lights[n],&b=result->lights[n];
   check(a.x==b.x && a.y==b.y && a.z==b.z && a.intensity==b.intensity,"stair altered solved emitter geometry");
  }
  if(!c.stairs[stair].changesPlane)check(s.runtimeDungeonHeightEpoch==epoch && dungeonMapInventory(s.runtimeDungeonHeightMap)==inventory,"same-plane unchanged stair inputs re-solved");
  check(!s.runtimeDungeonHeightMap.entrySupportConsumed && !s.transitionSupport.pending,"in-room stairs invented entry anchor");
  fields.update(*result,false);RemasterLightingFieldCache forced;forced.update(*result,false,true);
  check(fields.height==forced.height && fields.emission==forced.emission,"incremental fields disagree with full extraction");
 }
 const double sequenceMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 if(const char *tracePath=std::getenv("ALTTP_STAIR_REPLAY_TRACE")) {
  std::ifstream trace(tracePath);check(bool(trace),"actual chronology input");std::string line;int records=0,climb=0,dropped=0;
  while(std::getline(trace,line)) {
   if(records==600)break; // immutable first session; appended reruns preserved
   auto value=[&](const char *key,int index=0){auto pos=line.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"trace key");pos=line.find(':',pos)+1;if(line[pos]=='[')pos++;for(int k=0;k<index;k++)pos=line.find(',',pos)+1;return std::stoi(line.substr(pos));};
   check(value("room")==c.roomIndex && value("collision")==0 && value("module")==7,"trace scope");
   c.submodule=value("submodule");c.constructionPhase=value("construction");c.linkX=value("link");c.linkY=value("link",1);c.linkPlane=value("link",3);
   c.backgroundScrollX=value("scroll");c.backgroundScrollY=value("scroll",1);run();records++;
   if(c.submodule==8 && c.constructionPhase==1){climb++;dropped+=result->lights.empty();}
   else check(!result->lights.empty(),"recorded ordinary/init source missing");
  }
  check(records==600 && climb==249,"actual trace denominator changed");
#ifdef ALTTP_TEST_PRIOR_STAIR_PHASE_GATE
  check(dropped==249,"prior gate did not reproduce actual249blackout frames");
#else
  check(dropped==0,"actual climb lifecycle still drops sources");
#endif
  std::cout<<"actual_trace_records="<<records<<" climb_frames="<<climb<<" dropped="<<dropped<<" (recorded lifecycle/camera; own packet artwork fixed, not live VRAM chronology)\n";
 }
 c.submodule=2;run();check(result->lights.empty(),"room transition must suppress sources");
 c.submodule=8;c.constructionPhase=2;run();check(result->lights.empty(),"unsupported stair phase must suppress sources");
 std::cout<<"PASS handler sequence steps="<<steps<<", assemblies="<<int(c.stairCount)<<", physical sources="<<lights.size()<<", stable heights/maps/cache; no anchor/default support; sequence_plus_field_oracle_ms="<<sequenceMs<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
