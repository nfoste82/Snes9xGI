#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 for(int bg:{0,1})for(auto role:{RemasterTileRole::StairTop,RemasterTileRole::StairBottom})for(int base:{24,50}) {
  auto f=std::make_unique<RemasterFrame>();f->width=f->height=8;f->mainPixels.resize(64);f->subPixels.resize(64);
  RemasterFrameTileInstance i;i.tileId={123,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=bg;
  f->tileInstances.push_back(i);RemasterFrameAssetMetadata md;md.tileId=i.tileId;md.tileRole=role;md.hasHeight=true;md.height.fill(18);f->assetMetadata.push_back(md);
  for(int k=0;k<64;k++){f->mainPixels[k].instanceId=f->subPixels[k].instanceId=1;f->mainPixels[k].tilePixel=f->subPixels[k].tilePixel=k;}
  RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
  auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=m->hasPlaneOffsets=true;m->offsets.fill(base);m->fallbackDirections.fill(1);
  S9xRemasterApplyDungeonHeightMap(*f,c,*m,nullptr,nullptr,bg);
  for(auto &t:f->tileInstances)assert(t.heightOffset==0 && !t.hasPlacementHeight);
  assert(f->assetMetadata[0].height==md.height);
  // Only a producer-matched, complete assembly may carry a nonzero datum.
  m->producerOwnedStair[0]=true;m->producerHashes[0]=i.tileId.hash;
  m->fallbackDirections[0]=0;
  for(int signedBase:{-17,13}) {
   for(auto &t:f->tileInstances)t.heightOffset=0;
   m->offsets[0]=signedBase;S9xRemasterApplyDungeonHeightMap(*f,c,*m,nullptr,nullptr,bg);
   const auto &placed=f->tileInstances[f->mainPixels[0].instanceId-1];
   assert(placed.heightOffset==signedBase && !placed.hasPlacementHeight && f->assetMetadata[0].height==md.height);
  }
  m->producerOwnedStair[0]=false;m->producerHashes[0]=0;m->offsets.fill(base);
  for(auto &t:f->tileInstances)t.heightOffset=0;
  // Ordinary obstacle relief is not an authored absolute stair ramp.
  f->assetMetadata[0].tileRole=RemasterTileRole::Obstacle;
  S9xRemasterApplyDungeonHeightMap(*f,c,*m,nullptr,nullptr,bg);
  assert(f->tileInstances.back().heightOffset==base && !f->tileInstances.back().hasPlacementHeight);
 }
 std::cout<<"PASS explicit authored stair top/bottom both BGs/base24/50: no doubled offset/no fallback override; obstacle base retained\n";
}
