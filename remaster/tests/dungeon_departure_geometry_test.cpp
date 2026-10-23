#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=0;
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto gfx=std::make_unique<std::array<uint8_t,65536>>();auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 uint8_t indices[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};RemasterProfile p;p.upperFloorHeight=50;p.assets[id].hasHeight=true;p.assetGroups["dungeon_floor"].tileIds={id};
 for(int y=10;y<20;y++)for(int x=10;x<20;x++)(*attr)[y*64+x]=0;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.romIdentity=123;c.roomIndex=0x61;c.linkX=c.linkY=80;c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.backgroundScrollX=512+80;c.backgroundScrollY=3072+80;
 S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);auto &s=S9xRemasterState();uint16_t screen[64]={};RemasterFrame output;
 auto frame=[&](int mismatch=0){S9xRemasterBeginFrame(64,8,8,8);RemasterFrameTileInstance bg;bg.tileId=mismatch==1?RemasterTileContentId{id.hash+1,1,4}:id;bg.source=RemasterSourceType::Background;bg.sourceIndex=1;bg.tileNumber=mismatch==2?1:0;
  auto lower=bg;lower.sourceIndex=0;s.tileInstances={bg,lower};for(int k=0;k<64;k++){s.mainOwners[k]=0x02010000;s.mainInstanceIds[k]=1;s.mainTilePixels[k]=k;s.subOwners[k]=0x02000000;s.subInstanceIds[k]=2;s.subTilePixels[k]=k;}
  S9xRemasterEndFrame(screen,8,8,8,&output,&c);};
 frame();int cell=10*64+10;
 // Existing solved output stands for reviewed source placement, not a new solver
 // datum. Exercise complete map arrays and canonical local previews in EndFrame.
 s.runtimeDungeonHeightMap.offsets[cell]=40;s.runtimeDungeonBg1HeightMap.offsets[cell]=24;
 RemasterDungeonHeightMap::LocalHeightPreview local;local.cell=cell;for(int k=0;k<64;k++)local.height[k]=7-k%8;
 s.runtimeDungeonHeightMap.localHeightPreviews.push_back(local);s.runtimeDungeonBg1HeightMap.localHeightPreviews.push_back(local);
 auto verify=[&]{for(auto pixels:{&output.mainPixels,&output.subPixels})for(auto px:*pixels){const auto &i=output.tileInstances[px.instanceId-1];if(!(i.heightOffset==(i.sourceIndex?40:24) && i.hasPlacementHeight && i.placementHeight[px.tilePixel]==7-px.tilePixel%8))std::cerr<<"phase="<<int(c.constructionPhase)<<" sub="<<int(c.submodule)<<" room="<<c.roomIndex<<" bg="<<int(i.sourceIndex)<<" base="<<i.heightOffset<<" local="<<i.hasPlacementHeight<<" frozen="<<s.outgoingGeometry.valid<<'\n';need(i.heightOffset==(i.sourceIndex?40:24) && i.hasPlacementHeight && i.placementHeight[px.tilePixel]==7-px.tilePixel%8,"outgoing base/local geometry changed");}};
 auto epoch=s.runtimeDungeonHeightEpoch;
 // Earliest departure: room ID unchanged, collision construction/actor plane
 // already changed. Neither is allowed to rebase outgoing visible surfaces.
 c.submodule=2;c.constructionPhase=0;c.linkPlane=1;attr->fill(1);frame();verify();need(s.outgoingGeometry.valid,"not latched before room ID change");
 for(int phase=1;phase<8;phase++){c.constructionPhase=phase;frame();verify();}
 need(s.runtimeDungeonHeightEpoch==epoch,"partial source was re-solved");
 frame(true);need(output.tileInstances[output.mainPixels[0].instanceId-1].heightOffset==0,"changed producer reused source");
 frame(2);need(output.tileInstances[output.mainPixels[0].instanceId-1].heightOffset==0 && !output.tileInstances[output.mainPixels[0].instanceId-1].hasPlacementHeight,"same hash different placement word reused source");
  c.roomIndex=0x60;c.previousRoomIndex=c.roomIndex;c.hasDestinationOrigin=true;c.destinationOriginX=0;c.destinationOriginY=3072;c.scrollDirection=3;c.backgroundScrollX=592;c.constructionPhase=3;
  frame();verify();need(s.previousRuntimeDungeonHeightMap.offsets[cell]==40,"destination erased frozen source");
	// USA LoadNextRoom replaces A2 with CURRENT while source is still visible.
	c.previousRoomIndex=c.roomIndex;frame();verify();
	need(s.outgoingGeometry.valid,"loader scratch update discarded outgoing geometry");
	// Read-only completed attributes permit the incoming solve at phase2,
	// independently of the game's much later incremental collision completion.
	c.constructionPhase=2;c.hasCompletedTransitionAttributes=true;frame();verify();
	need(s.runtimeDungeonHeightMap.roomIndex==0x60,"destination not ready before scroll");
	c.hasCompletedTransitionAttributes=false;
  c.constructionPhase=8;c.attributeConstructionState=5;frame();verify();need(s.runtimeDungeonHeightMap.roomIndex==0x60,"destination solve absent");
 // Scroll finishes; viewport is entirely destination. Retire once, no revival
 // while lingering in handler2. Same asset identity must use destination base.
 c.backgroundScrollX=80;c.constructionPhase=14;frame();need(!s.outgoingGeometry.valid && s.outgoingGeometry.retired,"offscreen source retained");
 frame();need(!s.outgoingGeometry.valid && s.previousRuntimeDungeonHeightMap.roomIndex==0xffff,"retired source revived");
 c.submodule=0;frame();need(!s.outgoingGeometry.retired,"settle did not reset lifecycle");
 c.submodule=2;frame();need(s.outgoingGeometry.valid,"next legitimate departure not latched");
 c.romIdentity++;frame();need(!s.outgoingGeometry.valid && s.previousRuntimeDungeonHeightMap.roomIndex==0xffff,"ROM change reused source");
 c.submodule=0;frame();c.submodule=2;frame();need(s.outgoingGeometry.valid,"ROM fresh settled snapshot absent");
 c.roomIndex=0x75;c.previousRoomIndex=0xffff;frame();need(!s.outgoingGeometry.valid,"unrelated jump revived outgoing room");
 c.roomIndex=0x60;c.submodule=0;frame();
 // Defrost/reset/profile change invalidates retained source before application.
 c.submodule=2;frame();S9xRemasterInvalidateDungeonContinuity();need(!s.outgoingGeometry.valid,"defrost retained snapshot");
 c.submodule=0;frame();c.submodule=2;frame();S9xRemasterSetProfile(p);frame();need(!s.outgoingGeometry.valid,"profile edit reused old geometry");
 std::cout<<"PASS EndFrame settled/departure/plane+construction/mixed destination/offscreen unload/reset/profile producer-qualified both-BG local geometry\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
