// Read-only packet/profile comparison. No viewport can prove room coverage.
#include "../../remaster/remaster.h"
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
 try {
  if(argc<2) throw std::runtime_error("usage: capture [profile ...]");
  auto f=std::make_unique<RemasterFrame>();
  if(!S9xReadRemasterFrame(argv[1],*f)) throw std::runtime_error("invalid packet");
  std::cout<<"schema="<<f->schemaVersion<<" size="<<f->width<<'x'<<f->height<<" rom="<<f->profileRomSha256<<'\n';
  for(int a=2;a<argc;a++) {
   RemasterProfile p; std::vector<RemasterProfileDiagnostic> errors;
   if(!S9xRemasterLoadProfile(argv[a],p,errors)) throw std::runtime_error("invalid profile");
   int heights=0,normals=0,missing=0,matched=0;
   for(auto &m:f->assetMetadata) {
    auto i=p.assets.find(m.tileId); if(i==p.assets.end()){missing++;continue;} matched++;
    heights+=m.hasHeight!=i->second.hasHeight || m.height!=i->second.height;
    normals+=m.hasNormals!=i->second.hasNormals || m.normalXyz!=i->second.normalXyz;
   }
   std::cout<<argv[a]<<" rise="<<int(p.upperFloorHeight)<<" matched="<<matched<<" absent="<<missing<<" height_mismatch="<<heights<<" normal_mismatch="<<normals<<'\n';
  }
  for(auto xy:{std::array<int,2>{100,90},{200,90},{100,140},{200,140},{100,35},{220,60},{180,120},{40,60}}) {
   auto &px=f->mainPixels[xy[1]*f->width+xy[0]]; if(!px.instanceId) continue;
   auto &i=f->tileInstances[px.instanceId-1];
   int local=0; for(auto &m:f->assetMetadata) if(m.tileId==i.tileId) local=m.height[px.tilePixel];
   std::cout<<"xy="<<xy[0]<<','<<xy[1]<<" id="<<px.instanceId<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" source="<<int(i.sourceIndex)<<" pixel="<<int(px.tilePixel)<<" offset="<<int(i.heightOffset)<<" local="<<local<<" placement="<<i.hasPlacementHeight<<" effective="<<int(i.heightOffset)+(i.hasPlacementHeight?i.placementHeight[px.tilePixel]:local)<<'\n';
  }
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
