// Inspect actual visible source heights, including packets without room sidecars.
#include "../../runtime/remaster.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc!=2)return 2;RemasterFrame f;if(!S9xReadRemasterFrame(argv[1],f))return 1;
 std::cout<<"lights="<<f.lights.size()<<'\n';
 for(const auto &g:f.assetGroups)if(g.name.compare(0,18,"capture_animation_")==0) {
  bool torch=false;for(const auto &id:g.tileIds)torch|=id.hash==UINT64_C(0x713a84c159e10e36) || id.hash==UINT64_C(0x34eb3798eedfa0fc) || id.hash==UINT64_C(0x98b436e4f1eca7a9);
  if(torch){std::cout<<"observed_track="<<g.name<<" ids=";for(const auto &id:g.tileIds)std::cout<<std::hex<<id.hash<<',';std::cout<<std::dec<<'\n';}
 }
 std::map<RemasterTileContentId,const RemasterFrameAssetMetadata*> metadata;
 for(const auto &a:f.assetMetadata)metadata[a.tileId]=&a;
 struct Span{int count=0,lo=INT_MAX,hi=INT_MIN;std::set<int> bases;};
 std::map<uint64_t,Span> spans;
 for(const auto &px:f.mainPixels)if(px.instanceId && px.tilePixel<64) {
  const auto &i=f.tileInstances[px.instanceId-1];const auto it=metadata.find(i.tileId);
  if(it==metadata.end())continue;const auto &a=*it->second;const auto *e=a.emissionRgba.data()+px.tilePixel*4;
  if(!a.hasEmission || !e[3] || !(e[0] || e[1] || e[2]))continue;
  int local=i.hasPlacementHeight?i.placementHeight[px.tilePixel]:a.hasHeight?a.height[px.tilePixel]:0;
  auto &s=spans[i.tileId.hash];s.count++;s.lo=std::min(s.lo,i.heightOffset+local);s.hi=std::max(s.hi,i.heightOffset+local);s.bases.insert(i.heightOffset);
 }
 for(auto &entry:spans){const auto &s=entry.second;std::cout<<"hash="<<std::hex<<entry.first<<std::dec<<" pixels="<<s.count<<" effective="<<s.lo<<".."<<s.hi<<" bases=";for(auto b:s.bases)std::cout<<b<<',';std::cout<<'\n';}
}
