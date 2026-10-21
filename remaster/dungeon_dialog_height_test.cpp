#include "remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 for(int y=10;y<20;y++)for(int x=10;x<20;x++)(*attr)[y*64+x]=0;
 uint8_t indices[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].hasHeight=true;profile.assetGroups["dungeon_floor"].tileIds={id};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.romIdentity=123;c.roomIndex=2;
 c.linkX=c.linkY=80;c.backgroundScrollX=c.backgroundScrollY=80;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.collisionAttributes=attr->data();
 c.bg1GraphicsBase=0;
 S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);auto &s=S9xRemasterState();
 uint16_t screen[64]={};auto output=std::make_unique<RemasterFrame>();
 auto run=[&]{S9xRemasterBeginFrame(64,8,8,8);
  RemasterFrameTileInstance bg;bg.source=RemasterSourceType::Background;bg.tileId=id;
  s.tileInstances={bg,bg};s.tileInstances[1].sourceIndex=1;
  for(int k=0;k<64;k++){s.mainOwners[k]=0x02000000;s.mainInstanceIds[k]=1;s.mainTilePixels[k]=k;s.subOwners[k]=0x02010000;s.subInstanceIds[k]=2;s.subTilePixels[k]=k;}
  S9xRemasterEndFrame(screen,8,8,8,output.get(),&c);
 };
 run();const auto epoch=s.runtimeDungeonHeightEpoch;
 // Seed reviewed render output to test application independently of solver
 // quality: distinct BG bases and placement-local relief.
 const int cell=10*64+10;
 s.runtimeDungeonBg1HeightMap.offsets[cell]=24;s.runtimeDungeonHeightMap.offsets[cell]=40;
 RemasterDungeonHeightMap::LocalHeightPreview local;local.cell=cell;
 for(int k=0;k<64;k++)local.height[k]=7-k%8;
 s.runtimeDungeonBg1HeightMap.localHeightPreviews.push_back(local);
 s.runtimeDungeonHeightMap.localHeightPreviews.push_back(local);
 auto verify=[&]{for(auto pixels:{&output->mainPixels,&output->subPixels})for(const auto &px:*pixels) {
  const auto &i=output->tileInstances.at(px.instanceId-1);
  check(i.heightOffset==(i.sourceIndex?40:24) && i.hasPlacementHeight && i.placementHeight[px.tilePixel]==7-px.tilePixel%8,"dialog reset actual BG bases/local geometry");
 }};
 run();verify();
 const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
 // Previously consumed entry support must survive the text suspension.
 s.runtimeEntryRoom=2;s.runtimeEntrySupport.cell=650;s.runtimeEntrySupport.base=24;
 c.module=14;c.submodule=2;c.savedModule=7;
 S9xRemasterObserveDungeonTransition(c);
 check(s.runtimeEntryRoom==2 && s.runtimeEntrySupport.cell==650,"dialog lost entry support");
 s.runtimeEntryRoom=0xffff;s.runtimeEntrySupport={};
 for(int phase:{0,1,8,255}) {
  c.constructionPhase=phase;run();verify();
  check(s.runtimeDungeonHeightEpoch==epoch,"dialog re-solved unchanged inputs");
  check(inventory==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"dialog changed complete maps");
  check(!s.outgoingGeometry.valid,"text handler mistaken for door scroll");
 }
 c.module=7;c.submodule=0;run();verify();
 c.module=14;c.submodule=2;c.savedModule=7;
 // A dialog is not permission to freeze stale input or bypass safety gates.
 (*attr)[650]=1;run();check(s.runtimeDungeonHeightEpoch>epoch,"dialog ignored collision edits");
 for(int saved:{0,9,255}) {c.savedModule=saved;check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"non-dungeon/unknown saved module accepted");}
 c.savedModule=7;for(int handler:{0,1,3,7}){c.submodule=handler;check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"menu accepted");}
 c.submodule=2;c.collisionMode=1;check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"unsupported collision accepted");
 c.collisionMode=0;c.indoors=false;check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"outdoor text accepted");
 c.indoors=true;c.graphics=nullptr;check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"incomplete text inputs accepted");
 c.graphics=graphics->data();c.module=7;c.submodule=2;c.constructionPhase=0;
 check(!S9xRemasterDungeonHeightInputsPublishable(c,2),"partial dungeon loader accepted");
 std::cout<<"PASS dialog actual both-BG bases/local geometry, complete maps, entry support, return, exact invalidation and negative gates\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
