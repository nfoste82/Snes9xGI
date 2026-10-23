// Offscreen children require exact latched producer identity, never a partial motif.
#include "sprite_obstacle.h"
#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>
struct ClippedFixture {
 RemasterFrame f;RemasterProfile profile;RemasterDungeonFloorContext c;
 std::unique_ptr<RemasterDungeonHeightMap> map=std::make_unique<RemasterDungeonHeightMap>();
 std::array<uint8_t,65536> graphics{};RemasterSpriteObstacleDiagnostics d;
 ClippedFixture(int x,int y,int base=24) {
  f.width=128;f.height=96;f.mainPixels.resize(128*96);
  c.verifiedAlttpRom=c.indoors=c.hasTerrainActors=c.hasObstacleActors=true;c.roomIndex=81;
  c.actorScrollX=c.backgroundScrollX=576;c.actorScrollY=c.backgroundScrollY=2624;
  c.graphics=graphics.data();c.obstacleActors[0]={uint16_t(576+x),uint16_t(2624+y),0xee,9,0,0};
  map->roomIndex=81;map->hasPlaneOffsets=true;auto rooms=std::make_shared<RemasterRoomDiagnostics>();map->roomDiagnostics=rooms;
  for(int p=0;p<2;p++)for(int i=0;i<4096;i++) {
   auto &v=rooms->cells[p*4096+i];v.floorId=7+p;v.floorBase=p?0:base;v.planeObserved=true;
   map->planeOffsets[p][i]=v.floorBase;
  }
  RemasterObstacleGroup g;g.name="producer";g.spriteAssembly=true;
  const int dx[]={32,16,0,32,16,0},dy[]={16,16,16,0,0,0};
  const uint8_t names[]={0x2c,0x2e,0x2c,0x0c,0x0e,0x0c},flags[]={0x71,0x31,0x31,0x71,0x31,0x31};
  // Distinct, reproducible canonical identities. Source pages/flips match the
  // real six-slot dispatch, not merely an authored rectangular footprint.
  for(int part=0;part<6;part++) {
   auto &o=c.terrainOam[116+part];o.x=(x+dx[part])&511;o.y=uint8_t(y+dy[part]);o.name=names[part];o.flags=flags[part];o.size=2;
   for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
    int n=(names[part]|256)+((flags[part]&64)?1-xx:xx)+yy*16;
    for(int b=0;b<32;b++)graphics[0x8000+n*32+b]=uint8_t(n*37+b*11);
   }
  }
  for(int part=0;part<6;part++)for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
   int tx=x+dx[part]+xx*8,ty=y+dy[part]+yy*8;
   int n=(names[part]|256)+((flags[part]&64)?1-xx:xx)+yy*16;uint8_t pixels[64];
   for(int py=0;py<8;py++)for(int px=0;px<8;px++) {int v=0;for(int b=0;b<4;b++)v|=((graphics[0x8000+n*32+b/2*16+py*2+b%2]>>(7-px))&1)<<b;pixels[py*8+px]=v;}
   RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
   g.members.push_back({id,(dx[part]+xx*8)/8,(dy[part]+yy*8)/8,0,bool(flags[part]&64),false,dy[part]+yy*8==24});
   RemasterFrameAssetMetadata meta;meta.tileId=id;meta.hasHeight=meta.hasNormals=true;meta.height.fill(7);meta.normalXyz.fill(128);f.assetMetadata.push_back(meta);
   if(tx+8<=0 || ty+8<=0 || tx>=128 || ty>=96)continue;
   RemasterFrameTileInstance i;i.source=RemasterSourceType::Object;i.sourceIndex=116+part;i.tileNumber=n;i.vramAddress=uint16_t(0x8000+n*32);i.tileId=id;i.hFlip=flags[part]&64;i.palette=(flags[part]>>1)&7;
   f.tileInstances.push_back(i);
   for(int py=0;py<8;py++)for(int px=0;px<8;px++)if(tx+px>=0 && ty+py>=0 && tx+px<128 && ty+py<96) {
    auto &v=f.mainPixels[(ty+py)*128+tx+px];v.instanceId=f.tileInstances.size();v.tilePixel=py*8+(i.hFlip?7-px:px);
   }
  }
  profile.obstacleGroups.push_back(g);
 }
 void apply(){S9xRemasterApplySpriteObstacles(f,profile,&c,map.get(),&d);}
 void solved(int base) {
  apply();
  assert(d.resolvedAssemblies==1 && d.assemblies[0].producerActor==0 && d.assemblies[0].plane==0 && d.assemblies[0].base==base);
  RemasterLightingFieldCache fields;fields.update(f,false);
  for(size_t p=0;p<f.mainPixels.size();p++)if(f.mainPixels[p].instanceId) {
   const auto &i=f.tileInstances[f.mainPixels[p].instanceId-1];assert(i.heightOffset==base && !S9xRemasterInstanceGeometryUnresolved(i));assert(fields.height[p*2+1]);
  }
  // Paused authoring must retain proof without dereferencing the old VRAM.
  S9xRemasterApplySpriteObstacles(f,profile,nullptr,nullptr,&d);assert(d.resolvedAssemblies==1);
 }
};
int main() {
 for(int base:{-8,0,24,50})for(auto xy:{std::pair<int,int>{24,-8},{-8,24},{100,24},{24,80},{100,-8}}) {
  ClippedFixture f(xy.first,xy.second,base);f.solved(base);
 }
 for(int negative=0;negative<9;negative++) {
  ClippedFixture f(24,-8);
  if(negative==0)f.c.hasObstacleActors=false;
  if(negative==1)f.c.graphics=nullptr;
  if(negative==2)f.c.terrainOam[120].name^=1;
  if(negative==3)for(auto &p:f.f.mainPixels)if(p.instanceId==1)p.instanceId=0; // on-screen missing != clipped
  if(negative==4)f.c.obstacleActors[1]=f.c.obstacleActors[0];
  if(negative==5)for(int i=0;i<4096;i++)f.map->planeOffsets[0][i]=255;
  if(negative==6)f.profile.obstacleGroups[0].members.pop_back(); // sparse authoring
  if(negative==7)f.f.tileInstances[0].vramAddress^=0x2000;
  if(negative==8)f.c.terrainOam[100]=f.c.terrainOam[120]; // duplicate hidden producer slot
  f.apply();assert(!f.d.resolvedAssemblies);
 }
 std::cout<<"PASS clipped producer 20 signed/base/camera cases +9 ownership/metadata/support negatives\n";
}
