// Independent expected geometry plus complete differential placement inventory.
// Compile against a frozen original closure using -include original/remaster.h.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 using C=RemasterDungeonFloorContext;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto src=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 std::array<RemasterTileContentId,64> ids{};
 for(int tile=0;tile<64;tile++) {
  uint8_t pixels[64];std::fill_n(pixels,64,1);pixels[0]=1+(tile&7);pixels[1]=1+(tile>>3);pixels[2]=2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*gfx)[tile*32+b/2*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[tile]={S9xRemasterHashTile(4,pixels),1,4};
 }
 // The alternate base holds identical source bytes; split is nevertheless not
 // routed when bases differ. This is not a hash-based permission to split.
 std::copy_n(gfx->begin(),2048,gfx->begin()+32768);
 C c;c.indoors=c.verifiedAlttpRom=true;c.graphics=gfx->data();c.constructedMaps=maps->data();c.dungeonTemplateData=src->data();
 std::ofstream inventory;if(argc>1)inventory.open(argv[1],std::ios::binary);
 const int offsets[]={0x2716,0x29f0,0x2c6a,0x2e6a};int cases=0;
 // 0 collision-only, 1 explicit/missing attrs, 2 unknown/missing attrs,
 // 3 explicit/no threshold, 4 early mismatch, 5 late mismatch,
 // 6 priority-only writes, 7 overwritten opening, 8 split/base disagreement.
 for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int split=0;split<2;split++)for(int mode=0;mode<9;mode++) {
  src->fill(0xff);maps->fill(0xfe);attrs->fill(1);
  c.placementBackgroundIndex=bg;c.bg1GraphicsBase=mode==8?32768:0;
  c.collisionAttributes=mode==1 || mode==2?nullptr:attrs->data();
  const bool explicitFrame=mode!=0 && mode!=2;
  RemasterProfile profile;
  for(int tile=1;tile<64;tile++) {
   auto &a=profile.assets[ids[tile]];a.tileId=ids[tile];a.hasHeight=true;
   a.tileRole=explicitFrame?RemasterTileRole::Doorway:RemasterTileRole::Unknown;
   a.hasDoorframeMask=explicitFrame;a.doorframeMask=UINT64_MAX;
   for(int k=0;k<64;k++)a.height[k]=7-(d<2?k/8:k%8);
  }
  auto &floor=profile.assets[ids[40]];floor.tileRole=RemasterTileRole::Floor;floor.hasDoorframeMask=false;floor.height.fill(0);
  const int w=d<2?4:3,h=d<2?3:4,off=offsets[d];
  auto structural=[&](int x,int y){return d<2?(x==0 || x==w-1 || y==(d==0?0:h-1)):(y==0 || y==h-1 || x==(d==2?0:w-1));};
  auto put=[&](int at,int word){(*maps)[at]=word;(*maps)[at+1]=word>>8;};
  int first=-1,last=-1;
  for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
   const int word=structural(x,y)?1+x*h+y:40;
   (*src)[off+2*(x*h+y)]=word;(*src)[off+2*(x*h+y)+1]=0;
   const int px=flip&1?w-1-x:x,py=flip&2?h-1-y:y;
   const int owner=split?((d==0?y==0:d==1?y==h-1:d==2?x==0:x==w-1)?1:0):bg;
   const int at=(owner==bg?0:8192)+2*((20+py)*64+20+px);
   put(at,(mode==7 && !structural(x,y)?41:word)^((flip&1?0x4000:0)|(flip&2?0x8000:0))^(mode==6?0x2000:0));
   if(structural(x,y)){if(first<0)first=at;last=at;}
  }
  if(mode==4)put(first,0xffff);if(mode==5)put(last,0xffff);
  if(mode!=3)(*attrs)[20*64+20]=0x80+(d>=2);
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  const bool accepted=mode!=2 && mode!=4 && mode!=5 && !(mode==8 && split);
  const int direction=d^((flip&(d<2?2:1))?1:0);int expected=0,seen=0;
  for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
   const int owner=split?((d==0?y==0:d==1?y==h-1:d==2?x==0:x==w-1)?1:0):bg;
   const int px=flip&1?w-1-x:x,py=flip&2?h-1-y:y;
   const auto &t=(*tiles)[(20+py)*64+20+px];
   if(accepted && structural(x,y) && owner==bg) {
    expected++;need(t.doorAssembly==20*64+21 && t.doorDirection==direction,"exact structural ownership lost");
    need(t.doorTemplateOffset==off && t.splitDoorPart==bool(split),"source/order/split changed");
   }
   if(mode==7 && !structural(x,y))need(!t.doorAssembly,"overwritten opening retained owner");
  }
  for(const auto &t:*tiles)if(t.doorTemplateOffset && t.surface==C::Surface::Door)seen++;
  if(seen!=expected){std::cerr<<"case "<<d<<','<<flip<<','<<bg<<','<<split<<','<<mode<<" seen="<<seen<<" expected="<<expected<<'\n';throw std::runtime_error("structural denominator");}
  if(inventory)for(const auto &t:*tiles)dungeonPlacementInventory(inventory,t);
  cases++;
 }
 std::cout<<"PASS "<<cases<<" collision-only/exact/missing/absent/mismatch/priority/overwrite/base cases; both BGs, axes, H/V, splits\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
