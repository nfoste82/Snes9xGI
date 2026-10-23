// Read-only effective region samples from matching before/after review packets.
#include "../../runtime/remaster.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 RemasterFrame before,after;if(!S9xReadRemasterFrame(argv[1],before)||!S9xReadRemasterFrame(argv[2],after))return 1;
 auto height=[](const RemasterFrame &f,const RemasterFramePixel &p){const auto &i=f.tileInstances[p.instanceId-1];int h=i.heightOffset;
  if(i.hasPlacementHeight)return h+i.placementHeight[p.tilePixel];
  for(const auto &a:f.assetMetadata)if(a.tileId==i.tileId)return h+a.height[p.tilePixel];return h;};
 int changed=0,preserved=0;bool first=true;std::cout<<"{\"samples\":[";
 for(int bg=0;bg<2;bg++)for(size_t n=0;n<before.mainPixels.size();n++) {
  const auto &v=bg?before.mainPixels[n]:before.subPixels[n];const auto &w=bg?after.mainPixels[n]:after.subPixels[n];
  if(!v.instanceId||!w.instanceId||v.tilePixel>=64)continue;
  const auto &i=before.tileInstances[v.instanceId-1];if(i.source!=RemasterSourceType::Background)continue;
  const RemasterFrameAssetMetadata *m=nullptr;for(const auto &a:before.assetMetadata)if(a.tileId==i.tileId)m=&a;
  if(!m||m->tileRole!=RemasterTileRole::Door||!m->hasDoorframeMask)continue;
  bool marked=m->doorframeMask&(UINT64_C(1)<<v.tilePixel);int b=height(before,v),a=height(after,w);
  if(!marked && b!=a){std::cerr<<"opening changed\n";return 1;}
  if(b!=a)changed++;else preserved++;
  if((n%before.width)%8==0 || !marked) {
   if(!first)std::cout<<',';first=false;std::cout<<"{\"screen\":["<<n%before.width<<','<<n/before.width<<"],\"bg\":"<<int(i.sourceIndex)<<",\"canonical_pixel\":"<<int(v.tilePixel)<<",\"marked\":"<<marked<<",\"before\":"<<b<<",\"after\":"<<a<<'}';
  }
 }
 std::cout<<"],\"changed_frame_pixels\":"<<changed<<",\"preserved_door_pixels\":"<<preserved<<"}\n";
}
