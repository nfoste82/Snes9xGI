// An indexed art match alone cannot prove the replacement surface: secrets run
// before the replacement write. Old captures have no slots/LUT/secret evidence.
#define ALTTP_TEST_CACHE_STAGES
#include "../runtime/remaster.h"
#include "../runtime/profile_merge.h"
#include "../runtime/recording.h"
#include "dungeon_geometry_cache_inventory.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(int argc,char **argv) {
 using C=RemasterDungeonFloorContext;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto tpl=std::make_unique<std::array<uint8_t,12876>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 RemasterProfile profile;
 for(int tile=1;tile<=4;tile++) {
  (*gfx)[tile*32]=tile;uint8_t pixels[64]={};
  for(int x=0;x<8;x++)pixels[x]=(tile>>(7-x))&1;
  RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
  auto &a=profile.assets[id];a.tileId=id;a.hasHeight=true;a.height.fill(tile);
  (*tpl)[0xe82+(tile-1)*2]=tile;
 }
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.constructedMaps=maps->data();c.graphics=gfx->data();
 c.dungeonTemplateData=tpl->data();c.collisionAttributes=attrs->data();
 for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
  int i=(20+y)*64+20+x;(*maps)[2*i]=1+x*2+y;(*attrs)[i]=0x75;
 }
 auto out=std::make_unique<std::array<C::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,profile,*out);
 for(const auto &p:*out)assert(!p.removableSupportPlanes && "missing replacement evidence must be unknown, not ordinary floor");
 // Canonical authored arrays below are synthetic test-only inputs.
 RemasterObstacleGroup group;group.name="pot";group.movable=true;
 for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
  const int i=(20+y)*64+20+x;
  group.members.push_back({{(*out)[i].tileHash,1,4},x,y,0,false,false,y==1});
 }
 profile.obstacleGroups.push_back(group);
 for(int tile:{0x10e,0x11e}) {
  (*gfx)[tile*32]=tile==0x10e?0x55:0xaa;uint8_t pixels[64]={};
  for(int x=0;x<8;x++)pixels[x]=((*gfx)[tile*32]>>(7-x))&1;
  RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[id];
  a.tileId=id;a.tileRole=RemasterTileRole::Floor;a.hasHeight=true; // valid authored zero
 }
 c.replacementEvidence.available=true;c.replacementEvidence.room=c.roomIndex;
 auto &slot=c.replacementEvidence.slots[5];slot.state=0x1111;slot.position=2*(20*64+20);
 slot.ordinaryPotDeclaration=true;
 slot.words={0x0d0e,0x0d1e,0x4d0e,0x4d1e};
 auto run=[&](bool expected) {
  S9xRemasterDungeonPlacements(c,profile,*out);
  for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
   const auto &p=(*out)[(20+y)*64+20+x];
   assert(bool(p.structuralReplacement)==expected);
   assert(p.removableSupportPlanes==(expected?1<<((slot.position>>13)&1):0));
   if(expected)assert(p.hasHeight && p.height[0]>=1 && p.height[0]<=4); // relief never flattened on accepted identity
   if(expected){assert(p.structuralReplacement->height[0]==0);assert(p.structuralReplacement->hflip==bool(x));}
  }
 };
 run(true);
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++) {
  c.placementBackgroundIndex=bg;slot.position=2*(20*64+20)|(bg?0:0x2000);
  attrs->fill(1);
  for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
   int i=(20+y)*64+20+x,w=1+(flip&1?1-x:x)*2+(flip&2?1-y:y)+(flip<<14);
   (*maps)[2*i]=w;(*maps)[2*i+1]=w>>8;(*attrs)[(bg?0:4096)+i]=0x75;
  }
  run(true);
  auto negative=[&]{run(false);};
  profile.obstacleGroups[0].movable=false;negative();profile.obstacleGroups[0].movable=true;
  c.replacementEvidence.secrets={{slot.position,0x88}};negative();
  c.replacementEvidence.secrets[0].value=0x80;negative();
  c.replacementEvidence.secrets[0].value=0x0b;run(true);c.replacementEvidence.secrets.clear();
  slot.state=0;negative();slot.state=0x2020;negative();slot.state=0x1111;
  slot.ordinaryPotDeclaration=false;negative();slot.ordinaryPotDeclaration=true;
  slot.position^=0x2000;negative();slot.position^=0x2000;
  c.replacementEvidence.attributes[0x11e]=0x20;negative();c.replacementEvidence.attributes[0x11e]=0;
  c.replacementEvidence.slots[6]=slot;negative();c.replacementEvidence.slots[6]={};
  c.replacementEvidence.room++;negative();c.replacementEvidence.room--;
  c.verifiedAlttpRom=false;negative();c.verifiedAlttpRom=true;
  (*maps)[2*(20*64+20)]^=1;negative();(*maps)[2*(20*64+20)]^=1;
  (*attrs)[(bg?0:4096)+20*64+20]=0x76;negative();(*attrs)[(bg?0:4096)+20*64+20]=0x75;
  profile.obstacleGroups.push_back(profile.obstacleGroups[0]);profile.obstacleGroups.back().name="duplicate";negative();profile.obstacleGroups.pop_back();
  c.submodule=2;negative();c.submodule=0;
  for(auto &a:profile.assets)if(a.second.tileRole==RemasterTileRole::Floor) {
   const auto saved=a.second;
   a.second.hasHeight=false;negative();a.second=saved;
   a.second.tileRole=RemasterTileRole::Other;negative();a.second=saved;
   a.second.height.fill(-8);negative();a.second=saved; // nonzero underlay unsupported, not silently flattened
   profile.tileContexts[a.first]["alternative"]={};negative();profile.tileContexts.erase(a.first);
   a.second.hasNormals=false;run(true);a.second=saved; // normals not invented or needed for authored floor zero
  }
 }
 const auto bytes=S9xRemasterSerializeDungeonReplacementEvidence(c.replacementEvidence);
 RemasterDungeonReplacementEvidence restored;
 assert(S9xRemasterDeserializeDungeonReplacementEvidence(bytes,restored) && restored==c.replacementEvidence);
 assert(S9xRemasterReplacementSidecarMatches(bytes,c.replacementEvidence));
 assert(!S9xRemasterReplacementSidecarMatches(bytes,{}));
 restored.room++;assert(!S9xRemasterReplacementSidecarMatches(bytes,restored));
 auto malformed=bytes;malformed.push_back(0);assert(!S9xRemasterDeserializeDungeonReplacementEvidence(malformed,restored) && !restored.available);
 malformed=bytes;malformed[10]=255;assert(!S9xRemasterDeserializeDungeonReplacementEvidence(malformed,restored));
 std::string text;std::vector<RemasterProfileDiagnostic> errors;
 profile.gameTitle="replacement test";profile.romSha256=std::string(64,'0');
 if(!S9xRemasterSerializeProfile(profile,text,errors)){for(const auto &e:errors)std::cerr<<e.message<<'\n';return 1;}assert(text.find("schema_version = 40")!=std::string::npos);
 RemasterProfile parsed;assert(S9xRemasterParseProfile(text,parsed,errors) && parsed.obstacleGroups[0].movable);
 auto bad=text;bad.replace(bad.find("movable = true"),14,"movable = 1");assert(!S9xRemasterParseProfile(bad,parsed,errors));
 auto legacy=text;legacy.replace(legacy.find("schema_version = 40"),19,"schema_version = 39");assert(!S9xRemasterParseProfile(legacy,parsed,errors));
 auto before=profile;before.obstacleGroups[0].movable=false;
 std::string undo;assert(S9xRemasterSerializeProfile(before,undo,errors));
 assert(S9xRemasterParseProfile(undo,parsed,errors) && !parsed.obstacleGroups[0].movable);
 assert(S9xRemasterParseProfile(text,parsed,errors) && parsed.obstacleGroups[0].movable);
 auto disk=before;disk.reflectanceBoost=2;
 RemasterProfileMergeResult merge;
 assert(S9xRemasterMergeProfiles(before,profile,disk,{},false,merge,errors));
 assert(merge.saved.obstacleGroups[0].movable && merge.saved.reflectanceBoost==2 && merge.conflicts.empty());
 const auto records=RemasterProfileMerge::Split(text);
 bool saw=false;for(const auto &r:records)if(r.first.find("5:")==0){saw=true;assert(r.second.find("movable = true")!=std::string::npos);}assert(saw);
 // Completed capture owns values, never future live slot/LUT/secret updates.
 auto pin=S9xRemasterPinDungeonCaptureEvidence(&c,profile,nullptr,0,0,true);
 c.replacementEvidence.slots[5].words[0]++;
 assert(pin->context.replacementEvidence!=c.replacementEvidence);
 c.replacementEvidence=pin->context.replacementEvidence;
 auto packet=std::make_unique<RemasterFrame>();packet->width=packet->height=1;
 packet->originalRgb555.resize(1);packet->mainPixels.resize(1);packet->subPixels.resize(1);
 packet->profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 packet->area.id=c.roomIndex;packet->replacementEvidence=c.replacementEvidence;
 std::vector<uint8_t> serialized;assert(S9xSerializeRemasterFrame(*packet,serialized));
 auto replay=std::make_unique<RemasterFrame>();assert(S9xDeserializeRemasterFrame(serialized,*replay));
 assert(replay->replacementEvidence==c.replacementEvidence);
 serialized.push_back(0);assert(!S9xDeserializeRemasterFrame(serialized,*replay));serialized.pop_back();
 packet->area.id++;assert(!S9xSerializeRemasterFrame(*packet,serialized));packet->area.id--;
 packet->replacementEvidence={};assert(S9xSerializeRemasterFrame(*packet,serialized));assert(S9xDeserializeRemasterFrame(serialized,*replay) && !replay->replacementEvidence.available);
 auto recorded=RemasterRecording::Evidence(&c,nullptr,nullptr,nullptr,nullptr,true,0,0,0,nullptr,nullptr);
 const char marker[]="DRE1";const auto tag=std::search(recorded.begin(),recorded.end(),marker,marker+4);assert(tag!=recorded.end());
 const auto owned=recorded;c.replacementEvidence.attributes[0]=99;assert(recorded==owned);
 // Actual production EndFrame: cold/warm -> pickup -> repeat -> independent
 // forced uncached, comparing complete placements/maps and serialized packet.
 profile.romSha256=packet->profileRomSha256;profile.upperFloorHeight=50;
 c.placementBackgroundIndex=1;c.romIdentity=UINT64_C(0x66871d66be19ad2c);
 c.linkX=18*8;c.linkY=18*8;c.backgroundScrollX=c.backgroundScrollY=20*8;
 slot.position=2*(20*64+20);attrs->fill(1);maps->fill(0);
 for(int bg=0;bg<2;bg++)for(int y=16;y<26;y++)for(int x=16;x<26;x++) {
  const int i=y*64+x,at=(bg?0:8192)+2*i;
  (*maps)[at]=0x0e;(*maps)[at+1]=0x0d;(*attrs)[(bg?0:4096)+i]=0;
 }
 auto pot=[&](bool present) {
  for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
   const int i=(20+y)*64+20+x,w=present?1+x*2+y:slot.words[x*2+y];
   (*maps)[2*i]=w;(*maps)[2*i+1]=w>>8;(*attrs)[i]=present?0x75:0;
  }
 };
 auto &state=S9xRemasterState();S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);
 auto frame=std::make_unique<RemasterFrame>();uint16_t screen[256]={};RemasterAreaContext area;area.id=c.roomIndex;
 auto endFrame=[&] {
  S9xRemasterDungeonCacheStages()={};S9xRemasterBeginFrame(256,16,16,16);
  for(int x=0;x<2;x++)for(int y=0;y<2;y++) {
   const int cell=(20+y)*64+20+x,w=(*maps)[2*cell]|(*maps)[2*cell+1]<<8;
   uint8_t pixels[64]={};for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)for(int p=0;p<4;p++)pixels[yy*8+xx]|=(((*gfx)[uint16_t((w&1023)*32+p/2*16+yy*2+p%2)]>>(7-xx))&1)<<p;
   RemasterFrameTileInstance in;in.source=RemasterSourceType::Background;in.sourceIndex=1;in.tileNumber=w&1023;in.palette=(w>>10)&7;
   in.hFlip=w&0x4000;in.vFlip=w&0x8000;in.tileId={S9xRemasterHashTile(4,pixels),1,4};state.tileInstances.push_back(in);
   auto &observed=state.observedTiles[in.tileId.hash];observed.hash=in.tileId.hash;observed.bitDepth=4;std::copy_n(pixels,64,observed.indices);
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++) {
    const int at=(y*8+yy)*16+x*8+xx;
    state.mainOwners[at]=state.subOwners[at]=S9xRemasterOwner(RemasterSourceType::Background,1,0);
    state.mainInstanceIds[at]=state.subInstanceIds[at]=uint32_t(state.tileInstances.size());
    state.mainTilePixels[at]=state.subTilePixels[at]=(in.vFlip?7-yy:yy)*8+(in.hFlip?7-xx:xx);
   }
  }
  S9xRemasterEndFrame(screen,16,16,16,frame.get(),&c,nullptr,&area);
  assert(state.runtimeDungeonHeightValid && frame->replacementEvidence.available);
 };
 auto parity=[&] {
  const auto inventory=dungeonMapInventory(state.runtimeDungeonHeightMap)+dungeonMapInventory(state.runtimeDungeonBg1HeightMap);
  auto bg1=std::make_unique<std::array<C::Placement,4096>>(*state.dungeonRetainedBg1);
  auto bg2=std::make_unique<std::array<C::Placement,4096>>(*state.dungeonRetainedBg2);
  std::vector<uint8_t> warm,cold;assert(S9xSerializeRemasterFrame(*frame,warm));
  state.dungeonClassificationInputs->valid=false;state.dungeonSnapshotValid=false;state.runtimeDungeonHeightValid=false;state.runtimeDungeonHeightKey=0;
  endFrame();assert(S9xRemasterDungeonCacheStages().classifications>=2);
  assert(inventory==dungeonMapInventory(state.runtimeDungeonHeightMap)+dungeonMapInventory(state.runtimeDungeonBg1HeightMap));
  for(int i=0;i<4096;i++)for(int bg=0;bg<2;bg++) {
   const auto &a=bg?(*bg2)[i]:(*bg1)[i],&b=bg?(*state.dungeonRetainedBg2)[i]:(*state.dungeonRetainedBg1)[i];
   assert(a.tileHash==b.tileHash && a.sameGeometry(b));
  }
  assert(S9xSerializeRemasterFrame(*frame,cold) && warm==cold);
 };
 pot(true);endFrame();assert((*state.dungeonRetainedBg2)[20*64+20].structuralReplacement);
 auto potential=state.runtimeDungeonHeightMap.planeOffsets;const int beforeBase=state.runtimeDungeonHeightMap.offsets[20*64+20];
 assert(beforeBase!=255);parity();endFrame();assert(!S9xRemasterDungeonCacheStages().classifications && !S9xRemasterDungeonCacheStages().solves);
 pot(false);endFrame();assert(!(*state.dungeonRetainedBg2)[20*64+20].structuralReplacement);
 assert(state.runtimeDungeonHeightMap.planeOffsets==potential);
 std::cout<<"production ordinary pot base before="<<beforeBase<<" after="<<state.runtimeDungeonHeightMap.offsets[20*64+20]<<" pickup classifications="<<S9xRemasterDungeonCacheStages().classifications<<" solves="<<S9xRemasterDungeonCacheStages().solves<<'\n';
 parity();pot(true);endFrame();parity();
 // Both signed placement datums are independently consumed by the actual
 // structural solver. Local obstacle relief stays untouched.
 for(int base:{-24,24}) {
  c.previousRoomIndex=6;c.entrySupport={18*64+18,base,6,1,2,0};
  auto classified=std::make_unique<std::array<C::Placement,4096>>();S9xRemasterDungeonPlacements(c,profile,*classified);
  auto signedContext=c;signedContext.placements=classified->data();
  signedContext.entrySupport.destinationState=S9xRemasterDungeonEntryStateKey(signedContext,50);
  auto signedMap=std::make_unique<RemasterDungeonHeightMap>();assert(S9xRemasterBuildDungeonHeightMap(signedContext,50,*signedMap));
  assert(signedMap->entrySupportConsumed && signedMap->offsets[20*64+20]==base);
 }
 c.entrySupport={};c.previousRoomIndex=0xffff;
 // Publication and lifecycle invalidation must not preserve the owned visual
 // obstacle on a replacement hit. The completed packet is independently owned.
 const auto completed=frame->replacementEvidence;slot.position^=0x2000;
 endFrame();assert(!(*state.dungeonRetainedBg2)[20*64+20].structuralReplacement);
 assert(completed.slots[5].position!=frame->replacementEvidence.slots[5].position);
 slot.position^=0x2000;endFrame();parity();
 profile.obstacleGroups[0].movable=false;S9xRemasterSetProfile(profile);endFrame();assert(!(*state.dungeonRetainedBg2)[20*64+20].structuralReplacement);
 profile.obstacleGroups[0].movable=true;S9xRemasterSetProfile(profile);endFrame();parity();
 c.submodule=2;endFrame();c.submodule=0;endFrame();parity();
 (*attrs)[20*64+20]=1;endFrame();assert(!(*state.dungeonRetainedBg2)[20*64+20].structuralReplacement);parity();
 if(argc>=2) {
  std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
  auto ram=std::make_unique<std::array<uint8_t,0x20000>>();
  auto actual=S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),true,0x2f);
  assert(actual.available && actual.secrets.size()==8);
  assert(actual.secrets[7].position==0x1c68 && actual.secrets[7].value==0x0b);
  if(argc>=3) {
   std::ifstream source(argv[2],std::ios::binary);std::vector<uint8_t> runtime{std::istreambuf_iterator<char>(source),{}};
   assert(runtime.size()>=8+131072 && !std::memcmp(runtime.data(),"ALTPRT1\0",8));
   auto reference=S9xRemasterExtractDungeonReplacementEvidence(runtime.data()+8,131072,rom.data(),rom.size(),true,0x2f);
   assert(reference.available && reference.slots[7].state==0x1111 && reference.slots[7].position==0x1c68 && reference.slots[7].objectOffset==0x8c && reference.slots[7].ordinaryPotDeclaration);
   assert((reference.slots[7].words==std::array<uint16_t,4>({0xd0e,0xd1e,0x4d0e,0x4d1e})));
   std::cout<<"deterministic reference room02f slot7 state1111 offset008c position1c68 LR4d1e attr="<<int(reference.attributes[0x11e])<<" PASS (not observed capture truth)\n";
  }
  auto missing=S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),false,0x2f);assert(!missing.available);
  size_t available=0,unknown=0;
  for(uint16_t room=0;room<320;room++) {
   auto e=S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),true,room);
   if(e.available)available++;else unknown++;
  }
  std::cout<<"ROM bounded secret decode corpus rooms=320 available="<<available<<" explicit_unknown="<<unknown<<" (zero synthetic RAM; not placed geometry audit)\n";
  rom[0xdb69+0x2f*2]=0;rom[0xdb69+0x2f*2+1]=0;assert(!S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),true,0x2f).available);
  std::cout<<"verified ROM bank81 secret pointer room02f exact pos1c68 secret0b PASS (not live slot evidence)\n";
 }
 std::cout<<"replacement ownership / both BG planes / flips / relief / authored zeros / secrets and malformed evidence PASS\n";
}
