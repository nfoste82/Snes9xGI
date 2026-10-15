#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=80;
 c.placements=tiles->data();c.collisionAttributes=attrs->data();c.constructedMaps=words->data();c.graphics=gfx->data();
 c.stairCount=1;c.stairs[0]={40,40,0,0,true,true};
 for(int p=0;p<2;p++)for(int y=10;y<16;y++)for(int x=10;x<16;x++) {
  (*attrs)[p*4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=RemasterDungeonFloorContext::Surface::Floor;t.hasHeight=true;
 }
 const int border=12*64+16;
 for(int bg:{0,1})for(int flip=0;flip<4;flip++) {
  c.placementBackgroundIndex=bg;(*words)[border*2]=1;(*words)[border*2+1]=(flip&1?0x40:0)|(flip&2?0x80:0);
  (*gfx)[32]=0;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && !map->structuralConflicts,"transparent artwork created conflict");
  need(map->offsets[border]==255 && !map->fallbackDirections[border],"transparent artwork got fallback geometry");
  need(map->planeOffsets[0][12*64+15]==0 && map->planeOffsets[1][12*64+15]==0,"independent support lost");
  (*gfx)[32]=1;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->structuralConflicts && map->offsets[border]==255,"visible ambiguous geometry accepted");
 }
 std::cout<<"PASS transparent cleared cells both BGs/allflips: no invented fallback/conflict; independent support retained; nonempty ambiguity rejected\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
