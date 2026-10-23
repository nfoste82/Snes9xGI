// Profile handoff on Esc/resume must rebuild an invalidated equal-key solve.
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(0);
 uint8_t indices[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;auto &a=profile.assets[id];a.tileId=id;a.hasHeight=true;a.tileRole=RemasterTileRole::Floor;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.romIdentity=123;c.roomIndex=7;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.collisionAttributes=attr->data();c.bg1GraphicsBase=0;
 c.graphicsBase=0;
 c.backgroundScrollX=c.backgroundScrollY=80;c.linkX=c.linkY=80;
 S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);auto &s=S9xRemasterState();
 uint16_t screen[64]={};auto output=std::make_unique<RemasterFrame>();
 auto run=[&]{S9xRemasterBeginFrame(64,8,8,8);
  RemasterFrameTileInstance bg;bg.source=RemasterSourceType::Background;bg.tileId=id;
  s.tileInstances={bg,bg};s.tileInstances[1].sourceIndex=1;
  for(int k=0;k<64;k++){s.mainOwners[k]=0x02000000;s.mainInstanceIds[k]=1;s.mainTilePixels[k]=k;s.subOwners[k]=0x02010000;s.subInstanceIds[k]=2;s.subTilePixels[k]=k;}
  S9xRemasterEndFrame(screen,8,8,8,output.get(),&c);
 };
 run();run();check(s.runtimeDungeonHeightValid,"initial solve");
 const auto key=s.runtimeDungeonHeightKey;
 for(int repeat=0;repeat<3;repeat++) {
  const auto epoch=s.runtimeDungeonHeightEpoch;
  S9xRemasterSetProfile(profile);run();
  check(s.runtimeDungeonHeightValid && s.runtimeDungeonHeightKey==key,"equal-key resume discarded solve");
  check(s.runtimeDungeonHeightEpoch>epoch,"invalidated equal-key map not rebuilt");
  const auto rebuilt=s.runtimeDungeonHeightEpoch;run();check(s.runtimeDungeonHeightEpoch==rebuilt,"valid warm solve rebuilt");
 }
 // Scene-only settings revoke validity too. Do not repair only the macOS
 // resume caller or preserve invalid geometry after an actual input edit.
 s.runtimeDungeonHeightValid=false;const auto epoch=s.runtimeDungeonHeightEpoch;run();
 check(s.runtimeDungeonHeightValid && s.runtimeDungeonHeightEpoch>epoch,"explicit equal-key invalidation ignored");
 const auto oldKey=s.runtimeDungeonHeightKey;profile.assets[id].height[0]++;
 S9xRemasterSetProfile(profile);run();check(s.runtimeDungeonHeightValid && s.runtimeDungeonHeightKey!=oldKey,"actual geometry edits ignored");
 std::cout<<"PASS equal-key profile resume, repeated publication, warm reuse and geometry invalidation\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
