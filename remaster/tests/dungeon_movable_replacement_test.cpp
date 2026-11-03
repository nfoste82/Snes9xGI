// An indexed art match alone cannot prove the replacement surface: secrets run
// before the replacement write. Old captures have no slots/LUT/secret evidence.
#include "../runtime/remaster.h"
#include "../runtime/profile_merge.h"
#include "../runtime/recording.h"
#include <cassert>
#include <iostream>
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
  slot.position^=0x2000;negative();slot.position^=0x2000;
  c.replacementEvidence.attributes[0x11e]=0x20;negative();c.replacementEvidence.attributes[0x11e]=0;
  c.replacementEvidence.slots[6]=slot;negative();c.replacementEvidence.slots[6]={};
  c.replacementEvidence.room++;negative();c.replacementEvidence.room--;
  c.verifiedAlttpRom=false;negative();c.verifiedAlttpRom=true;
  (*maps)[2*(20*64+20)]^=1;negative();(*maps)[2*(20*64+20)]^=1;
  (*attrs)[(bg?0:4096)+20*64+20]=0x76;negative();(*attrs)[(bg?0:4096)+20*64+20]=0x75;
  profile.obstacleGroups.push_back(profile.obstacleGroups[0]);profile.obstacleGroups.back().name="duplicate";negative();profile.obstacleGroups.pop_back();
  c.submodule=2;negative();c.submodule=0;
 }
 const auto bytes=S9xRemasterSerializeDungeonReplacementEvidence(c.replacementEvidence);
 RemasterDungeonReplacementEvidence restored;
 assert(S9xRemasterDeserializeDungeonReplacementEvidence(bytes,restored) && restored==c.replacementEvidence);
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
   assert(reference.slots[7].words==std::array<uint16_t,4>({0xd0e,0xd1e,0x4d0e,0x4d1e}));
   std::cout<<"deterministic reference room02f slot7 state1111 offset008c position1c68 LR4d1e attr="<<int(reference.attributes[0x11e])<<" PASS (not observed capture truth)\n";
  }
  auto missing=S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),false,0x2f);assert(!missing.available);
  rom[0xdb69+0x2f*2]=0;rom[0xdb69+0x2f*2+1]=0;assert(!S9xRemasterExtractDungeonReplacementEvidence(ram->data(),ram->size(),rom.data(),rom.size(),true,0x2f).available);
  std::cout<<"verified ROM bank81 secret pointer room02f exact pos1c68 secret0b PASS (not live slot evidence)\n";
 }
 std::cout<<"replacement ownership / both BG planes / flips / relief / authored zeros / secrets and malformed evidence PASS\n";
}
