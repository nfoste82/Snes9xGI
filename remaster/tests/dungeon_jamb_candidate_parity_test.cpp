// Partial-column classifier, not a solver-only manufactured ownership witness.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 using C=RemasterDungeonFloorContext;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto src=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 std::array<RemasterTileContentId,32> ids{};
 for(int tile=0;tile<32;tile++) {
  uint8_t p[64];std::fill_n(p,64,1);p[0]=1+(tile&7);p[1]=1+(tile>>3);p[2]=2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*gfx)[tile*32+b/2*16+y*2+b%2]|=((p[y*8+x]>>b)&1)<<(7-x);
  ids[tile]={S9xRemasterHashTile(4,p),1,4};
 }
 C c;c.indoors=c.verifiedAlttpRom=true;c.graphics=gfx->data();c.constructedMaps=maps->data();c.dungeonTemplateData=src->data();
 c.bg1GraphicsBase=0;
 std::ofstream inventory;if(argc>1)inventory.open(argv[1],std::ios::binary);
 const int offsets[]={0x2716,0x29f0,0x2c6a,0x2e6a};int cases=0;
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int side:{0,3})for(int mode=0;mode<5;mode++) {
  maps->fill(0xfe);src->fill(0xff);c.placementBackgroundIndex=bg;
  const auto support=d==0?RemasterSupportDirection::South:d==1?RemasterSupportDirection::North:d==2?RemasterSupportDirection::East:RemasterSupportDirection::West;
  RemasterProfile profile;
  for(int tile:{5,6}) {
   auto &a=profile.assets[ids[tile]];a.tileId=ids[tile];a.hasHeight=a.hasDoorframeMask=true;
   a.tileRole=RemasterTileRole::Doorway;a.doorframeMask=UINT64_MAX;a.height.fill(-3);
   a.doorwaySupport=tile==5 && mode!=3;a.doorwaySupportDirection=support;
  }
  const int h=d<2?3:4,off=offsets[d];
  const int direction=d^((flip&(d<2?2:1))?1:0);
  const int dx=direction==2?-1:direction==3?1:0,dy=direction==0?-1:direction==1?1:0;
  const int foot=20*64+20,other=foot+dx+64*dy;
  for(int band=0;band<2;band++) {
   const int xx=d<2?side:(d==2?2-band:band),yy=d<2?(d==0?2-band:band):side;
   for(int equivalent=0;equivalent<(mode==4?2:1);equivalent++) {
    const int n=off+equivalent*24+2*(xx*h+yy);(*src)[n]=5+band;(*src)[n+1]=0;
   }
   const int word=(5+band)^((flip&1?0x4000:0)|(flip&2?0x8000:0))^(mode==2?0x2000:0);
   const int at=2*(band?other:foot);(*maps)[at]=word;(*maps)[at+1]=word>>8;
  }
  if(mode==1)(*maps)[2*other]^=1;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  for(int i:{foot,other}) {
   const auto &t=(*tiles)[i];need(!t.doorAssembly,"partial jamb forged full assembly");
   need(bool(t.doorwayColumnContact)==(mode!=1 && mode!=3),"jamb predicate/order changed");
   if(t.doorwayColumnContact) {
    need(t.doorwayColumnContact==foot+1 && t.doorwayColumnBand==(i==foot?0:1),"jamb owner/band changed");
    need(t.doorwayColumnSource==off,"identical canonical sources changed first source");
   }
  }
  if(inventory)for(const auto &t:*tiles)dungeonPlacementInventory(inventory,t);
  cases++;
 }
 std::cout<<"PASS "<<cases<<" partial jamb classifier cases; all axes/flips/sides/BGs, overwrite, priority, absent support, identical sources\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
