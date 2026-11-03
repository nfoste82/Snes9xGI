// Independent expected ownership plus optional complete-placement differential.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char **argv) {
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
 std::ofstream inventory;if(argc==2)inventory.open(argv[1]);int cases=0;
 auto word=[&](int cell,int v){(*maps)[cell*2]=v;(*maps)[cell*2+1]=v>>8;};
 for(int axis=0;axis<2;axis++)for(int bg=0;bg<2;bg++)for(int edge:{0,1})
 for(int offset:{0xb66,0xb86,0xba6,0xbc6,0xc66,0xc86,0xca6,0xcc6})for(int flip=0;flip<4;flip++) {
  c.placementBackgroundIndex=bg;const int stride=axis?1:64;
  const int cell=edge?(axis?63*64+61:61*64+63):10*64+10;
  maps->fill(0xff);templates->fill(0xff);
  for(int k=0;k<3;k++) {int v=k|(flip<<14);int part=axis?k*4:k;
   (*templates)[offset+2*part]=v;(*templates)[offset+2*part+1]=v>>8;word(cell+k*stride,v);}
  auto classify=[&]{S9xRemasterDungeonPlacements(c,profile,*tiles);cases++;
   if(inventory.is_open()){std::ostringstream all;for(const auto &t:*tiles)dungeonPlacementInventory(all,t);
    uint64_t digest=UINT64_C(14695981039346656037);for(unsigned char ch:all.str())digest=(digest^ch)*UINT64_C(1099511628211);
    inventory<<cases<<' '<<digest<<'\n';}};
  classify();assert(tiles->at(cell+stride).highCornerFace && tiles->at(cell+stride).wallTemplateOffset==offset);
  assert(tiles->at(cell+stride).height==profile.assets[ids[1]].height);
  word(cell+2*stride,3|(flip<<14));classify();assert(!tiles->at(cell+stride).highCornerFace);
  word(cell+2*stride,2|(flip<<14));profile.assetGroups["obstacles"].tileIds={ids[1]};
  classify();assert(!tiles->at(cell+stride).highCornerFace);profile.assetGroups.erase("obstacles");
  profile.assets[ids[1]].hasHeight=false;classify();assert(!tiles->at(cell+stride).highCornerFace);
  profile.assets[ids[1]].hasHeight=true;
  // Masked index may admit priority-only mismatches; exact full predicate MUST
  // still reject them in this consumer (unlike ordinary masked door frames).
  word(cell,0x2000|(flip<<14));classify();assert(!tiles->at(cell+stride).highCornerFace);
  word(cell,flip<<14);
  const int duplicate=offset==0xb66?0xb86:0xb66;
  std::copy_n(templates->data()+offset,32,templates->data()+duplicate);
  classify();assert(!tiles->at(cell+stride).highCornerFace);
 }
 std::cout<<"PASS corner candidates "<<cases<<" axes/BGs/edges/HV/overwrite/missing/priority/ambiguous\n";
}
