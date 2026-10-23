#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=1;
 c.placements=tiles->data();c.collisionAttributes=attrs->data();c.constructedMaps=maps->data();c.dungeonTemplateData=tpl->data();
 // Sixteen-band authored structure on unobserved plane0, anchored only by
 // an established isolated authored cap. Plane1 remains an independent floor.
 for(int flip=0;flip<4;flip++) {
  tiles->fill({});attrs->fill(1);
  for(int y=10;y<30;y++)for(int x=8;x<16;x++){(*attrs)[4096+y*64+x]=0;auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;t.floorPlaneMask=2;}
  const int cap=9*64+10;(*tiles)[cap].surface=S::Floor;(*tiles)[cap].hasHeight=true;(*tiles)[cap].height.fill(136);
  for(int n=0;n<16;n++){auto &t=(*tiles)[(10+n)*64+10];t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   (*attrs)[4096+(10+n)*64+10]=1;
   if(n==0)t.wallTemplateOffset=0x2e8;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=flip&2?y:7-y;}
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"tall solve");
  need(map->structuralPlaneOffsets[0][10*64+10]==120 && map->structuralPlaneOffsets[0][25*64+10]==0 && map->planeOffsets[1][25*64+11]==0,"stack beyond four retains signed structural solve and independent support");
  (*tiles)[cap].surface=S::Unknown;(*attrs)[4096+cap]=1;
  for(int n=0;n<16;n++)(*attrs)[4096+(10+n)*64+10]=0;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*map) && map->offsets[25*64+10]==255,"nonblocking underpass cannot anchor unobserved tall wall");
 }
 // Complete transformed side torch inherits the entire backing cross section,
 // not actor floor0 or one landing-height constant. Source pixels are canonical.
 for(int flip=0;flip<4;flip++) {
  tiles->fill({});maps->fill(0);map->offsets.fill(255);for(auto &a:map->structuralPlaneOffsets)a.fill(255);map->localHeightPreviews.clear();
  const int x=20,y=20;for(int n=0;n<8;n++){int word=0x800+n;(*tpl)[0xa44+n*2]=word;(*tpl)[0xa45+n*2]=word>>8;}
  for(int yy=0;yy<4;yy++)for(int xx=0;xx<2;xx++) {
   int sx=flip&1?1-xx:xx,sy=flip&2?3-yy:yy,word=(0x800+sx*4+sy)^((flip&1?0x4000:0)|(flip&2?0x8000:0));word|=0x2400;
   int i=(y+yy)*64+x+xx;(*maps)[i*2]=word;(*maps)[i*2+1]=word>>8;auto &t=(*tiles)[i];t.surface=S::Wall;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int k=0;k<64;k++)t.height[k]=7-k/8; // wrong-axis authored template remains immutable
  }
  for(int xx=0;xx<2;xx++)for(int yy:{y-1,y+4}){int i=yy*64+x+xx;auto &t=(*tiles)[i];t.surface=S::Wall;t.hasHeight=true;
   for(int k=0;k<64;k++)t.height[k]=7-k%8;map->offsets[i]=map->structuralPlaneOffsets[0][i]=40+8*xx;}
  S9xRemasterAttachWallMounts(c,*map);int i=y*64+x;
  need(map->offsets[i]==40 && map->localHeightPreviews.size()==8,"full fixture attaches through both ends");
  for(auto &v:map->localHeightPreviews)for(int k=0;k<64;k++)need(v.height[k]==(flip&1?k%8:7-k%8),"displayed backing ramp transformed to canonical source");
  need((*tiles)[i].height[0]==7 && (*tiles)[i].height[63]==0,"authored array mutated");
  map->roomIndex=c.roomIndex;map->hasRoomGeometry=true;map->producerHashes[i]=123;map->producerFlips[i]=flip;
  RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance instance;instance.tileId={123,1,4};instance.source=RemasterSourceType::Background;instance.sourceIndex=1;instance.hFlip=flip&1;instance.vFlip=flip&2;f.tileInstances.push_back(instance);
  RemasterFrameAssetMetadata meta;meta.tileId=instance.tileId;meta.hasHeight=true;meta.height=(*tiles)[i].height;f.assetMetadata.push_back(meta);
  for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
  auto camera=c;camera.backgroundScrollX=x*8;camera.backgroundScrollY=y*8;
  S9xRemasterApplyDungeonHeightMap(f,camera,*map);
  for(int k=0;k<64;k++){const auto &v=f.tileInstances[f.mainPixels[k].instanceId-1];need(v.hasPlacementHeight && v.heightOffset==40 && v.placementHeight[k]==(flip&1?k%8:7-k%8),"visible runtime does not consume exact mounted source");}
  map->offsets[i]=255;map->localHeightPreviews.clear();(*maps)[i*2]^=1;
  S9xRemasterAttachWallMounts(c,*map);need(map->offsets[i]==255 && map->localHeightPreviews.empty(),"partial overwrite attachment accepted");
  (*maps)[i*2]^=1;map->structuralPlaneOffsets[0][(y+4)*64+x]=41;
  S9xRemasterAttachWallMounts(c,*map);need(map->localHeightPreviews.empty(),"incompatible backing ends accepted");
  map->structuralPlaneOffsets[0][(y+4)*64+x]=40;
  for(int xx=0;xx<2;xx++)for(int yy:{y-1,y+4})map->structuralPlaneOffsets[1][yy*64+x+xx]=50+8*xx;
  S9xRemasterAttachWallMounts(c,*map);need(map->localHeightPreviews.empty(),"incompatible overlapping structural owners accepted");
 }
 std::cout<<"PASS tall structural stacks, independent planes, transforms, terminal mounted sources, overwrite/conflict rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
