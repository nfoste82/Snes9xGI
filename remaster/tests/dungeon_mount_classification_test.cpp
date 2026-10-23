#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 RemasterProfile p;std::array<RemasterTileContentId,8> ids;
 for(int n=0;n<8;n++){uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)(*graphics)[n*32+b/2*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  auto &a=p.assets[ids[n]];a.hasHeight=true;for(int k=0;k<64;k++)a.height[k]=7-k%8;
 }
 C c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.graphics=graphics->data();c.dungeonTemplateData=tpl->data();
 auto put=[&](int i,int word){(*maps)[i*2]=word;(*maps)[i*2+1]=word>>8;};
 for(int off:{0xa14,0xa44})for(int flip=0;flip<4;flip++) {
  maps->fill(0);tpl->fill(255);for(auto &v:p.assets)v.second.tileRole=RemasterTileRole::Unknown;p.assets[ids[0]].tileRole=RemasterTileRole::Wall;
  const bool north=off==0xa14;int w=north?4:2,h=north?2:4;
  for(int n=0;n<8;n++){(*tpl)[off+n*2]=n;(*tpl)[off+n*2+1]=0;}
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){int sx=flip&1?w-1-x:x,sy=flip&2?h-1-y:y,n=north?sy*w+sx:sx*h+sy;put((20+y)*64+20+x,n|0x2400|(flip<<14));}
  S9xRemasterDungeonPlacements(c,p,*tiles);need(tiles->at(20*64+20).wallTemplateOffset==off,"transformed full wall mount classification");
  put(20*64+20,1023);S9xRemasterDungeonPlacements(c,p,*tiles);need(!tiles->at(21*64+20).wallTemplateOffset,"partial overwrite promoted");
 }
 maps->fill(0);tpl->fill(255);for(auto &v:p.assets)v.second.tileRole=RemasterTileRole::Unknown;
 p.assets[ids[4]].tileRole=RemasterTileRole::Wall;
 for(int n=0;n<4;n++){(*tpl)[0x2e82+16+n*2]=n;(*tpl)[0x2e83+16+n*2]=0;p.assets[ids[n]].tileRole=RemasterTileRole::Door;put((20+n)*64+20,n|0x2000);}
 put(19*64+20,4);put(24*64+20,4);S9xRemasterDungeonPlacements(c,p,*tiles);
 need(tiles->at(21*64+20).surface==S::Wall && tiles->at(21*64+20).wallTemplateOffset==0x2e82,"complete authored outer door band remains wall geometry");
 put(24*64+20,1023);S9xRemasterDungeonPlacements(c,p,*tiles);need(tiles->at(21*64+20).surface==S::Door,"door missing wall end promoted");
 std::cout<<"PASS handler layouts, full transformed mounts, overwrite rejection, door wall-band ownership\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
