#include "../runtime/remaster.h"
#include "../runtime/hidden_emitter_samples.h"
#include "../runtime/profile_merge.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = REMASTER_OWNER_UNSUPPORTED;
int main()
{
 RemasterProfile p; p.schemaVersion=35;p.gameTitle="Offscreen quality";p.romSha256=std::string(64,'a');
 std::vector<RemasterProfileDiagnostic> errors; std::string text;
 // Old profiles have an independent offscreen default of one, regardless of
 // global or per-area visible quality. Explicit exact zero remains meaningful.
 assert(p.offscreenRepresentativeEmitters==1);
 assert(S9xRemasterParseProfile("schema_version = 16\n[game]\ntitle = \"Legacy\"\nrom_sha256 = \""+std::string(64,'a')+"\"\n[lighting_space]\nrepresentative_emitters = 8\n",p,errors));
 assert(p.offscreenRepresentativeEmitters==1);
 assert(S9xRemasterAreaSceneSettings(p,{97,false}).offscreenRepresentativeEmitters==1);
 p.areaSceneSettings["room_061"].representativeEmitters=0;
 assert(S9xRemasterAreaSceneSettings(p,{97,false}).offscreenRepresentativeEmitters==1);
 // Exhaustive independent visible/default/area inheritance, including valid exact zeros.
 for(int visible=0;visible<=8;visible++) for(int areaVisible=-1;areaVisible<=8;areaVisible++)
 for(int hidden=-1;hidden<=8;hidden++) for(int areaHidden=-1;areaHidden<=8;areaHidden++) {
  p.representativeEmitters=visible;p.offscreenRepresentativeEmitters=hidden;
  auto &a=p.areaSceneSettings["room_061"];a.representativeEmitters=areaVisible;a.offscreenRepresentativeEmitters=areaHidden;
  const int effectiveVisible=areaVisible<0?visible:areaVisible;
   const int expected=areaHidden>=0?areaHidden:hidden>=0?hidden:1;
  auto resolved=S9xRemasterAreaSceneSettings(p,{97,false});
  assert(resolved.representativeEmitters==effectiveVisible && resolved.offscreenRepresentativeEmitters==expected);
 }
 for(int value=-1;value<=8;value++) {
  p.offscreenRepresentativeEmitters=value;p.areaSceneSettings["room_061"].offscreenRepresentativeEmitters=value;
  assert(S9xRemasterSerializeProfile(p,text,errors));RemasterProfile q;
  assert(S9xRemasterParseProfile(text,q,errors));
   assert(q.offscreenRepresentativeEmitters==(value<0?1:value) && q.areaSceneSettings==p.areaSceneSettings);
  auto settings=S9xRemasterGetSceneSettings(p);settings.offscreenRepresentativeEmitters=(value+2)%9;
  assert(!S9xRemasterSceneSettingsEqual(settings,S9xRemasterGetSceneSettings(p)));
  S9xRemasterApplySceneSettings(q,settings);assert(q.offscreenRepresentativeEmitters==settings.offscreenRepresentativeEmitters);
 }
 for(const char *value:{"-2","9","1.5","nan","true"}) {
  assert(!S9xRemasterParseProfile(text+"\n[area_scene_settings.room_062]\noffscreen_representative_emitters = "+value+"\n",p,errors));
  auto invalid=text;auto start=invalid.find("offscreen_representative_emitters = ");
  invalid.replace(start,invalid.find('\n',start)-start,std::string("offscreen_representative_emitters = ")+value);
  assert(!S9xRemasterParseProfile(invalid,p,errors));
 }
 {auto invalid=text;invalid.replace(invalid.find("schema_version = 36"),19,"schema_version = 35");assert(!S9xRemasterParseProfile(invalid,p,errors));}
 assert(S9xRemasterParseProfile(text,p,errors));
 assert(!S9xRemasterParseProfile(text+"\n[area_scene_settings.room_062]\noffscreen_representative_emitters = 1\noffscreen_representative_emitters = 2\n",p,errors));
 assert(S9xRemasterParseProfile(text,p,errors));
 {auto invalid=p;invalid.offscreenRepresentativeEmitters=9;std::string output;assert(!S9xRemasterSerializeProfile(invalid,output,errors));}
 RemasterFrame f;f.width=f.height=8;f.originalRgb555.resize(64);f.mainPixels.resize(64);f.subPixels.resize(64);
 f.representativeEmitters=0;f.offscreenRepresentativeEmitters=1;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++) {RemasterFrameHiddenEmitter e;e.x=x+0.5f;e.y=y+0.5f;e.emission={{255,128,64,25}};f.hiddenEmitters.push_back(e);}
 assert(S9xRemasterHiddenDirectSamples(f,false,true).size()==1);
 f.offscreenRepresentativeEmitters=0;assert(S9xRemasterHiddenDirectSamples(f,false,true).size()==64);
 assert(S9xRemasterHiddenDirectSamples(f,false,true)[0].radiance[3]==0);
 f.offscreenRepresentativeEmitters=-1;f.representativeEmitters=2;
 assert(S9xRemasterHiddenDirectSamples(f,false,true).size()==2);
 f.offscreenRepresentativeEmitters=1;
 assert(S9xRemasterHiddenDirectSamples(f,false,false).size()==64); // diagnostic exact mode
 // Duplicate XY/overlapping owners must retain the existing exact fallback.
 f.hiddenEmitters.push_back(f.hiddenEmitters[0]);
 assert(S9xRemasterHiddenDirectSamples(f,false,true).size()==65);
 f.hiddenEmitters.pop_back();
 for(int value=-1;value<=8;value++) {
  f.offscreenRepresentativeEmitters=value;std::vector<uint8_t> bytes;RemasterFrame replay;
  assert(S9xSerializeRemasterFrame(f,bytes)&&S9xDeserializeRemasterFrame(bytes,replay));
  assert(S9xRemasterEffectiveOffscreenRepresentatives(replay)==(value<0?2:value));
  if(value>=0) {
   auto malformed=bytes;malformed.back()=9;assert(!S9xDeserializeRemasterFrame(malformed,replay));
   malformed=bytes;malformed.pop_back();assert(!S9xDeserializeRemasterFrame(malformed,replay));
   malformed=bytes;malformed.insert(malformed.end(),bytes.end()-5,bytes.end());assert(!S9xDeserializeRemasterFrame(malformed,replay));
   bytes.resize(bytes.size()-5);assert(S9xDeserializeRemasterFrame(bytes,replay));
   assert(S9xRemasterEffectiveOffscreenRepresentatives(replay)==2);
  }
 }
 {std::vector<uint8_t> bytes;f.offscreenRepresentativeEmitters=9;assert(!S9xSerializeRemasterFrame(f,bytes));f.offscreenRepresentativeEmitters=1;f.schemaVersion=27;assert(!S9xSerializeRemasterFrame(f,bytes));f.schemaVersion=REMASTER_FRAME_SCHEMA_VERSION;}
 p.offscreenRepresentativeEmitters=3;p.areaSceneSettings["room_061"].offscreenRepresentativeEmitters=1;
 f.area={97,false};S9xRemasterApplyProfileToFrame(p,f);assert(f.offscreenRepresentativeEmitters==1);
 S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);S9xRemasterBeginFrame(1,1,1,1);
 uint16_t pixel=0;RemasterAreaContext area{97,false};S9xRemasterEndFrame(&pixel,1,1,1,nullptr,nullptr,nullptr,&area);
 assert(S9xRemasterCompletedFrame()->offscreenRepresentativeEmitters==1);
 p.offscreenRepresentativeEmitters=-1;p.representativeEmitters=0;p.areaSceneSettings["room_061"].representativeEmitters=0;
 p.areaSceneSettings["room_061"].offscreenRepresentativeEmitters=3;
 S9xRemasterSetSceneSettings(S9xRemasterGetSceneSettings(p));S9xRemasterSetAreaSceneSettings(p.areaSceneSettings);
 S9xRemasterBeginFrame(1,1,1,1);S9xRemasterEndFrame(&pixel,1,1,1,nullptr,nullptr,nullptr,&area);
 assert(S9xRemasterCompletedFrame()->representativeEmitters==0&&S9xRemasterCompletedFrame()->offscreenRepresentativeEmitters==3);
 p.areaSceneSettings["room_061"].offscreenRepresentativeEmitters=-1;
 S9xRemasterSetAreaSceneSettings(p.areaSceneSettings);S9xRemasterBeginFrame(1,1,1,1);
  S9xRemasterEndFrame(&pixel,1,1,1,nullptr,nullptr,nullptr,&area);assert(S9xRemasterCompletedFrame()->offscreenRepresentativeEmitters==1);
 auto base=p,local=p,disk=p;local.offscreenRepresentativeEmitters=2;disk.areaSceneSettings["overworld"].offscreenRepresentativeEmitters=0;
 RemasterProfileMergeResult merged;assert(S9xRemasterMergeProfiles(base,local,disk,{},false,merged,errors));
 assert(merged.conflicts.empty()&&merged.saved.offscreenRepresentativeEmitters==2&&merged.saved.areaSceneSettings.at("overworld").offscreenRepresentativeEmitters==0);
 // Area sections are indivisible merge records, not fieldwise merges.
 local=base;disk=base;local.areaSceneSettings["room_061"].offscreenRepresentativeEmitters=2;
 disk.areaSceneSettings["room_061"].reflectanceBoost=4;
 assert(S9xRemasterMergeProfiles(base,local,disk,{},false,merged,errors)&&merged.conflicts.size()==1);
 // Native editing/Undo uses the same cleanup helper. Returning a newly created
 // default-only record to inheritance must restore a clean map, not an empty section.
 RemasterProfile edited;
 S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",2,false);
 assert(edited.areaSceneSettings.size()==1);
 S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",-1,false);
 assert(edited.areaSceneSettings.empty());
 S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",2,false); // undo
 S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",-1,false); // redo
 assert(edited.areaSceneSettings.empty());
 for(int channel=0;channel<3;channel++) {
  auto &a=edited.areaSceneSettings["room_061"];
  if(channel==0)a.originalSceneContribution=0;
  if(channel==1)a.representativeEmitters=0;
  if(channel==2)a.reflectanceBoost=0;
  const auto before=a;
  S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",2,false);
  S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",-1,false);
  assert(edited.areaSceneSettings.at("room_061")==before);edited.areaSceneSettings.clear();
 }
 S9xRemasterSetAreaOffscreenRepresentatives(edited,"room_061",-1,true);
 assert(edited.areaSceneSettings.size()==1); // intentional preexisting default record
 // Actual historical layout, not only current-schema bytes with OER1 removed.
 RemasterFrame historical;historical.width=historical.height=1;
 historical.originalRgb555={0};historical.mainPixels.resize(1);historical.subPixels.resize(1);
 historical.representativeEmitters=3;std::vector<uint8_t> legacy;RemasterFrame old;
 assert(S9xSerializeRemasterFrame(historical,legacy));
 // With no metadata/instances the only v30+ payload difference is the final
 // empty palette-artwork count. Remove it and pin the historical v28 header.
 assert(legacy.size()>=4);legacy.resize(legacy.size()-4);legacy[8]=28;legacy[9]=legacy[10]=legacy[11]=0;
 assert(S9xDeserializeRemasterFrame(legacy,old));
 assert(old.schemaVersion==28&&old.offscreenRepresentativeEmitters==-1&&S9xRemasterEffectiveOffscreenRepresentatives(old)==3);
 std::cout<<"PASS offscreen representatives inheritance, exact/approximate samples, capture strictness, production publication and merge\n";
}
