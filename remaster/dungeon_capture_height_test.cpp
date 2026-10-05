// Portable exact-capture regression. Run with capture, pinned candidate, full
// room061 ALTPPD1 and ALTPAT1. This is a bug regression, NOT room approval.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
#include <pthread.h>
static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static std::vector<uint8_t> read(const char *path) {
 std::ifstream in(path,std::ios::binary); require(bool(in),"missing fixture");
 return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
static void exercise(int argc,char **argv) {
   require(argc==5 || argc==6 || argc==7,"usage: capture profile snapshot attributes [mixed-only [verified-ROM]]");
  const bool exact=argc==5;
 auto frame=std::make_unique<RemasterFrame>(); require(S9xReadRemasterFrame(argv[1],*frame),"capture decode");
 auto profile=std::make_unique<RemasterProfile>(); std::vector<RemasterProfileDiagnostic> errors;
 require(S9xRemasterLoadProfile(argv[2],*profile,errors),"profile decode");
 auto raw=read(argv[3]), attr=read(argv[4]); const size_t vram=14+224*(30+256*6);
 require(raw.size()==vram+65536+512 && !std::memcmp(raw.data(),"ALTPPD1\0",8),"complete snapshot");
 require(attr.size()==8202 && !std::memcmp(attr.data(),"ALTPAT1\0",8),"complete attributes");
 auto word=[&](int i){return raw[i]|raw[i+1]<<8;};
 require(word(8)==0x61 && (attr[8]|attr[9]<<8)==0x61,"room pin");
 RemasterDungeonFloorContext c; c.verifiedAlttpRom=c.indoors=true; c.roomIndex=0x61;
 c.linkX=248; c.linkY=448; c.graphics=raw.data()+vram; c.graphicsBase=word(30)*2;
  c.collisionAttributes=attr.data()+10;
  std::vector<uint8_t> rom;
  if(argc==7){rom=read(argv[6]);require(rom.size()==1048576,"unheadered ROM");c.dungeonTemplateData=rom.data()+0x1b52;}
 std::vector<uint8_t> maps(16384);
 for(int layer=0;layer<2;layer++) for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
  int a=(word(18+layer*10)+(y/32)*2048+(x/32)*1024+(y%32)*32+x%32)&32767;
  int t=(layer?0:8192)+(y*64+x)*2; maps[t]=c.graphics[a*2]; maps[t+1]=c.graphics[a*2+1];
 }
 c.constructedMaps=maps.data(); auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,*profile,*tiles); c.placements=tiles->data();
 // The room geometry relates these floors without needing stair fallback.
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 require(S9xRemasterBuildDungeonHeightMap(c,profile->upperFloorHeight,*map),"room solve");
 auto height=[&](int x,int y,bool sub=false) {
  auto &px=(sub?frame->subPixels:frame->mainPixels)[y*frame->width+x];
  auto &i=frame->tileInstances[px.instanceId-1];
  for(auto &m:frame->assetMetadata) if(m.tileId==i.tileId)
   return int(i.heightOffset)+(i.hasPlacementHeight?i.placementHeight[px.tilePixel]:m.height[px.tilePixel]);
  return int(i.heightOffset);
 };
  if(exact) {
  require(frame->mainPixels[100*frame->width+181].instanceId==2103 && frame->subPixels[100*frame->width+181].instanceId==407,"exact occurrence 1");
 require(frame->mainPixels[96*frame->width+224].instanceId==1453 && frame->subPixels[96*frame->width+224].instanceId==413,"exact occurrence 2");
 require(height(181,100)==8 && height(224,96)==4 && height(176,93)==23,"captured before south heights");
  require(height(72,69)==34 && height(72,70)==9 && height(64,69)==14 && height(64,70)==37,"captured diagonal stripe");
  }
 for(auto &i:frame->tileInstances) { i.hasPlacementHeight=false; i.heightOffset=0; }
 // Captured tilePixel=0 begins at screen y=29, room y=32: actual sampling
 // origin is 3 (PPU.VOffset=2 plus the renderer's one-line bias).
 c.backgroundScrollY=3;
 // Check every visible target against complete room placed identity. This
 // rejects a different capture camera/state instead of accepting crop coverage.
 for(unsigned y=0;y<frame->height;y++) for(unsigned x=0;x<frame->width;x++) {
  auto &px=frame->mainPixels[y*frame->width+x]; if(!px.instanceId) continue;
  auto &i=frame->tileInstances[px.instanceId-1];
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1) continue;
  const int cell=((y+3)&511)/8*64+x/8;
  if(i.tileId.hash==UINT64_C(0xf8dd3cd336d542e0) || i.tileId.hash==UINT64_C(0x3bc2b767d36c52c3) ||
     i.tileId.hash==UINT64_C(0x31aba23efb31666b) || i.tileId.hash==UINT64_C(0x503ab068dfa941d3)) {
   // Authored profiles are independently checked against the pinned candidate.
   for(auto &m:frame->assetMetadata) if(m.tileId==i.tileId) require(profile->assets.at(i.tileId).height==m.height,"loaded profile mismatch");
   require(tiles->at(cell).height==profile->assets.at(i.tileId).height,"placed identity mismatch");
  }
 }
  S9xRemasterApplyDungeonHeightMap(*frame,c,*map);
  int floorPixels=0, belowBefore=0, belowAfter=0, changed=0, low=255, high=0, beforeLow=255, beforeHigh=0, retainedSlope=0;
  for(unsigned y=0;y<frame->height;y++) for(unsigned x=0;x<frame->width;x++) {
   auto &px=frame->mainPixels[y*frame->width+x]; if(!px.instanceId) continue;
   auto &i=frame->tileInstances[px.instanceId-1];
   if(i.tileId.hash!=UINT64_C(0x31aba23efb31666b) || i.sourceIndex!=1) continue;
   const int cell=((y+3)&511)/8*64+x/8;
   const auto floor=S9xRemasterMixedFloorBase(*map,cell);
   if(floor==255) continue;
   const int before=map->offsets[cell]+profile->assets.at(i.tileId).height[px.tilePixel], after=height(x,y);
   require(after==std::max(before,int(floor)),"mixed floor lower bound");
   if(after!=before) changed++;
   if(before>floor) { require(after==before,"slope above floor changed"); retainedSlope++; }
   if((tiles->at(cell).groundedFloorMask>>px.tilePixel)&1) {
    floorPixels++; belowBefore+=before<floor; belowAfter+=after<floor;
    low=std::min(low,after); high=std::max(high,after);
    beforeLow=std::min(beforeLow,before); beforeHigh=std::max(beforeHigh,before);
   }
  }
  std::cout<<"mixed target floor pixels="<<floorPixels<<" below="<<belowBefore<<" -> "<<belowAfter<<" changed="<<changed<<" floor range="<<beforeLow<<".."<<beforeHigh<<" -> "<<low<<".."<<high<<" retained slope pixels="<<retainedSlope<<" unknown="<<map->mixedFloorUnknown<<" conflicts="<<map->mixedFloorConflicts<<'\n';
  require(floorPixels>0 && belowBefore>0 && belowAfter==0,"target must reproduce and fix floor hole");
  if(exact) {
 require(height(181,100)==8 && height(224,96)==12 && height(176,93)==15,"corrected south heights");
 require(height(181,100,true)==0 && height(224,96,true)==4,"BG1 must remain independent");
 require(height(72,69)==30 && height(72,70)==29 && height(64,69)==38 && height(64,70)==37,"continuous diagonal rows");
 require(height(56,69)==45,"clipped upper face attached");
  std::cout<<"PASS exact 2103/407,1453/413; south 8/4/23 -> 8/12/15; diagonal 34/9/14/37 -> 30/29/38/37\n";
  }
}
struct Run { int argc; char **argv; std::string error; };
static void *worker(void *arg) {
 auto &run=*static_cast<Run *>(arg);
 try { exercise(run.argc,run.argv); } catch(const std::exception &e) {run.error=e.what();}
 return nullptr;
}
int main(int argc,char **argv) {
 Run run{argc,argv,{}}; pthread_attr_t attr;
 if(pthread_attr_init(&attr) || pthread_attr_setstacksize(&attr,512*1024)) return 1;
 pthread_t thread; const int created=pthread_create(&thread,&attr,worker,&run);
 pthread_attr_destroy(&attr);
 if(created || pthread_join(thread,nullptr)) return 1;
 if(!run.error.empty()) {std::cerr<<run.error<<'\n'; return 1;}
 std::cout<<"PASS 512 KiB capture solve/application stack\n";
}
