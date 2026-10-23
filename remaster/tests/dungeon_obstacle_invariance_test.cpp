// Shared role/assembly/apply contract, not a room/hash regression.
#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try{
 using Surface=RemasterDungeonFloorContext::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto templates=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterProfile p;std::array<RemasterTileContentId,8> ids;
 for(int n=0;n<8;n++){
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*graphics)[n*32+(b/2)*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=p.assets[ids[n]];a.hasHeight=a.hasNormals=true;
  for(int k=0;k<64;k++)a.height[k]=(k*7+n*3)%31;a.normalXyz.fill(128);
  (*templates)[0x84a+n*2]=n;
 }
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.dungeonTemplateData=templates->data();
 auto set=[&](int x,int y,int word){(*maps)[2*(y*64+x)]=word;(*maps)[2*(y*64+x)+1]=word>>8;};
 for(int flip=0;flip<4;flip++){
  maps->fill(0xff);
  for(int x=0;x<2;x++)for(int y=0;y<4;y++)set(10+x,10+y,((flip&1?1-x:x)*4+(flip&2?3-y:y))|(flip&1?0x4000:0)|(flip&2?0x8000:0)|0x2000);
  S9xRemasterDungeonPlacements(c,p,*tiles);
  for(int x=0;x<2;x++)for(int y=0;y<4;y++){
   const auto &t=(*tiles)[(10+y)*64+10+x];check(t.surface==Surface::Obstacle,"transformed complete column");
   check(t.height==p.assets[ids[(flip&1?1-x:x)*4+(flip&2?3-y:y)]].height,"authored relief changed");
  }
  set(10,10,1023);S9xRemasterDungeonPlacements(c,p,*tiles);
  check((*tiles)[11*64+10].surface==Surface::Unknown,"partial overwrite accepted");
 }
 // Explicit role beats stale wall group and inferred structural usage.
 p.assetGroups["wall_faces"].tileIds={ids[0]};p.assets[ids[0]].tileRole=RemasterTileRole::Obstacle;
 set(2,2,0);S9xRemasterDungeonPlacements(c,p,*tiles);check((*tiles)[2*64+2].surface==Surface::Obstacle,"explicit obstacle precedence");
 // Overlapping complete claims with repeated alternating columns have no owner.
 maps->fill(0xff);for(int x=0;x<3;x++)for(int y=0;y<4;y++)set(20+x,20+y,(x%2)*4+y);
 p.assetGroups.clear();p.assets[ids[0]].tileRole=RemasterTileRole::Unknown;
 for(int y=0;y<4;y++){(*templates)[0x85a+y*2]=4+y;(*templates)[0x85a+(4+y)*2]=y;}
 S9xRemasterDungeonPlacements(c,p,*tiles);check((*tiles)[21*64+21].surface==Surface::Unknown,"ambiguous owner accepted");
 for(bool explicitRole:{false,true})for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int base:{-24,0,37,83}){
  auto f=std::make_unique<RemasterFrame>();f->width=f->height=8;f->mainPixels.resize(64);f->subPixels.resize(64);
  f->scenePixels.resize(64);
  RemasterFrameAssetMetadata a;a.tileId=ids[0];a.hasHeight=a.hasNormals=true;a.height=p.assets[ids[0]].height;a.normalXyz=p.assets[ids[0]].normalXyz;
  a.tileRole=explicitRole?RemasterTileRole::Obstacle:RemasterTileRole::Unknown;f->assetMetadata.push_back(a);
  RemasterFrameTileInstance i;i.tileId=ids[0];i.source=RemasterSourceType::Background;i.sourceIndex=bg;i.hFlip=flip&1;i.vFlip=flip&2;
  if(explicitRole)i.assetGroup="stair_treads"; // obsolete group cannot veto explicit obstacle support
  f->tileInstances.push_back(i);
  for(int k=0;k<64;k++){f->mainPixels[k].instanceId=f->subPixels[k].instanceId=f->scenePixels[k].instanceId=1;f->mainPixels[k].tilePixel=f->subPixels[k].tilePixel=f->scenePixels[k].tilePixel=k;}
  auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=true;m->offsets.fill(255);m->offsets[0]=base;
  m->producerHashes[0]=ids[0].hash;m->producerFlips[0]=flip;m->producerObstacle[0]=!explicitRole;
  m->fallbackDirections[0]=15;m->structuralPlaneOffsets[0][0]=base;m->mixedFloorBases[0][0]=base+12;
  RemasterDungeonHeightMap::LocalHeightPreview preview;preview.cell=0;preview.height.fill(5);m->localHeightPreviews.push_back(preview);
  S9xRemasterApplyDungeonHeightMap(*f,c,*m,nullptr,nullptr,bg);
  // Independent scene path uses separate instances, as live EndFrame does.
  auto scene=std::make_unique<RemasterFrame>();scene->width=scene->height=8;scene->mainPixels=f->scenePixels;scene->tileInstances.push_back(i);scene->assetMetadata.push_back(a);
  S9xRemasterApplyDungeonHeightMap(*scene,c,*m,nullptr,nullptr,bg);
  for(const auto *pixels:{&f->mainPixels,&f->subPixels})for(const auto &px:*pixels){const auto &applied=f->tileInstances[px.instanceId-1];
   check(!applied.hasPlacementHeight && applied.heightOffset==base,"obstacle local replacement/base");
   check(applied.heightOffset+a.height[px.tilePixel]==base+a.height[px.tilePixel],"signed composition");}
  check(f->assetMetadata[0].normalXyz==a.normalXyz,"normal mutation");
  check(!scene->tileInstances[0].hasPlacementHeight && scene->tileInstances[0].heightOffset==base,"independent scene obstacle");
 }
 RemasterFrameTileInstance normalInstance;normalInstance.hasPlacementHeight=true;
 RemasterFrameAssetMetadata normalMetadata;normalMetadata.hasHeight=normalMetadata.hasNormals=true;
 normalMetadata.height.fill(24);normalMetadata.normalXyz.fill(128);
 for(int k=0;k<64;k++){normalInstance.placementHeight[k]=31-k%8-k/8;normalMetadata.normalXyz[k*3+2]=255;}
 check(S9xRemasterContinuedDiagonalNormalMask(normalInstance,&normalMetadata)!=0,"normal fixture must exercise correction");
 normalMetadata.tileRole=RemasterTileRole::Obstacle;
 check(!S9xRemasterContinuedDiagonalNormalMask(normalInstance,&normalMetadata),"obstacle normal correction not vetoed");
 std::cout<<"PASS transformed/overwritten/ambiguous assembly, explicit precedence, both BG/buffers, signed bases, border/floor/preview veto\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
