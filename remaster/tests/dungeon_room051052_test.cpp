// Exact production EndFrame output verification; no geometry fabricated here.
#include "../runtime/lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 need(argc==3,"usage: pinned-input-capture production-output");
 auto input=std::make_unique<RemasterFrame>(),output=std::make_unique<RemasterFrame>();
 need(S9xReadRemasterFrame(argv[1],*input) && S9xReadRemasterFrame(argv[2],*output),"read");
 RemasterLightingFieldCache fields;fields.update(*output,true);
 size_t stairs=0,walls=0;std::map<uint64_t,size_t> stairHashes;
 for(size_t k=0;k<output->scenePixels.size();k++) {
  const auto &px=output->scenePixels[k];if(!px.instanceId)continue;
  const auto &i=output->tileInstances.at(px.instanceId-1);if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
  const auto *m=S9xRemasterFrameMetadataForTile(*output,i.tileId);need(m,"metadata");
  const auto e=S9xRemasterResolveFrameGeometry(*m,*output,px.instanceId-1);
  if(i.sourceIndex==0 && (i.tileId.hash==UINT64_C(0xc21e390677b3449f) || i.tileId.hash==UINT64_C(0xdd29a2e039ea282b))) {
   need(e.hasHeight && e.hasNormals && fields.height[k*2+1] && fields.authoredNormals[k],"authored stair actual scene fields");
   need(fields.height[k*2]==S9xRemasterSignedHeightField(i.heightOffset,e.height[px.tilePixel]),"stair composition");
   need(output->sceneRgb555[k]==i.paletteRgb555[px.tilePixel],"stair canonical color");stairs++;stairHashes[i.tileId.hash]++;
  }
  // The full-room solver has 48 recovered cells; only this viewport's subset
  // is visible. Compare every scene usage, not a hand-picked attractive pixel.
  const int x=((720+k%output->width)&511)/8,y=((2766+k/output->width)&511)/8;
  if(i.sourceIndex==1 && ((x>=16 && x<=18)||(x>=45 && x<=47)) &&
    ((y>=16 && y<=19)||(y>=22 && y<=25)||(y>=28 && y<=31)) && i.heightOffset==24) {
   need(e.hasHeight && !i.hasPlacementHeight && e.height==m->height,"wall immutable local relief");
   need(fields.height[k*2+1] && fields.height[k*2]==S9xRemasterSignedHeightField(24,e.height[px.tilePixel]),"actual wall raised field");walls++;
  }
 }
 need(stairs==256 && stairHashes.size()==2,"two stair scene denominators");need(walls>0,"raised wall viewport denominator");
 need(output->assetMetadata.size()==input->assetMetadata.size(),"metadata count");
 for(const auto &a:input->assetMetadata) {
  const auto *found=S9xRemasterFrameMetadataForTile(*output,a.tileId);need(found,"output asset identity");const auto &b=*found;
  need(a.tileId==b.tileId && a.height==b.height && a.hasHeight==b.hasHeight && a.normalXyz==b.normalXyz,"authored metadata changed");
 }
 std::cout<<"PASS production exact _052 stairs="<<stairs<<" raised_wall_pixels="<<walls<<" immutable authored arrays\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
