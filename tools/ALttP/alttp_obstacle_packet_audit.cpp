// Read-only canonical-pixel audit. Never substitutes current profile metadata.
#include "../../remaster/remaster.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *m){if(!b)throw std::runtime_error(m);}
int main(int argc,char **argv){try{
 check(argc==2,"usage: packet");auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[1],*f),"packet parse");
 std::map<RemasterTileContentId,const RemasterFrameAssetMetadata*> metadata;
 for(const auto &m:f->assetMetadata)metadata.emplace(m.tileId,&m);
 std::map<uint32_t,std::array<size_t,3>> references;
 const std::vector<RemasterFramePixel>* buffers[]={&f->mainPixels,&f->subPixels,&f->scenePixels};
 for(int b=0;b<3;b++)for(const auto &p:*buffers[b])if(p.instanceId)references[p.instanceId][b]++;
 std::cout<<"{\"schema\":"<<f->schemaVersion<<",\"scene_pixels\":"<<f->scenePixels.size()<<",\"instances\":[";bool first=true;
 for(size_t n=0;n<f->tileInstances.size();n++){
  const auto &i=f->tileInstances[n];auto found=metadata.find(i.tileId);if(found==metadata.end())continue;const auto &m=*found->second;
  const bool reported=i.tileId.hash==UINT64_C(0xeff6f17a9f4420d9)||i.tileId.hash==UINT64_C(0x2fd3eada43bdfcd4)||i.tileId.hash==UINT64_C(0x19088620e4b960cc);
  if(!reported && m.tileRole!=RemasterTileRole::Obstacle && i.assetGroup!="obstacles")continue;
  if(!first)std::cout<<',';first=false;auto r=references[uint32_t(n+1)];
  std::cout<<"{\"id\":"<<n+1<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"role\":"<<int(m.tileRole)<<",\"group\":\""<<i.assetGroup<<"\",\"bg\":"<<int(i.sourceIndex)<<",\"base\":"<<i.heightOffset<<",\"override\":"<<i.hasPlacementHeight<<",\"references\":["<<r[0]<<','<<r[1]<<','<<r[2]<<"],\"flips\":["<<i.hFlip<<','<<i.vFlip<<"],\"normal_correction\":"<<S9xRemasterContinuedDiagonalNormalMask(i,&m)<<",\"pixels\":[";
  for(int k=0;k<64;k++){if(k)std::cout<<',';const int local=i.hasPlacementHeight?i.placementHeight[k]:(m.hasHeight?m.height[k]:0);
   std::cout<<"["<<int(m.height[k])<<','<<local<<','<<i.heightOffset+local<<','<<int(m.normalXyz[k*3])<<','<<int(m.normalXyz[k*3+1])<<','<<int(m.normalXyz[k*3+2])<<']';}
  std::cout<<"]}";
 }
 std::cout<<"]}\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
