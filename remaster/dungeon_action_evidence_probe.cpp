// Read-only own-packet diagnostics for composed walls and owned terrain actors.
#include "remaster.h"
#include "lighting_field_cache.h"
#include <iostream>
int main(int argc,char **argv) {
 if(argc<2)return 2;std::string p=argv[1];auto f=std::make_unique<RemasterFrame>();
 if(!S9xReadRemasterFrame(p,*f))return 2;
 RemasterLightingFieldCache fields;fields.update(*f,false);auto full=fields.height;fields.update(*f,false);if(full!=fields.height)return 3;
 if(argc==3){auto other=std::make_unique<RemasterFrame>();if(!S9xReadRemasterFrame(argv[2],*other))return 2;fields.update(*other,false);RemasterLightingFieldCache forced;forced.update(*other,false,true);if(fields.height!=forced.height)return 3;
  int changed=0;for(size_t n=0;n<full.size();n++)changed+=full[n]!=fields.height[n];std::cout<<"effective field differences="<<changed<<'\n';}
 RemasterDungeonFloorContext c;
 std::ifstream stateFile(p+".dungeon.json");std::string state{std::istreambuf_iterator<char>(stateFile),{}};
 auto number=[&](const char *key,int index=0){auto at=state.find(std::string("\"")+key+"\":");if(at==std::string::npos)return 0;at=state.find(':',at)+1;if(state[at]=='[')at++;for(int n=0;n<index;n++)at=state.find(',',at)+1;return std::stoi(state.substr(at));};
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
 c.groundedEvidence=state.find("\"grounded_evidence\":true")!=std::string::npos;
 S9xRemasterReadTerrainEvidence(p+".terrain-input.bin",c);
 std::cout<<"terrain="<<c.hasTerrainActors<<" module="<<int(c.module)<<" grounded="<<c.groundedEvidence<<" link="<<c.linkX<<','<<c.linkY<<" z="<<c.linkZ<<" scroll="<<c.actorScrollX<<','<<c.actorScrollY<<'\n';
 for(int k=0;k<16;k++){auto &a=c.terrainActors[k];if(a.state)std::cout<<"actor "<<k<<" xy="<<a.x<<','<<a.y<<" state="<<int(a.state)<<" kind="<<int(a.kind)<<" z="<<int(a.z)<<" plane="<<int(a.plane)<<'\n';}
 for(int k=0;k<128;k++){auto &o=c.terrainOam[k];if(o.name==0x46)std::cout<<"oam "<<k<<" xy="<<o.x<<','<<int(o.y)<<" flags="<<int(o.flags)<<" size="<<int(o.size)<<'\n';}
 for(size_t n=0;n<f->tileInstances.size();n++) {
  auto &i=f->tileInstances[n];if(i.source!=RemasterSourceType::Object)continue;
  if(i.tileNumber!=0x46 && i.tileNumber!=0x47 && i.tileNumber!=0x56 && i.tileNumber!=0x57)continue;
  int count=0;for(auto &px:f->mainPixels)count+=px.instanceId==n+1;if(!count)continue;
  const RemasterFrameAssetMetadata *m=nullptr;for(auto &a:f->assetMetadata)if(a.tileId==i.tileId)m=&a;
  std::cout<<"obj "<<n+1<<" slot="<<int(i.sourceIndex)<<" tile="<<i.tileNumber<<" pal="<<int(i.palette)<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" pixels="<<count<<" base="<<i.heightOffset<<" local="<<(m && m->hasHeight)<<'\n';
 }
 int linkMaximum=-32768,linkPixels=0;std::set<int> linkSlots;
 for(const auto &px:f->mainPixels)if(px.instanceId) {
  const auto &i=f->tileInstances[px.instanceId-1];if(i.source!=RemasterSourceType::Object || i.assetGroup!="link_sprite")continue;
  const RemasterFrameAssetMetadata *m=nullptr;for(auto &a:f->assetMetadata)if(a.tileId==i.tileId)m=&a;
  if(!m || !m->hasHeight)continue;
  linkMaximum=std::max(linkMaximum,int(i.heightOffset)+(i.hasPlacementHeight?i.placementHeight[px.tilePixel]:m->height[px.tilePixel]));linkPixels++;linkSlots.insert(i.sourceIndex);
 }
 std::cout<<"authored Link visible maximum="<<linkMaximum<<" pixels="<<linkPixels<<" slots=";for(int slot:linkSlots)std::cout<<slot<<',';std::cout<<'\n';
 for(int y=38;y<=47;y++)for(int x=2;x<=11;x++) {
  int sx=x*8-(c.backgroundScrollX&511),sy=y*8-(c.backgroundScrollY&511);if(sx<0 || sx>=int(f->width) || sy<0 || sy+7>=int(f->height))continue;
  auto &px=f->mainPixels[sy*f->width+sx];if(!px.instanceId)continue;auto &i=f->tileInstances[px.instanceId-1];
  const RemasterFrameAssetMetadata *m=nullptr;for(auto &a:f->assetMetadata)if(a.tileId==i.tileId)m=&a;
  std::cout<<"cell "<<x<<','<<y<<" bg="<<int(i.sourceIndex)<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" base="<<i.heightOffset<<" placement="<<i.hasPlacementHeight<<" local=";
  for(int yy=0;yy<8;yy++)std::cout<<int(i.hasPlacementHeight?i.placementHeight[yy*8]:(m?m->height[yy*8]:0))<<',';std::cout<<'\n';
 }
}
