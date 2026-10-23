#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 for(int p=0;p<2;p++)for(int y=10;y<15;y++)for(int x=10;x<15;x++)(*attr)[p*4096+y*64+x]=0;
 uint8_t pixels[64]={};RemasterTileContentId id={S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile profile;profile.upperFloorHeight=50;profile.assets[id].hasHeight=true;
 profile.assetGroups["dungeon_floor"].tileIds.push_back(id);
 S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);
 auto &s=S9xRemasterState();RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
 c.groundedEvidence=true;c.romIdentity=123;c.roomIndex=0x61;c.linkX=80;c.linkY=80;
 c.constructedMaps=maps->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();
 uint16_t screen[64]={};auto frame=[&]{S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 // Fully authored source geometry, no injected placement datum: three wall
 // tiles establish a signed24 rise on one gameplay plane.
 uint8_t wallPixels[64];std::fill_n(wallPixels,64,1);
 RemasterTileContentId wallId={S9xRemasterHashTile(4,wallPixels),1,4};
 profile.assets[wallId].hasHeight=true;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)profile.assets[wallId].height[y*8+x]=y;
 profile.assetGroups["wall_faces"].tileIds.push_back(wallId);
 for(int y=0;y<8;y++)(*gfx)[32+y*2]=255;
 S9xRemasterSetProfile(profile);attr->fill(1);
 for(int y:{12,18})for(int yy=y;yy<y+3;yy++)for(int x=8;x<18;x++)(*attr)[yy*64+x]=0;
 for(int bg=0;bg<2;bg++)for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*maps)[bg*8192+(y*64+x)*2]=1;
 c.linkY=18*8;c.stairCount=1;c.stairs[0].x=10;c.stairs[0].y=14;c.stairs[0].highIsNorth=false;
 frame();assert(s.runtimeDungeonHeightMap.planeOffsets[0][19*64+10]==24);
 assert(s.transitionSupport.valid && s.transitionSupport.base==24);
 c.roomIndex=0x60;c.previousRoomIndex=0x61;c.submodule=2;frame();
 // Construct independent flat destination. Entry inherits actual predecessor24,
 // not the destination's cold relative0 nor source authored wall artwork.
 maps->fill(0);attr->fill(1);c.stairCount=0;c.linkY=80;
 for(int p=0;p<2;p++)for(int y=10;y<15;y++)for(int x=10;x<15;x++)(*attr)[p*4096+y*64+x]=0;
 frame();c.submodule=0;frame();assert(s.runtimeDungeonHeightMap.entrySupportConsumed);
 assert(s.runtimeDungeonHeightMap.planeOffsets[0][11*64+10]==24);
 assert(s.runtimeDungeonBg1HeightMap.planeOffsets[0][11*64+10]==24);
 frame();c.roomIndex=0x61;c.previousRoomIndex=0x60;c.submodule=2;frame();
 attr->fill(1);for(int y:{12,18})for(int yy=y;yy<y+3;yy++)for(int x=8;x<18;x++)(*attr)[yy*64+x]=0;
 for(int bg=0;bg<2;bg++)for(int y=15;y<18;y++)for(int x=8;x<18;x++)(*maps)[bg*8192+(y*64+x)*2]=1;
 c.linkY=18*8;c.stairCount=1;c.stairs[0].x=10;c.stairs[0].y=14;frame();
 c.submodule=0;frame();assert(s.runtimeDungeonHeightMap.entrySupportConsumed);
 assert(s.runtimeDungeonHeightMap.planeOffsets[0][19*64+10]==24);
 assert(s.runtimeDungeonHeightMap.planeOffsets[0][13*64+10]==0);
 // In-room stairs preserve the completed entry datum, rather than starting a
 // new cross-room anchor or forcing the active plane/entry floor to relative0.
 c.submodule=16;frame();c.linkY=12*8;c.submodule=0;frame();
 assert(s.runtimeDungeonHeightMap.entrySupportConsumed && s.transitionSupport.base==0);
 maps->fill(0);attr->fill(1);c.stairCount=0;c.linkY=80;
 for(int p=0;p<2;p++)for(int y=10;y<15;y++)for(int x=10;x<15;x++)(*attr)[p*4096+y*64+x]=0;
 S9xRemasterInvalidateDungeonContinuity();c.roomIndex=0x61;c.previousRoomIndex=0xffff;
 frame();
 // Fixture source datum stands for a solved raised predecessor; producer must
 // read active-plane support, not visible highest support or entry position.
 s.runtimeDungeonHeightMap.planeOffsets[0].fill(37);
 frame();assert(s.transitionSupport.valid && s.transitionSupport.base==37);
 c.roomIndex=0x60;c.previousRoomIndex=0x61;c.submodule=2;frame();
 assert(s.transitionSupport.pending);c.constructionPhase=3;frame();
 c.submodule=0;c.previousRoomIndex=0x60;frame(); // loader updates A2 during scroll
 assert(s.runtimeDungeonHeightMap.entrySupportConsumed);
 assert(s.runtimeDungeonHeightMap.planeOffsets[0][11*64+10]==37);
 assert(s.runtimeDungeonBg1HeightMap.planeOffsets[0][11*64+10]==37);
 frame();frame();assert(s.runtimeDungeonHeightMap.entrySupportConsumed);
 const auto epoch=s.runtimeDungeonHeightEpoch;frame();assert(s.runtimeDungeonHeightEpoch==epoch);
 c.linkPlane=1;frame();assert(s.runtimeDungeonHeightMap.planeOffsets[1][11*64+10]==0);
 c.linkPlane=0;frame();assert(s.runtimeDungeonHeightMap.entrySupportConsumed && s.runtimeDungeonHeightMap.planeOffsets[0][11*64+10]==37);
 // Complete input/profile geometry changes must expire the destination anchor,
 // not issue a fresh state pin for an old transition.
 (*maps)[0]=1;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 (*maps)[0]=0;
 frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed); // no resurrected pin
 // Plane changes invalidate solve, but do not merge the underpass or lose the
 // anchored plane's graph datum. Unsupported independent plane stays relative.
 c.linkPlane=1;frame();assert(s.runtimeDungeonHeightMap.planeOffsets[1][11*64+10]==0);
 c.linkPlane=0;frame(); // stale geometry test above has already expired37
 S9xRemasterInvalidateDungeonContinuity();frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 assert(!s.transitionSupport.pending);
 // A room-ID jump without observed module07/02 is not a continuity edge.
 c.roomIndex=0x61;c.previousRoomIndex=0x60;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 // Stair/fall/warp handlers cannot be accepted as level doors.
 for(int handler:{7,8,14,16,17,21}) {
  frame();c.submodule=handler;frame();c.roomIndex^=1;c.previousRoomIndex=c.roomIndex^1;c.submodule=0;frame();
  assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 }
 frame();c.submodule=2;c.roomIndex^=1;c.previousRoomIndex=c.roomIndex^1;frame();
 S9xRemasterInvalidateDungeonContinuity();c.submodule=0;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 frame();c.submodule=2;c.roomIndex^=1;c.previousRoomIndex=c.roomIndex^1;frame();
 c.romIdentity++;c.submodule=0;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 frame();c.submodule=2;c.roomIndex^=1;c.previousRoomIndex=c.roomIndex^1;frame();
 S9xRemasterSetProfile(profile);c.submodule=0;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 frame();c.groundedEvidence=false;frame();c.submodule=2;c.roomIndex^=1;c.previousRoomIndex=c.roomIndex^1;frame();
 c.groundedEvidence=true;c.submodule=0;frame();assert(!s.runtimeDungeonHeightMap.entrySupportConsumed);
 std::cout<<"PASS live entry producer: observed completed door, active support37, both BGs, cache/plane, reset/defrost hook, ROM/profile, unsupported stairs/falls/warps and room-ID jump rejection\n";
}
