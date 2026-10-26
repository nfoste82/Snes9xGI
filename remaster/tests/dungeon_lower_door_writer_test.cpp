// Exact lower-stair writer proof: split terminal band, transforms, overwrites.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){try {
 check(argc==3,"usage: own098 prefix");check(captureFixtureMain(argc,argv)==0,"exact replay");
 const std::string path=argv[1];auto bytes=read(path+".dungeon-input.bin"),tpl=read(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;check(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"own profile");
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();auto maps=std::make_unique<std::array<uint8_t,16384>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.graphicsBase=c.bg1GraphicsBase=16384;
 c.graphics=bytes.data()+24584;c.collisionAttributes=bytes.data()+16392;c.dungeonTemplateData=tpl.data()+8;c.constructedMaps=maps->data();
 for(int flip=0;flip<4;flip++)for(int mode=0;mode<4;mode++)for(int bg=0;bg<2;bg++) {
  auto at=[&](int x,int y){return (flip&2?63-y:y)*64+(flip&1?63-x:x);};c.placementBackgroundIndex=bg;
  for(int layer=0;layer<2;layer++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
   const int from=8+(layer?0:8192)+2*(y*64+x),to=(layer==bg?0:8192)+2*at(x,y);
   const int word=(bytes[from]|bytes[from+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));(*maps)[to]=word;(*maps)[to+1]=word>>8;
  }
  if(mode==1)(*maps)[(bg?0:8192)+2*at(31,58)]^=1; // other/map shared terminal write lost
  if(mode==2)(*maps)[(bg?8192:0)+2*at(30,56)+1]^=0x20; // harmless priority
  if(mode==3)(*maps)[(bg?8192:0)+2*at(31,56)]^=1; // signed opening identity matters
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  for(int x=30;x<34;x++)check(bool((*tiles)[at(x,58)].doorAssembly)==(mode==0 || mode==2),"split full writer proof/overwrite");
  if(!bg && (mode==0 || mode==2))for(int x:{30,33})for(int y:{56,57})check((*tiles)[at(x,y)].doorAssembly && !(*tiles)[at(x,y)].doorwayColumnContact,"whole writer outranks partial columns");
 }
 std::cout<<"PASS lower26..29 exact split writer, H/V, priority and all-word overwrite rejection\n";
 return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
