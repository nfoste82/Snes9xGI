// Placement ownership overrides a reusable asset Door role, not its metadata.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try {
 for(int bg:{0,1})for(int flip=0;flip<4;flip++)for(int base:{-8,16,40})for(bool owned:{false,true})for(bool stale:{false,true}) {
  auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=7;map->hasRoomGeometry=true;map->offsets.fill(255);map->offsets[0]=base;
  map->producerHashes[0]=123;map->producerFlips[0]=flip;map->producerOwnedWall[0]=owned;
  // A stale region attachment cannot turn the full owned ramp back into a
  // per-hash door cutout. Ordinary unowned Door usages retain region behavior.
  map->doorframeAttachments.push_back({0,0,base,UINT64_C(1),123,true});
  RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
  RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.tileId={stale?124u:123u,1,4};i.hFlip=flip&1;i.vFlip=flip&2;f.tileInstances={i};
  RemasterFrameAssetMetadata a;a.tileId=i.tileId;a.tileRole=RemasterTileRole::Door;a.hasHeight=true;for(int k=0;k<64;k++)a.height[k]=7-k%8;f.assetMetadata={a};
  for(int k=0;k<64;k++){f.mainPixels[k].instanceId=f.subPixels[k].instanceId=1;f.mainPixels[k].tilePixel=f.subPixels[k].tilePixel=k;}
  S9xRemasterApplyDungeonHeightMap(f,c,*map,nullptr,nullptr,bg);
  for(auto *pixels:{&f.mainPixels,&f.subPixels})for(auto &px:*pixels) {
   const auto &applied=f.tileInstances[px.instanceId-1];
   need(applied.heightOffset==(stale?0:owned || px.tilePixel==0?base:0),"ownership/region/stale projection");
   need(!applied.hasPlacementHeight,"authored ramp replaced");
  }
  need(f.assetMetadata[0].height==a.height && f.assetMetadata[0].tileRole==RemasterTileRole::Door,"authored metadata changed");
 }
 std::cout<<"PASS owned ROM wall vs Door asset: both BGs/buffers, all flips/signed bases, unowned region and stale producer rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
