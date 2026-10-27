// Diagnostic producer fixture only: no production policy approximation.
#include "../runtime/frame.h"
#include <cstdlib>
#include <string>
int main(int argc,char **argv) {
 if(argc!=6)return 2;
 const std::string mode=argv[2];int order=std::atoi(argv[3]),hv=std::atoi(argv[4]),vertical=std::atoi(argv[5]);
 RemasterFrame f;f.width=vertical?8:24;f.height=vertical?24:8;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 for(int j=0;j<6;j++) {
  RemasterFrameTileInstance i;i.tileId={uint64_t(100+j),1,4};i.source=RemasterSourceType::Background;
  i.sourceIndex=j<3?order:j==3?1-order:2;i.hFlip=hv&1;i.vFlip=hv&2;i.heightOffset=j<2?90:0;
  if(j==5)i.source=RemasterSourceType::Object;
  RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.hasHeight=true;m.hasNormals=mode!="normals_missing";
  m.tileRole=j<2?RemasterTileRole::Wall:RemasterTileRole::Floor;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   int tx=i.hFlip?7-x:x,ty=i.vFlip?7-y:y;
   m.height[ty*8+tx]=j<2?15-j*8-(vertical?y:x):0;
  }
  if(j==3 && (mode=="fallback" || mode=="missing"))m.hasHeight=false;
  if(j==2 && mode=="missing")m.hasHeight=false;
  f.tileInstances.push_back(i);f.assetMetadata.push_back(m);
 }
 const size_t n=f.width*f.height;f.originalRgb555.resize(n);f.sceneRgb555.resize(n);
 f.mainPixels.resize(n);f.subPixels.resize(n);f.scenePixels.resize(n);
 if(mode=="hud" || mode=="hud_cover")for(int b=0;b<2;b++){f.observedBgPixels[b].resize(n);f.observedBgDepth[b].resize(n);}
 for(unsigned y=0;y<f.height;y++)for(unsigned x=0;x<f.width;x++) {
  auto at=y*f.width+x;unsigned axis=vertical?y:x,j=axis/8;
  unsigned k=((hv&2)?7-y%8:y%8)*8+((hv&1)?7-x%8:x%8);
  auto pixel=[&](unsigned id){const auto &i=f.tileInstances[id-1];return RemasterFramePixel{uint32_t(i.source)<<24|uint32_t(i.sourceIndex)<<16,id,uint8_t(k)};};
  auto p=pixel(j+1);f.scenePixels[at]=p;f.mainPixels[at]=p;
  bool cover=(mode=="floor_cover" && axis==16) || (mode=="floor_partial" && axis==16 && (vertical?x:y)==3) ||
   (mode=="wall_cover" && axis>=8 && axis<16) || (mode=="wall_partial" && axis==8 && (vertical?x:y)==3) ||
   ((mode=="fallback" || mode=="missing") && axis==16);
  if(cover)f.mainPixels[at]=pixel(4);
  if(mode=="obj" && axis==16)f.mainPixels[at]=pixel(6);
  if(mode=="same_scene") {
   auto clone=f.tileInstances[j];clone.heightOffset=123;f.tileInstances.push_back(clone);
   f.mainPixels[at].instanceId=f.tileInstances.size();
  }
  if(mode=="hud" || mode=="hud_cover") {
   f.mainPixels[at]=pixel(5);f.subPixels[at]=pixel(4);
   f.observedBgPixels[order][at]=p;f.observedBgDepth[order][at]=10;
   f.observedBgPixels[1-order][at]=pixel(4);f.observedBgDepth[1-order][at]=mode=="hud_cover"?20:5;
  }
 }
 return S9xWriteRemasterFrame(f,argv[1])?0:1;
}
