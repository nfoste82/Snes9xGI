#include "remaster.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 for(int y=10;y<15;y++)for(int x=10;x<15;x++)(*attr)[y*64+x]=0;
 uint8_t pixels[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].hasHeight=true;
 profile.assetGroups["dungeon_floor"].tileIds.push_back(id);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=c.groundedEvidence=true;
 c.roomIndex=7;c.linkX=80;c.linkY=80;c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();
 S9xRemasterSetLiveFramesEnabled(true);uint16_t screen[64]={};auto &s=S9xRemasterState();
 auto frame=[&]{S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 S9xRemasterSetProfile(profile);frame();auto epoch=s.runtimeDungeonHeightEpoch;auto key=s.dungeonGeometryProfileKey;
 // Same room/maps/arrays/groups, only semantic activation changes.
 for(int change=0;change<5;change++) {
  auto &a=profile.assets[id];
  if(change==0)a.tileRole=RemasterTileRole::Obstacle;
  if(change==1){a.tileRole=RemasterTileRole::Wall;a.hasTileSemantics=true;a.tileSemanticNormals={"east"};}
  if(change==2)a.tileSemanticNormals={"south"};
  if(change==3)a.tileSemanticContextDependent=true;
  if(change==4){a.tileRole=RemasterTileRole::Door;a.hasTileSemantics=false;a.hasDoorframeMask=true;a.doorframeMask=1;}
  S9xRemasterSetProfile(profile);frame();assert(s.dungeonGeometryProfileKey!=key && s.runtimeDungeonHeightEpoch>epoch);
  key=s.dungeonGeometryProfileKey;epoch=s.runtimeDungeonHeightEpoch;frame();assert(s.runtimeDungeonHeightEpoch==epoch);
  assert(s.activeProfile.assets[id].height==profile.assets[id].height);
 }
 S9xRemasterSetProfile(profile);frame();assert(s.runtimeDungeonHeightEpoch==epoch); // identical reload
 // Distinct signed local relief with identical low bytes must invalidate geometry.
 for(int height : {256,-256,512,-512}) {
  profile.assets[id].height.fill(static_cast<int16_t>(height));
  S9xRemasterSetProfile(profile);frame();assert(s.dungeonGeometryProfileKey!=key && s.runtimeDungeonHeightEpoch>epoch);
  key=s.dungeonGeometryProfileKey;epoch=s.runtimeDungeonHeightEpoch;
 }
 std::cout<<"PASS semantic-only activation/facing/context/door-mask rebuild; unchanged cache reuse\n";
}
