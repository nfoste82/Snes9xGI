// Immutable packet diagnostics; does not apply a newer profile.
#include "remaster.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc!=2)return 2;const std::string p=argv[1];auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(p,*f))return 2;
 std::ifstream file(p+".dungeon.json");std::string text{std::istreambuf_iterator<char>(file),{}};
 auto number=[&](const char *key,int index=0){auto at=text.find(std::string("\"")+key+"\":");if(at==std::string::npos)return 0;at=text.find(':',at)+1;if(text[at]=='[')at++;for(int n=0;n<index;n++)at=text.find(',',at)+1;return std::stoi(text.substr(at));};
 const int sx=number("scroll"),sy=number("scroll",1);
 std::cout<<"schema="<<f->schemaVersion<<" scene="<<f->scenePixels.size()<<" lights="<<f->lights.size()<<" scale="<<f->lightingCoordinateScale<<'\n';
 for(auto &l:f->lights)std::cout<<"light="<<l.x<<','<<l.y<<','<<l.z<<" intensity="<<l.intensity<<'\n';
 for(auto *buffer:{&f->mainPixels,&f->subPixels,&f->scenePixels}) {
  size_t emitting=0;for(auto &px:*buffer)if(px.instanceId && px.tilePixel<64) {
   const auto &i=f->tileInstances[px.instanceId-1];auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);
   if(m && m->hasEmission && m->emissionRgba[px.tilePixel*4+3])emitting++;
  }
  std::cout<<"emitting_pixels="<<emitting<<'\n';
 }
 auto &pixels=f->scenePixels.empty()?f->mainPixels:f->scenePixels;
 for(int y=0;y<int(f->height);y++)for(int x=0;x<int(f->width);x++) {
  const int rx=(sx+x)&511,ry=(sy+y)&511;
  if(rx!=(number("link_xy")+8)%512 || ry%8!=0)continue;
  auto &px=pixels[y*f->width+x];if(!px.instanceId)continue;auto &i=f->tileInstances[px.instanceId-1];
  auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);
  std::cout<<"sample="<<rx<<','<<ry<<" bg="<<int(i.sourceIndex)<<" group="<<i.assetGroup<<" role="<<(m?int(m->tileRole):-1)<<" base="<<i.heightOffset<<" local="<<(i.hasPlacementHeight?int(i.placementHeight[px.tilePixel]):m&&m->hasHeight?int(m->height[px.tilePixel]):-1)<<'\n';
 }
}
