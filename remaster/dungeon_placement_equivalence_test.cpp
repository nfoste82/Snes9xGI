// Differential placement/diagnostic inventory and classifier microbenchmark.
// Compile before/after a performance-only change; compare stdout byte-for-byte.
// Timing goes to stderr. Includes both BGs, flips, priority and overwritten cells.
#include "remaster.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
static std::vector<uint8_t> load(const char *path) {
 std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("missing input");
 return {std::istreambuf_iterator<char>(in),{}};
}
int main(int argc,char **argv) {try {
 if(argc!=5)throw std::runtime_error("usage: snapshot attributes profile ROM");
 auto raw=load(argv[1]),attr=load(argv[2]),rom=load(argv[4]);
 const size_t vo=14+224*(30+256*6);
 if(raw.size()!=vo+65536+512 || attr.size()!=8202 || rom.size()!=1048576 ||
    std::memcmp(raw.data(),"ALTPPD1\0",8) || std::memcmp(attr.data(),"ALTPAT1\0",8))throw std::runtime_error("invalid inputs");
 auto word=[&](int i){return raw[i]|raw[i+1]<<8;};
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 if(!S9xRemasterLoadProfile(argv[3],profile,errors))throw std::runtime_error(errors.front().message);
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=word(8);
 c.graphics=raw.data()+vo;c.collisionAttributes=attr.data()+10;c.dungeonTemplateData=rom.data()+0x1b52;
 c.graphicsBase=word(30)*2;c.bg1GraphicsBase=word(20)*2;
 for(int layer=0;layer<2;layer++)for(int y=0;y<64;y++)for(int x=0;x<64;x++) {
  const int address=(word(18+layer*10)+y/32*2048+x/32*1024+y%32*32+x%32)&32767;
  const int target=(layer?0:8192)+(y*64+x)*2;
  (*maps)[target]=c.graphics[address*2];(*maps)[target+1]=c.graphics[address*2+1];
 }
 const auto original=*maps;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 double ms=0;int calls=0;
 for(int variant=0;variant<6;variant++)for(int bg=0;bg<2;bg++) {
  *maps=original;
  for(int i=0;i<8192;i++) {
   if(variant>=1 && variant<=3)(*maps)[i*2+1]^=variant<<6; // H,V,HV
   if(variant==4)(*maps)[i*2+1]^=0x20; // priority only
   if(variant==5 && i%17==0){(*maps)[i*2]=0xff;(*maps)[i*2+1]=0xff;} // overwrites
  }
  auto context=c;context.placementBackgroundIndex=bg;
  if(!bg){std::rotate(maps->begin(),maps->begin()+8192,maps->end());std::swap(context.graphicsBase,context.bg1GraphicsBase);}
  context.constructedMaps=maps->data();
  const auto start=std::chrono::steady_clock::now();
  S9xRemasterDungeonPlacements(context,profile,*tiles);
  ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();calls++;
  std::cout<<variant<<' '<<bg<<'\n';
  for(const auto &t:*tiles) {
   // Every semantic field, never object padding or only a tile hash.
    std::cout<<int(t.surface)<<' '<<t.groundedFloorMask<<' '<<t.doorframeMask<<' '<<t.hasDoorframeMask<<' '
     <<t.doorTemplateOffset<<' '<<t.doorAssembly<<' '<<int(t.doorDirection)<<' '<<t.splitDoorPart<<' '<<t.hasHeight<<' '<<t.hflip<<' '<<t.vflip<<' '
    <<int(t.floorPlaneMask)<<' '<<t.distinctLowerUsage<<' '<<int(t.structuralFloorPlaneMask)<<' '
    <<t.wallTemplateOffset<<' '<<t.tileHash<<' '<<t.supportingTrim<<' '<<t.terminalCorner<<' '
    <<t.highPerimeterFace<<' '<<t.lowerDoorWallPart<<' '<<t.diagnosticWallTemplateOffset<<' '
    <<int(t.diagnosticUphillDirections)<<' '<<int(t.diagnosticNormalUphill)<<' '
	    <<t.diagnosticAuthoredFloor<<' '<<t.diagnosticRomFloorTrim<<' '<<t.diagonalHeightPreview<<' '
	    <<int(t.diagonalPreviewRow)<<' '<<int(t.diagonalPreviewFoot)<<' '<<t.diagonalIntercept<<' '
	    <<int(t.diagonalMinimum)<<' '<<int(t.diagonalMaximum)<<' '<<t.semanticSoutheastWall<<' '<<t.semanticBoundaryKnown<<' '<<int(t.removableSupportPlanes)
     <<' '<<t.obstacleAssembly<<' '<<t.obstacleTemplate<<' '<<int(t.obstacleAssemblyHeight)<<' '<<int(t.obstacleAssemblyFlip);
   for(auto h:t.height)std::cout<<' '<<int(h);std::cout<<'\n';
  }
 }
 std::cerr<<"room="<<c.roomIndex<<" placement_calls="<<calls<<" mean_ms="<<ms/calls<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
