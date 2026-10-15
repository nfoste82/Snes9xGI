// This source builds candidate and reference via the documented macro. Compare
// stdout byte-for-byte: includes all canonical instance and pixel results.
#include "remaster.h"
#include <iostream>
int main(){
 for(int bg:{0,1})for(int flip=0;flip<4;flip++) {
  auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=m->hasPlaneOffsets=true;m->offsets.fill(255);
  m->offsets[0]=-8;m->offsets[1]=24;m->producerHashes[0]=m->producerHashes[1]=123;m->producerFlips[0]=m->producerFlips[1]=flip;
  m->fallbackDirections[0]=1;m->mixedFloorBases[0][1]=30;m->structuralPlaneOffsets[0][1]=24;
  RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
  RemasterFrame f;f.width=16;f.height=8;f.mainPixels.resize(128);f.subPixels.resize(128);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.tileId={123,1,4};i.hFlip=flip&1;i.vFlip=flip&2;f.tileInstances={i};
  RemasterFrameAssetMetadata a;a.tileId=i.tileId;a.hasHeight=true;for(int k=0;k<64;k++)a.height[k]=k/8;f.assetMetadata={a};
  for(int k=0;k<128;k++){f.mainPixels[k].instanceId=f.subPixels[k].instanceId=1;f.mainPixels[k].tilePixel=f.subPixels[k].tilePixel=(k/16)*8+k%8;}
  S9xRemasterApplyDungeonHeightMap(f,c,*m,nullptr,nullptr,bg);
  for(auto &t:f.tileInstances){std::cout<<t.heightOffset<<','<<t.hasPlacementHeight<<':';for(auto h:t.placementHeight)std::cout<<h<<',';std::cout<<'\n';}
  for(auto *pixels:{&f.mainPixels,&f.subPixels})for(auto &px:*pixels)std::cout<<px.instanceId<<','<<int(px.tilePixel)<<';';
 }
}
