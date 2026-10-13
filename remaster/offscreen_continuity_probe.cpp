// Pinned production room solve, fixed-world camera-edge transport packets.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){if(captureProbeMain(argc,argv))return 1;try {
 const std::string path=argv[1],out=argv[2];auto input=read(path+".dungeon-input.bin"),meta=read(path+".dungeon.json");
 std::string text(meta.begin(),meta.end());auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"state");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoi(text.substr(pos));};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.linkPlane=number("plane");c.module=number("module");c.submodule=number("submodule");c.constructionPhase=number("construction_phase");c.collisionMode=number("collision_mode");c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;
 auto &s=S9xRemasterState();RemasterFrame original;check(S9xReadRemasterFrame(path,original),"frame");
 RemasterFrame full=original;full.profileRomSha256=s.activeProfile.romSha256;full.lights.clear();full.hiddenEmitters.clear();
 for(auto &px:full.mainPixels)px.owner=0x02020000; // source-only oracle: HUD covers every emitter
 c.backgroundScrollX=256;c.backgroundScrollY=0;
 const auto d=S9xRemasterAppendOffscreenRoomLights(full,c,s.activeProfile,&s.runtimeDungeonHeightMap,&s.runtimeDungeonBg1HeightMap,S9xRemasterHashTile);
 check(d.unknownBases==0 && full.lights.empty(),"complete source oracle");const auto sources=full.hiddenEmitters;
 std::ofstream report(out+"-sweep.txt");
 // Keep receiver/world geometry fixed. Reproject source coordinates back into
 // reference world for transport; only discovery viewport membership changes.
 for(int scroll=144;scroll>=48;scroll--) {
  c.backgroundScrollY=scroll;RemasterFrame f=original;f.lights.clear();f.hiddenEmitters.clear();
  for(auto &px:f.mainPixels)px.owner=0;
  auto diag=S9xRemasterAppendOffscreenRoomLights(f,c,s.activeProfile,&s.runtimeDungeonHeightMap,&s.runtimeDungeonBg1HeightMap,S9xRemasterHashTile);
  for(auto &l:f.lights)l.y+=scroll;
  for(auto &e:f.hiddenEmitters)e.y+=scroll;
  for(auto *buffer:{&f.mainPixels,&f.subPixels})for(size_t k=0;k<buffer->size();k++) {
   const auto &a=(*buffer)[k],&b=buffer==&f.mainPixels?original.mainPixels[k]:original.subPixels[k];
   check(a.instanceId==b.instanceId && a.tilePixel==b.tilePixel,"source recovery changed visible geometry fields");
  }
  check(f.originalRgb555==original.originalRgb555 && f.sceneRgb555==original.sceneRgb555,"stock/scene colors changed");
  // Pixel transport of visible members; independent fixed-world source oracle.
  for(const auto &e:sources)if(e.y>=scroll && e.y<scroll+int(f.height))f.hiddenEmitters.push_back(e);
#ifndef ALTTP_TEST_LEGACY_OFFSCREEN_SPHERES
  check(f.lights.empty() && f.hiddenEmitters.size()==sources.size(),"viewport-dependent source loss/duplication");
  auto canonical=[](const auto &e){return std::make_tuple(e.x,e.y,e.z,e.depth,e.normal,e.emission,e.omnidirectional);};
  std::vector<decltype(canonical(sources.front()))> expected,actual;
  for(auto &e:sources)expected.push_back(canonical(e));for(auto &e:f.hiddenEmitters)actual.push_back(canonical(e));
  std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());check(expected==actual,"source geometry changes at edge");
#endif
  double power=0;for(auto &e:f.hiddenEmitters)power+=e.emission[3];
  for(auto &l:f.lights)power+=l.intensity*4*3.14159265358979323846*l.radius*l.radius;
  if(scroll==144 || scroll==67 || scroll==66 || scroll==58)std::cout<<"edge_area_power scroll="<<scroll<<" calibrated_alpha_sum="<<power<<" spheres="<<f.lights.size()<<" pixels="<<f.hiddenEmitters.size()<<'\n';
  // Plain numeric evidence for GPU harness, without any capture format mutation.
  report<<"camera "<<scroll<<' '<<f.lights.size()<<' '<<f.hiddenEmitters.size()<<' '<<diag.unknownBases<<'\n';
  for(auto l:f.lights)report<<"sphere "<<l.x<<' '<<l.y<<' '<<l.z<<' '<<l.radius<<' '<<l.intensity<<' '<<l.red<<' '<<l.green<<' '<<l.blue<<'\n';
  for(auto e:f.hiddenEmitters)report<<"pixel "<<e.x<<' '<<e.y<<' '<<e.z<<' '<<e.depth<<' '<<e.normal[0]<<' '<<e.normal[1]<<' '<<e.normal[2]<<' '<<int(e.emission[0])<<' '<<int(e.emission[1])<<' '<<int(e.emission[2])<<' '<<int(e.emission[3])<<' '<<e.omnidirectional<<'\n';
 }
 std::cout<<"fixed_phase_source_pixels="<<sources.size()<<" camera_sweep=144..48 unknown_bases=0\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
