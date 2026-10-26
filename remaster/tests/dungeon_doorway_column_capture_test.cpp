// Exact classifier failure-class ablations, including spatial H/V transforms.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){try {
 check(captureFixtureMain(argc,argv)==0,"exact own replay");
 auto replay=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*replay),"replay packet");
 auto &state=S9xRemasterState();size_t stairPixels=0,doorPixels=0;
 // Own092 camera pin; no coordinates or scene visibility affect the classifier.
 for(size_t at=0;at<replay->scenePixels.size();at++) {
  const auto &px=replay->scenePixels[at];if(!px.instanceId)continue;const auto &i=replay->tileInstances[px.instanceId-1];
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=0)continue;
  const int x=((720+at%replay->width)&511)/8,y=((2833+at/replay->width)&511)/8;
  if(x<30 || x>33)continue;
  const auto *m=S9xRemasterFrameMetadataForTile(*replay,i.tileId);check(m,"authored metadata");
  const auto g=S9xRemasterResolveFrameGeometry(*m,*replay,px.instanceId-1);
  if(y==40 || y==41) {
   check(!i.hasPlacementHeight && g.hasHeight && g.hasNormals && g.height==m->height && g.normalXyz==m->normalXyz && i.heightOffset==0,"stair effective local source bypassed");stairPixels++;
  }
  if((x==30 || x==33) && (y==56 || y==57)) {
   const int expected=y==57 && (m->doorframeMask&(UINT64_C(1)<<px.tilePixel))?8:0;
   check(!i.hasPlacementHeight && i.heightOffset==expected && i.vFlip && g.height==m->height && g.normalXyz==m->normalXyz,"jamb source/band/transform bypassed");doorPixels++;
  }
 }
 check(stairPixels==467 && doorPixels==256,"effective scene receiver denominators");
 for(int x:{30,33})for(int y:{56,57}) {
  const auto &t=(*state.dungeonRetainedBg1)[y*64+x];check(t.doorAssembly && !t.doorwayColumnContact,"complete lower four-word writer scope");
 }
 const std::string path=argv[1];auto bytes=read(path+".dungeon-input.bin"),tpl=read(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;check(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"own profile");
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.placementBackgroundIndex=0;c.graphicsBase=c.bg1GraphicsBase=16384;
 c.graphics=bytes.data()+24584;c.collisionAttributes=bytes.data()+16392;c.dungeonTemplateData=tpl.data()+8;c.constructedMaps=maps->data();
 for(int flip=0;flip<4;flip++)for(int mode=0;mode<4;mode++) {
  auto at=[&](int x,int y){return (flip&2?63-y:y)*64+(flip&1?63-x:x);};
  for(int bg=0;bg<2;bg++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
   const int from=8+(bg?0:8192)+2*(y*64+x),to=bg*8192+2*at(x,y);
   const int word=(bytes[from]|bytes[from+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
   (*maps)[to]=word;(*maps)[to+1]=word>>8;
  }
  // Ablate one signed opening word to isolate the independent partial-column
  // path. The intact capture now has a complete subtype2 lower-stair proof.
  (*maps)[2*at(31,56)]^=1;
  if(mode==1)(*maps)[2*at(30,57)]^=1; // actual content overwrite
  if(mode==2)(*maps)[2*at(30,57)+1]^=0x20; // priority-only harmless
  auto p=profile;
  if(mode==3)p.assets[{UINT64_C(0x9eb947628ab00ab3),1,4}].geometryByFlip=true;
  S9xRemasterDungeonPlacements(c,p,*tiles);
  for(int x:{30,33})for(int y:{56,57}) {
   const auto &t=(*tiles)[at(x,y)];const bool expected=mode!=3 && !(mode==1 && x==30);
   check(bool(t.doorwayColumnContact)==expected,"transformed/malformed/alternate column recognition");
   check(!t.doorAssembly,"partial column forged assembly");
  }
  for(int x=30;x<34;x++)check(!(*tiles)[at(x,58)].doorwayColumnContact,"overwritten outer band claimed");
 }
 std::cout<<"PASS exact092 transformed surviving columns, word overwrite, priority-only write and alternate-source rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
