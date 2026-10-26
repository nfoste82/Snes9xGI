// Read-only terminal pixel inventory. Never reconstructs/replaces capture geometry.
#include "../../runtime/frame.h"
#include <iostream>
#include <fstream>
#include <memory>
int main(int argc,char **argv) {
 if(argc!=3){std::cerr<<"capture output-prefix\n";return 2;}
 auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(argv[1],*f)){std::cerr<<"invalid packet\n";return 2;}
 for(int which=0;which<3;which++) {
  const auto &pixels=which==0?f->mainPixels:which==1?f->subPixels:f->scenePixels;
  for(size_t at=0;at<pixels.size();at++) {
   const auto &p=pixels[at];if(!p.instanceId || p.instanceId>f->tileInstances.size() || p.tilePixel>=64)continue;
   const auto &i=f->tileInstances[p.instanceId-1];
   if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
   const auto *a=S9xRemasterFrameMetadataForTile(*f,i.tileId);
   const auto g=S9xRemasterResolveFrameGeometry(a?*a:RemasterFrameAssetMetadata{},*f,p.instanceId-1);
   std::cout<<"{\"buffer\":\""<<(which==0?"main":which==1?"sub":"scene")<<"\",\"sx\":"<<at%f->width<<",\"sy\":"<<at/f->width
    <<",\"bg\":"<<int(i.sourceIndex)+1<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"hf\":"<<i.hFlip<<",\"vf\":"<<i.vFlip
    <<",\"instance\":"<<p.instanceId<<",\"k\":"<<int(p.tilePixel)<<",\"base\":"<<i.heightOffset<<",\"placement\":"<<i.hasPlacementHeight
    <<",\"context\":"<<f->contextGeometry.count(p.instanceId)<<",\"status\":"<<int(i.geometryStatus)<<",\"has_height\":"<<g.hasHeight
    <<",\"local\":"<<g.height[p.tilePixel]<<",\"actual\":"<<i.heightOffset+g.height[p.tilePixel]
    <<",\"asset_local\":"<<(a?a->height[p.tilePixel]:0)<<"}\n";
  }
 }
 for(int scene=0;scene<2;scene++) {
  const auto &rgb=scene?f->sceneRgb555:f->originalRgb555;
  if(rgb.size()!=size_t(f->width)*f->height)continue;
  std::ofstream o(std::string(argv[2])+(scene?"-scene.ppm":"-original.ppm"),std::ios::binary);
  o<<"P6\n"<<f->width<<' '<<f->height<<"\n255\n";
  for(auto v:rgb)for(int shift:{0,5,10})o.put(char(((v>>shift)&31)*255/31));
 }
 return 0;
}
