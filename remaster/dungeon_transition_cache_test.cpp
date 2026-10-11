#include "remaster.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool v,const char *s){if(!v)throw std::runtime_error(s);}
int main(int argc,char **argv){try{
 // Deliberately identical rooms reproduce a consumed room-ID invalidation.
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 for(int p=0;p<2;p++)for(int y=10;y<15;y++)for(int x=10;x<15;x++)(*attr)[p*4096+y*64+x]=0;
 uint8_t indices[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assetGroups["dungeon_floor"].tileIds.push_back(id);profile.assets[id].hasHeight=true;
 S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x61;c.linkX=80;c.linkY=80;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.collisionAttributes=attr->data();
 auto &s=S9xRemasterState();uint16_t screen[64]={};
 auto frame=[&](){S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 frame();check(s.runtimeDungeonHeightMap.roomIndex==0x61,"initial solve");auto epoch=s.runtimeDungeonHeightEpoch;
 // Exercise application, not only retained cache identity. Global room-grid
 // scroll remains near3072, not the obsolete fixed boundary512.
 auto source=std::make_unique<RemasterDungeonHeightMap>();source->roomIndex=0x61;source->hasRoomGeometry=true;source->offsets.fill(50);
 auto pending=std::make_unique<RemasterDungeonHeightMap>();pending->roomIndex=0x60;pending->hasRoomGeometry=true;pending->offsets.fill(255);
 auto apply=[&](int scroll,int expected){
  RemasterFrame f;f.width=8;f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileId=id;f.tileInstances.push_back(i);
  for(auto &px:f.mainPixels){px.instanceId=1;px.tilePixel=0;}
  c.backgroundScrollX=scroll;c.backgroundScrollY=3072;
  S9xRemasterApplyDungeonHeightMap(f,c,*pending,source.get());
  check(f.tileInstances[f.mainPixels[0].instanceId-1].heightOffset==expected,"transition source base dropped/leaked");
 };
 c.roomIndex=0x60;c.previousRoomIndex=0x61;c.submodule=2;frame();
 for(int phase=0;phase<16;phase++){c.constructionPhase=phase;apply(512,50);apply(504,0);frame();check(s.runtimeDungeonHeightEpoch==epoch,"construction solved prematurely");}
 c.module=14;c.submodule=0;frame();check(s.runtimeDungeonHeightEpoch==epoch,"non-dungeon module published");c.module=7;c.submodule=2;
 source->producerHashes[0]=id.hash+1;apply(512,0);source->producerHashes[0]=id.hash;apply(512,50);
 // Source local fallback and actor plane survive along with the base.
 source->hasPlaneOffsets=true;source->planeOffsets[0].fill(24);source->fallbackDirections[0]=8;
 {RemasterFrame f;f.width=8;f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
  RemasterFrameTileInstance bg;bg.source=RemasterSourceType::Background;bg.sourceIndex=1;bg.tileId=id;
  RemasterFrameTileInstance actor;actor.source=RemasterSourceType::Object;actor.sourceIndex=0;
  f.tileInstances={bg,actor};for(auto &px:f.subPixels){px.instanceId=1;px.tilePixel=0;}
  for(auto &px:f.mainPixels){px.instanceId=2;px.tilePixel=0;}
  c.backgroundScrollX=512;c.backgroundScrollY=3072;
  S9xRemasterApplyDungeonHeightMap(f,c,*pending,source.get());
  check(f.tileInstances[0].heightOffset==50 && f.tileInstances[0].hasPlacementHeight && f.tileInstances[0].placementHeight[7]==7,"source fallback dropped");
  check(f.tileInstances[1].heightOffset==24,"departing actor support dropped");}
 check(s.runtimeDungeonHeightEpoch==epoch,"partial transition published");check(s.previousRuntimeDungeonHeightMap.roomIndex==0x61,"source lost");
 // Complete collision/room construction precedes viewport completion.
 c.submodule=2;c.constructionPhase=8;c.attributeConstructionState=4;frame();
 check(s.runtimeDungeonHeightEpoch==epoch,"partial collision builder published");
 c.attributeConstructionState=5;frame();
 check(s.runtimeDungeonHeightMap.roomIndex==0x60 && s.runtimeDungeonHeightEpoch==epoch+1,"complete scrolling destination not published");
 check(s.previousRuntimeDungeonHeightMap.roomIndex==0x61,"incoming solve destroyed outgoing map");
 // Settling exact complete inputs is a hit, not another geometry solve.
 c.attributeConstructionState=255;c.constructionPhase=0;
 c.submodule=0;frame();check(s.runtimeDungeonHeightMap.roomIndex==0x60,"destination never invalidated");check(s.runtimeDungeonHeightEpoch==epoch+1,"destination solve count");
 frame();check(s.runtimeDungeonHeightEpoch==epoch+1,"per-frame rebuild");
 c.linkPlane=1;frame();check(s.runtimeDungeonHeightEpoch==epoch+2,"plane not invalidated");
 check(s.runtimeDungeonHeightMap.requiredPlaneCells[0]==0 && s.runtimeDungeonHeightMap.requiredPlaneCells[1]>0,"planes merged");
 c.collisionMode=1;frame();c.collisionMode=0;frame();check(s.runtimeDungeonHeightMap.roomIndex==0x60,"collision-mode return");
 if(argc==3){
  RemasterProfile pin;std::vector<RemasterProfileDiagnostic> errors;
  check(S9xRemasterLoadProfile(argv[1],pin,errors),"sidecar profile");
  c.ppuMapSize={3,3};
  S9xRemasterWriteDungeonCaptureEvidence(argv[2],c,pin,&s.runtimeDungeonHeightMap,s.runtimeDungeonHeightKey,s.runtimeDungeonHeightEpoch,true);
  std::ifstream evidence(std::string(argv[2])+".dungeon-input.bin",std::ios::binary|std::ios::ate);
  check(evidence && evidence.tellg()==8+16384+8192+65536,"complete automatic sidecar");
 }
 std::cout<<"PASS 061->060 partial/settled identical inputs, retained source, cache reuse, independent planes\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
