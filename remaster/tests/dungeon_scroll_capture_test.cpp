#include "../runtime/remaster.h"
#include <stdexcept>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
 check(argc==3,"usage: existing-unique-prefix own-profile");std::string prefix=argv[1];
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;check(S9xRemasterLoadProfile(argv[2],p,errors),"profile");S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);
 setenv("S9X_REMASTER_DUNGEON_SCROLL_CAPTURE",prefix.c_str(),1);
 auto maps=std::make_unique<std::array<uint8_t,16384>>();auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();auto templates=std::make_unique<std::array<uint8_t,12876>>();
 attrs->fill(1);RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x50;
 c.constructedMaps=maps->data();c.collisionAttributes=attrs->data();c.graphics=gfx->data();c.dungeonTemplateData=templates->data();
 auto frame=[&]{uint16_t screen[64]={};S9xRemasterBeginFrame(64,8,8,8);S9xRemasterEndFrame(screen,8,8,8,nullptr,&c);};
 auto name=[&](int crossing,int at){std::ostringstream s;s<<prefix<<"-crossing"<<crossing<<'-'<<std::setw(3)<<std::setfill('0')<<at<<".s9xrmf";return s.str();};
 check(!std::ifstream(name(1,0)).good(),"unique prefix required");frame();check(!std::ifstream(name(1,0)).good(),"navigation consumed diagnostic");
 // An orphan sidecar must not be replaced or paired with a new packet.
 {std::ofstream old(name(1,0)+".dungeon.json");old<<"sentinel";}
 c.submodule=2;c.previousRoomIndex=0x60;for(int k=0;k<125;k++){if(k==90)c.submodule=0;frame();}
 check(!std::ifstream(name(1,0)).good(),"orphan sidecar overwritten");
 {std::ifstream old(name(1,0)+".dungeon.json");std::string v;old>>v;check(v=="sentinel","sentinel changed");}
 check(std::ifstream(name(1,119)).good() && !std::ifstream(name(1,120)).good(),"first bound");
 for(int k=0;k<140;k++)frame(); // reverse crossing may occur much later
 c.roomIndex=0x60;c.previousRoomIndex=0x50;c.submodule=2;
 for(int k=0;k<121;k++){if(k==90)c.submodule=0;frame();}
 for(const char *suffix:{"",".profile.toml",".dungeon.json",".dungeon-input.bin",".dungeon-templates.bin"})check(std::ifstream(name(2,119)+suffix).good(),"reverse own evidence missing");
 check(!std::ifstream(name(2,120)).good(),"reverse bound");
 c.submodule=2;c.roomIndex=0x50;c.previousRoomIndex=0x60;frame();check(!std::ifstream(name(3,0)).good(),"third unbounded crossing");
 std::cout<<"PASS diagnostic waits, two delayed directions bounded120each, orphan/packet nonoverwrite, own evidence retained\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
