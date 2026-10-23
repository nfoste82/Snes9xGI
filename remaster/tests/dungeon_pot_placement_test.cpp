#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main(){
 using S=RemasterDungeonFloorContext::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 RemasterProfile profile;
 for(int tile=1;tile<=4;tile++){(*gfx)[tile*32]=tile;uint8_t pixels[64]={};for(int x=0;x<8;x++)pixels[x]=((*gfx)[tile*32]>>(7-x))&1;
  RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};RemasterAssetMetadata a;a.tileId=id;a.hasHeight=true;a.height.fill(tile);profile.assets[id]=a;
  (*tpl)[0xe82+(tile-1)*2]=tile;
 }
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();c.collisionAttributes=attr->data();
 auto out=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 for(int flip=0;flip<4;flip++)for(int plane=0;plane<2;plane++) {
  maps->fill(0);attr->fill(1);
  for(int x=0;x<2;x++)for(int y=0;y<2;y++){int i=(20+y)*64+20+x,sx=flip&1?1-x:x,sy=flip&2?1-y:y;int w=1+sx*2+sy+(flip&1?0x4000:0)+(flip&2?0x8000:0);(*maps)[2*i]=w;(*maps)[2*i+1]=w>>8;(*attr)[plane*4096+i]=0x75;}
  S9xRemasterDungeonPlacements(c,profile,*out);
  for(int y=0;y<2;y++)for(int x=0;x<2;x++){const auto &a=(*out)[(20+y)*64+20+x];assert(a.surface==S::Obstacle && a.removableSupportPlanes==(1<<plane));}
  (*maps)[2*(20*64+20)]^=1;S9xRemasterDungeonPlacements(c,profile,*out);for(auto &a:*out)assert(!a.removableSupportPlanes);
  (*maps)[2*(20*64+20)]^=1;(*attr)[plane*4096+20*64+20]=0;S9xRemasterDungeonPlacements(c,profile,*out);for(auto &a:*out)assert(!a.removableSupportPlanes);
  // Same artwork with ordinary attributes is a decorative/fake pot, not a
  // pickup producer. All-zero collision alone cannot create structural support.
  for(int y=0;y<2;y++)for(int x=0;x<2;x++)(*attr)[plane*4096+(20+y)*64+20+x]=0;
  S9xRemasterDungeonPlacements(c,profile,*out);for(auto &a:*out)assert(!a.removableSupportPlanes);
  // Mixed indexed ownership must reject even when each byte is liftable.
  for(int y=0;y<2;y++)for(int x=0;x<2;x++)(*attr)[plane*4096+(20+y)*64+20+x]=0x75+x;
  S9xRemasterDungeonPlacements(c,profile,*out);for(auto &a:*out)assert(!a.removableSupportPlanes);
 }
 std::cout<<"pot templates all flips / planes / overwrite / mixed index rejection PASS\n";
}
