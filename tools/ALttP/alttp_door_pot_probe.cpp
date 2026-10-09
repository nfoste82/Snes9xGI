// Full placed inventory plus producer evidence, not a viewport approval.
#include "../../remaster/remaster.h"
#include <iostream>
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 if(argc!=2)throw std::runtime_error("capture");const std::string p=argv[1];
 auto input=read(p+".dungeon-input.bin"),tpl=read(p+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 if(!S9xRemasterLoadProfile(p+".profile.toml",profile,errors))throw std::runtime_error("profile");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=tpl.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 auto t=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(c,profile,*t);
 for(int off:{0x10a8,0xe92}){std::cout<<"pot template "<<std::hex<<off;for(int n=0;n<4;n++)std::cout<<' '<<(tpl[8+off+n*2]|tpl[9+off+n*2]<<8);std::cout<<std::dec<<'\n';}
 std::cout<<"pot final";for(int i:{43*64+40,44*64+40,43*64+41,44*64+41})std::cout<<' '<<std::hex<<(c.constructedMaps[2*i]|c.constructedMaps[2*i+1]<<8);std::cout<<std::dec<<'\n';
 for(int i=0;i<4096;i++)if((*t)[i].doorTemplateOffset || (*t)[i].removableSupportPlanes || (*t)[i].tileHash==UINT64_C(0x71232b754e969748)) {
  const auto &a=(*t)[i];std::cout<<i%64<<','<<i/64<<" role="<<int(a.surface)<<" door="<<std::hex<<a.doorTemplateOffset<<" mask="<<a.doorframeMask<<std::dec<<" removable="<<int(a.removableSupportPlanes)<<'\n';
 }
 for(int tile:{0x10e,0x11e}){uint8_t pix[64];for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(c.graphicsBase+tile*32+(b/2)*16+y*2+b%2)]>>(7-x))&1)<<b;pix[y*8+x]=v;}
  const auto hash=S9xRemasterHashTile(4,pix);auto a=profile.assets.find({hash,1,4});std::cout<<"replacement "<<std::hex<<tile<<" hash="<<hash<<std::dec<<" role="<<(a==profile.assets.end()?-1:int(a->second.tileRole))<<" span=";
  if(a!=profile.assets.end())std::cout<<int(*std::min_element(a->second.height.begin(),a->second.height.end()))<<','<<int(*std::max_element(a->second.height.begin(),a->second.height.end()));std::cout<<'\n';
 }
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
