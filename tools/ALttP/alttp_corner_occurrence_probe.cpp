// Packet observations only: no camera/state inference from an older capture.
#include "../../remaster/frame.h"
#include <iostream>
int main(int argc,char **argv) {
 for(int n=1;n<argc;n++) {
  auto f=std::make_unique<RemasterFrame>();if(!S9xReadRemasterFrame(argv[n],*f))return 1;
  std::cout<<argv[n]<<" schema="<<f->schemaVersion<<'\n';
  for(size_t k=0;k<f->tileInstances.size();k++) {
   const auto &i=f->tileInstances[k];if(i.tileId.hash!=UINT64_C(0xee78ea413516533b))continue;
   const auto *a=S9xRemasterFrameMetadataForTile(*f,i.tileId);
   std::cout<<"id="<<k+1<<" BG="<<int(i.sourceIndex)+1<<" flips="<<i.hFlip<<i.vFlip<<" base="<<i.heightOffset<<" override="<<i.hasPlacementHeight;
   if(a && a->hasHeight){std::cout<<" authored=";for(auto h:a->height)std::cout<<int(h)<<',';}
   for(auto *p:{&f->mainPixels,&f->subPixels,&f->scenePixels}) {
    int x0=INT_MAX,y0=INT_MAX,x1=-1,y1=-1,count=0;
    for(size_t at=0;at<p->size();at++)if((*p)[at].instanceId==k+1){int x=at%f->width,y=at/f->width;x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);count++;}
    std::cout<<" buffer="<<(p==&f->mainPixels?0:p==&f->subPixels?1:2)<<" count="<<count<<" bounds="<<x0<<','<<y0<<','<<x1<<','<<y1;
   }
   std::cout<<'\n';
  }
 }
}
