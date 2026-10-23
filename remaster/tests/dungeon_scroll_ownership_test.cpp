#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 RemasterDungeonFloorContext c;c.module=7;c.submodule=2;
 // All cardinal room boundaries, including nonzero global rows/columns.
 for(int direction=0;direction<4;direction++) {
  c.previousRoomIndex=0x61;c.roomIndex=direction==0?0x51:direction==1?0x71:direction==2?0x60:0x62;
  c.backgroundScrollX=direction==2?504:direction==3?1016:512;
  c.backgroundScrollY=direction==0?3064:direction==1?3576:3072;
  auto room=[&](int x,int y){return S9xRemasterDungeonRoomForScreenPosition(c,x,y);};
  require(room(0,0)==(direction==0 || direction==2?c.roomIndex:c.previousRoomIndex),"leading ownership");
  require(room(direction>=2?8:0,direction<2?8:0)==(direction==0 || direction==2?c.previousRoomIndex:c.roomIndex),"boundary ownership");
  for(int submodule:{0,1,8,16}) {c.submodule=submodule;require(room(0,0)==c.roomIndex && room(8,8)==c.roomIndex,"non-scroll leaked previous room");}
 c.submodule=2;
 }
 // USA Dungeon_LoadHeader computes destination origin from actual scroll
 // +/-256 rounded to512; ScrollRoom keeps16-bit offsets, never room-ID wraps.
 // Use deliberately non-grid room IDs, then16-bit wrap at0xffff in both axes.
 c.hasDestinationOrigin=true;c.roomIndex=0x50;c.previousRoomIndex=0x61;
 for(int direction=0;direction<4;direction++)for(int origin:{0,512,65024}) {
  c.scrollDirection=direction;c.destinationOriginX=c.destinationOriginY=origin;
  c.backgroundScrollX=origin;c.backgroundScrollY=origin;
  if(direction==0)c.backgroundScrollY=uint16_t(origin-8);
  if(direction==1)c.backgroundScrollY=uint16_t(origin+504);
  if(direction==2)c.backgroundScrollX=uint16_t(origin-8);
  if(direction==3)c.backgroundScrollX=uint16_t(origin+504);
  const auto first=S9xRemasterDungeonRoomForScreenPosition(c,0,0);
  const auto next=S9xRemasterDungeonRoomForScreenPosition(c,direction>=2?8:0,direction<2?8:0);
  require(first==(direction%2?c.roomIndex:c.previousRoomIndex),"loader-origin ownership/wrap first");
  require(next==(direction%2?c.previousRoomIndex:c.roomIndex),"loader-origin ownership/wrap boundary");
 }
 c.hasDestinationOrigin=false;
 uint8_t attrs[8192]={},maps[16384]={},gfx[65536]={};
 c.verifiedAlttpRom=c.indoors=true;c.collisionAttributes=attrs;c.constructedMaps=maps;c.graphics=gfx;
 for(int phase=0;phase<18;phase++)for(int builder:{0,1,2,3,4,5,255}) {
  c.constructionPhase=phase;c.attributeConstructionState=builder;
  require(S9xRemasterDungeonHeightInputsPublishable(c,0xffff)==(phase>=8 && phase<=15 && builder==5),"scroll publication attestation");
 }
 for(int handler:{8,16})for(int phase=0;phase<3;phase++) {
  c.submodule=handler;c.constructionPhase=phase;
  require(S9xRemasterDungeonHeightInputsPublishable(c,0xffff)==(phase<=1),"prior stair publication regressed");
 }
 // Actual application splits the SAME asset instance across two rooms;
 // distinct bases cannot bleed through repeated hash/flip identity.
 c.submodule=2;c.roomIndex=0x60;c.previousRoomIndex=0x61;c.backgroundScrollX=504;c.backgroundScrollY=3072;
 auto dst=std::make_unique<RemasterDungeonHeightMap>(),src=std::make_unique<RemasterDungeonHeightMap>();
 dst->roomIndex=0x60;src->roomIndex=0x61;dst->hasRoomGeometry=src->hasRoomGeometry=true;dst->offsets.fill(12);src->offsets.fill(40);
 RemasterFrame f;f.width=16;f.height=8;f.mainPixels.resize(128);f.subPixels.resize(128);
 RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;i.tileNumber=0;
 f.tileInstances.push_back(i);for(auto &p:f.mainPixels)p.instanceId=1;
 S9xRemasterApplyDungeonHeightMap(f,c,*dst,src.get());
 require(f.tileInstances[f.mainPixels[0].instanceId-1].heightOffset==12,"incoming base missing");
 require(f.tileInstances[f.mainPixels[8].instanceId-1].heightOffset==40,"outgoing base changed");
 // Same decoded hash, wrong uploaded tile word is NOT destination provenance.
 f.tileInstances.clear();f.tileInstances.push_back(i);for(auto &p:f.mainPixels)p.instanceId=1;
 maps[2*63]=1;
 S9xRemasterApplyDungeonHeightMap(f,c,*dst,src.get());
 require(f.tileInstances[f.mainPixels[0].instanceId-1].heightOffset==0,"not-yet-uploaded destination word borrowed a base");
 require(f.tileInstances[f.mainPixels[8].instanceId-1].heightOffset==40,"upload rejection destroyed outgoing base");
 // A overwritten/mirrored producer must reject not only its base but the
 // retained local fallback ramp. Same hash is insufficient after a flip.
 c.backgroundScrollX=512;src->producerHashes[0]=123;src->producerFlips[0]=0;src->fallbackDirections[0]=8;
 f.tileInstances.clear();i.tileId={123,1,4};i.hFlip=true;f.tileInstances.push_back(i);
 for(auto &p:f.mainPixels)p.instanceId=1;
 S9xRemasterApplyDungeonHeightMap(f,c,*dst,src.get());
 require(!f.tileInstances[f.mainPixels[0].instanceId-1].hasPlacementHeight &&
  f.tileInstances[f.mainPixels[0].instanceId-1].heightOffset==0,"wrong outgoing flip resurrected fallback geometry");
 std::cout<<"PASS four directions, stale previous IDs in both stair handlers, exact construction gate, two-room runtime bases\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
