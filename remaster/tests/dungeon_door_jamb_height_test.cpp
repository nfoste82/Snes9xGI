// Passage collision must not discard an independently bounded door-wall jamb.
#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 C c;c.verifiedAlttpRom=true;c.placements=tiles->data();c.collisionAttributes=attrs->data();
 c.constructedMaps=words->data();c.dungeonTemplateData=tpl->data();
 for(int n=0;n<12;n++){int word=0x800+n;(*tpl)[0x1470+n*2]=word;(*tpl)[0x1471+n*2]=word>>8;}
 for(int flip=0;flip<4;flip++)for(int base:{-256,0,32,300}) {
  tiles->fill({});words->fill(0);attrs->fill(0);
  map->offsets.fill(255);for(auto &p:map->structuralPlaneOffsets)p.fill(255);
  const int origin=12*64+18;
  for(int xx=0;xx<4;xx++)for(int yy=0;yy<3;yy++) {
   int sx=flip&1?3-xx:xx,sy=flip&2?2-yy:yy;
   int word=(0x800+sx*3+sy)^((flip&1?0x4000:0)|(flip&2?0x8000:0));word|=0x2000;
   int i=origin+yy*64+xx;(*words)[i*2]=word;(*words)[i*2+1]=word>>8;
   auto &t=(*tiles)[i];t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int k=0;k<64;k++)t.height[k]=7-k/8;
   if(yy!=1){map->offsets[i]=map->structuralPlaneOffsets[0][i]=base+(flip&2?yy-1:1-yy)*8;(*attrs)[i]=1;}
  }
  const int i=origin+64;
  S9xRemasterAttachWallMounts(c,*map);
  need(map->offsets[i]==base && map->offsets[i+3]==base,"all transformed signed jambs attach");
  need(map->offsets[i+1]==255 && map->offsets[i+2]==255,"opening does not become a wall/support bridge");
  need(map->localHeightPreviews.empty() && (*tiles)[i].height[0]==7,"authored relief preserved");
  map->roomIndex=c.roomIndex;map->hasRoomGeometry=true;
  map->producerHashes[i]=123;map->producerFlips[i]=flip;
  RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance instance;instance.tileId={123,1,4};instance.source=RemasterSourceType::Background;
  instance.sourceIndex=1;instance.hFlip=flip&1;instance.vFlip=flip&2;f.tileInstances.push_back(instance);
  RemasterFrameAssetMetadata meta;meta.tileId=instance.tileId;meta.hasHeight=true;meta.height=(*tiles)[i].height;f.assetMetadata.push_back(meta);
  for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
  auto camera=c;camera.backgroundScrollX=i%64*8;camera.backgroundScrollY=i/64*8;
  S9xRemasterApplyDungeonHeightMap(f,camera,*map);
  for(int k=0;k<64;k++)need(f.tileInstances[f.mainPixels[k].instanceId-1].heightOffset==base,"runtime must apply signed terminal base");
  auto reject=[&](const char *why){map->offsets[i]=map->structuralPlaneOffsets[0][i]=255;S9xRemasterAttachWallMounts(c,*map);need(map->offsets[i]==255,why);};
  (*words)[origin*2]^=1;reject("partial footprint rejected");(*words)[origin*2]^=1;
  map->structuralPlaneOffsets[0][origin]=255;reject("missing end rejected");map->structuralPlaneOffsets[0][origin]=base+(flip&2?-8:8);
  map->structuralPlaneOffsets[0][origin+128]++;reject("contradictory ends rejected");map->structuralPlaneOffsets[0][origin+128]--;
  (*attrs)[origin]=0;reject("underpass end rejected");(*attrs)[origin]=1;
  (*tiles)[i].height[9]++;reject("nonaffine relief rejected");(*tiles)[i].height[9]--;
  (*tiles)[i].surface=S::Door;reject("incompatible semantic role rejected");(*tiles)[i].surface=S::Wall;
  for(int end:{origin,origin+128}){(*attrs)[4096+end]=1;map->structuralPlaneOffsets[1][end]=map->structuralPlaneOffsets[0][end]+4;}
  reject("multiple differing structural owners rejected");
 }
 std::cout<<"PASS signed/flipped door jambs, exact assembly and independent-end negatives\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
