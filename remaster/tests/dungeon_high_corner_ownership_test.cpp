// HIGH curved corner columns retain ownership after companion overwrites.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto templates=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterProfile profile;std::array<RemasterTileContentId,4> ids;
 for(int n=0;n<4;n++) {
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*graphics)[n*32+(b/2)*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[ids[n]];
  a.hasHeight=true;for(int k=0;k<64;k++)a.height[k]=31-k/8;
 }
 profile.assetGroups["wall_faces"].tileIds={ids[0],ids[2]};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.dungeonTemplateData=templates->data();
 auto word=[&](int cell,int v){(*maps)[cell*2]=v;(*maps)[cell*2+1]=v>>8;};
 const int cell=10*64+10;
 for(int offset:{0xb66,0xb86,0xba6,0xbc6,0xc66,0xc86,0xca6,0xcc6})for(int flip=0;flip<4;flip++) {
  maps->fill(0xff);templates->fill(0xff);
  for(int k=0;k<3;k++) {int v=k|(flip<<14);(*templates)[offset+2*k]=v;(*templates)[offset+2*k+1]=v>>8;word(cell+k*64,v);}
  auto classify=[&]{S9xRemasterDungeonPlacements(c,profile,*tiles);};
  classify();assert(tiles->at(cell+64).highCornerFace && tiles->at(cell+64).wallTemplateOffset==offset);
  assert(tiles->at(cell+64).height==profile.assets[ids[1]].height);
  word(cell+128,3|(flip<<14));classify();assert(!tiles->at(cell+64).highCornerFace); // partial overwrite
  word(cell+128,2|(flip<<14));profile.assetGroups["obstacles"].tileIds={ids[1]};
  classify();assert(!tiles->at(cell+64).highCornerFace);profile.assetGroups.erase("obstacles");
  profile.assets[ids[1]].hasHeight=false;classify();assert(!tiles->at(cell+64).highCornerFace);
  profile.assets[ids[1]].hasHeight=true;
  // Same final column claimed by two distinct HIGH templates: no ownership.
  const int duplicate=offset==0xb66?0xb86:0xb66;
  std::copy_n(templates->data()+offset,6,templates->data()+duplicate);
  classify();assert(!tiles->at(cell+64).highCornerFace);
 }
 std::cout<<"PASS 32 transformed high-corner columns; overwrite, obstacle and ambiguous-owner rejection\n";
}
