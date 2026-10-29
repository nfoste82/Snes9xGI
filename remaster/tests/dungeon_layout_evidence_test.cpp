// Fixture-free live evidence, ownership and production capture parity.
#include "../runtime/remaster.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
static std::string Read(const std::string &p) {std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main()
{
 std::array<uint8_t,0x618> ram={};
 // Explicit, unique captured bytes across pages: a wrong-page read cannot
 // alias the expected value (in particular $40e and $60e must differ).
 ram[0xa6]=0x21;ram[0xa7]=0x22;ram[0xa8]=0x23;ram[0xa9]=0x24;ram[0xaa]=0x25;
 ram[0x452]=0x26;ram[0x453]=0x27;ram[0x126]=0x28;
 ram[0x40e]=0x01;ram[0x40f]=0x81;
 ram[0xfc]=0x02;ram[0xfd]=0x82;
 ram[0xe2]=0x03;ram[0xe3]=0x83;ram[0xe8]=0x04;ram[0xe9]=0x84;
 ram[0x600]=0x05;ram[0x601]=0x85;ram[0x602]=0x06;ram[0x603]=0x86;
 ram[0x604]=0x07;ram[0x605]=0x87;ram[0x606]=0x08;ram[0x607]=0x88;
 ram[0x608]=0x09;ram[0x609]=0x89;ram[0x60a]=0x0a;ram[0x60b]=0x8a;
 ram[0x60c]=0x0b;ram[0x60d]=0x8b;ram[0x60e]=0x0c;ram[0x60f]=0x8c;
 ram[0x610]=0x0d;ram[0x611]=0x8d;ram[0x612]=0x0e;ram[0x613]=0x8e;
 ram[0x614]=0x0f;ram[0x615]=0x8f;ram[0x616]=0x10;ram[0x617]=0x90;
 auto e=S9xRemasterExtractDungeonLayoutEvidence(ram.data(),ram.size(),true,true);
 assert(e.available);
 assert((e.quadrant==std::array<uint8_t,5>{{33,34,35,36,37}}));
 assert(e.loadedLayout==33025&&e.overrides==33282);
 assert((e.blastwall==std::array<uint8_t,2>{{38,39}}));
 assert((e.bounds==std::array<uint16_t,8>{{34053,34310,34567,34824,35081,35338,35595,35852}}));
 assert((e.targets==std::array<uint16_t,4>{{36109,36366,36623,36880}}));
 assert((e.scroll==std::array<uint16_t,2>{{33539,33796}}));
 assert(e.transitionCounter==40);
 for(auto flags:{std::pair<bool,bool>{false,true},{true,false},{false,false}})
  assert(!S9xRemasterExtractDungeonLayoutEvidence(ram.data(),ram.size(),flags.first,flags.second).available);
 assert(!S9xRemasterExtractDungeonLayoutEvidence(nullptr,ram.size(),true,true).available);
 assert(!S9xRemasterExtractDungeonLayoutEvidence(ram.data(),0x617,true,true).available);
 ram.fill(0);auto zero=S9xRemasterExtractDungeonLayoutEvidence(ram.data(),ram.size(),true,true);
 assert(zero.available&&zero.loadedLayout==0&&zero.targets[3]==0);
 std::ostringstream absent,present;S9xRemasterWriteDungeonLayoutEvidenceJson(absent,{});S9xRemasterWriteDungeonLayoutEvidenceJson(present,zero);
 assert(absent.str()=="{\"available\":false}");
 assert(present.str().find("\"available\":true")!=std::string::npos&&present.str().find("\"bounds_y_a0_b0_a1_b1\":[0,0,0,0]")!=std::string::npos);
 std::ostringstream raw;S9xRemasterWriteDungeonLayoutEvidenceJson(raw,e);
 assert(raw.str()=="{\"available\":true,\"quadrant_a6_aa\":[33,34,35,36,37],\"loaded_layout_word\":33025,\"blastwall_x_y\":[38,39],\"override_word_fc\":33282,\"bounds_y_a0_b0_a1_b1\":[34053,34310,34567,34824],\"bounds_x_a0_b0_a1_b1\":[35081,35338,35595,35852],\"scroll_copy2_x_y\":[33539,33796],\"targets_ud_ud_end_lr_lr_end\":[36109,36366,36623,36880],\"transition_counter\":40}");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.layoutEvidence=e;c.roomIndex=c.previousRoomIndex=0x55;
 RemasterProfile p;p.gameTitle="Layout evidence";p.romSha256=std::string(64,'a');
 auto pin=S9xRemasterPinDungeonCaptureEvidence(&c,p,nullptr,1,2,false);
 c.layoutEvidence=zero;assert(pin&&pin->context.layoutEvidence.bounds==e.bounds);
 for(int module:{6,7,14})for(int sub=0;sub<32;sub++) {
  c.module=module;c.submodule=sub;
  assert(S9xRemasterDungeonScrollCaptureEligible(c)==(module==7&&(sub==1||sub==2)));
 }
 c.module=7;c.submodule=1;c.verifiedAlttpRom=false;assert(!S9xRemasterDungeonScrollCaptureEligible(c));
 c.verifiedAlttpRom=true;c.indoors=false;assert(!S9xRemasterDungeonScrollCaptureEligible(c));c.indoors=true;
 const auto root="build/remaster-tests/layout-evidence-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 assert(std::filesystem::create_directory(root));
 S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);
 uint16_t pixel=0x1234;RemasterAreaContext area{0x55,false};
 auto run=[&](){RemasterFrame f;S9xRemasterBeginFrame(1,1,1,1);S9xRemasterEndFrame(&pixel,1,1,1,&f,&c,nullptr,&area);std::vector<uint8_t> b;assert(S9xSerializeRemasterFrame(f,b));return b;};
 c.submodule=0;auto baseline=run();c.layoutEvidence=e;assert(run()==baseline);
 // First animation frame owns the new layout values even if frame two changes
 // them. This does not claim unrelated existing context pointers are all owned.
 S9xRemasterRequestFrameCapture(root+"/animation.s9xrmf",2);run();c.layoutEvidence=zero;run();
 auto captured=S9xRemasterState().lastWrittenCapturePath;
 assert(Read(captured+".dungeon.json").find("\"layout_evidence\":"+raw.str())!=std::string::npos);
 c.layoutEvidence={};S9xRemasterWriteDungeonCaptureEvidence(root+"/absent",c,p,nullptr,0,0,false);
 assert(Read(root+"/absent.dungeon.json").find("\"layout_evidence\":{\"available\":false}")!=std::string::npos);
 c.layoutEvidence=e;c.submodule=1;auto transitionBaseline=run();
 // Same-room submodule 1 now dispatches, 2 still dispatches. Chronology is
 // bounded, not guaranteed complete; this checks sequential occupied-file
 // preservation, not atomic exclusion of concurrent writers.
 const auto prefix=root+"/scroll";setenv("S9X_REMASTER_DUNGEON_SCROLL_CAPTURE",prefix.c_str(),1);
 const auto occupied=prefix+"-crossing1-000.s9xrmf";{std::ofstream out(occupied);out<<"preserve";}
 assert(run()==transitionBaseline);assert(Read(occupied)=="preserve");assert(!std::filesystem::exists(occupied+".dungeon.json"));
 assert(run()==transitionBaseline);const auto next=prefix+"-crossing1-001.s9xrmf";
 assert(Read(next+".dungeon.json").find("\"layout_evidence\":{\"available\":true")!=std::string::npos);
 for(int k=2;k<121;k++)run();assert(!std::filesystem::exists(prefix+"-crossing1-120.s9xrmf"));
 c.submodule=0;run();c.submodule=2;run();assert(std::filesystem::exists(prefix+"-crossing2-000.s9xrmf"));
 for(int k=1;k<120;k++)run();c.submodule=0;run();c.submodule=1;run();
 assert(!std::filesystem::exists(prefix+"-crossing3-000.s9xrmf"));
 unsetenv("S9X_REMASTER_DUNGEON_SCROLL_CAPTURE");
 c.submodule=0;assert(run()==baseline);
 // JSON does not imply binary recording support: RTE1 remains byte-identical.
 auto a=RemasterRecording::Evidence(&c,nullptr,nullptr,nullptr,nullptr,false,0,0,0,nullptr,nullptr);
 c.layoutEvidence={};auto b=RemasterRecording::Evidence(&c,nullptr,nullptr,nullptr,nullptr,false,0,0,0,nullptr,nullptr);assert(a==b);
 std::cout<<"PASS layout extraction, zero/absence, JSON, first-pin, dispatch/cap/no-overwrite and packet/RTE1 parity; "<<root<<'\n';
}
