// Complete pinned room solve attribution. No synthetic geometry or input mutation.
#define ALTTP_TEST_BUILD_WORK
#include "../runtime/remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static std::vector<uint8_t> read(const std::string &p) {
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error(p);
 return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char **argv) {try {
 if(argc!=5)throw std::runtime_error("usage: input templates profile output-inventory");
 auto input=read(argv[1]),tpl=read(argv[2]);
 if(input.size()!=8+16384+8192+65536 || tpl.size()!=8+12876)throw std::runtime_error("complete input sizes");
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;
 if(!S9xRemasterLoadProfile(argv[3],p,errors))throw std::runtime_error("profile");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=47;c.linkX=8055;c.linkY=1389;c.linkPlane=1;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+8+16384;
 c.graphics=input.data()+8+16384+8192;c.dungeonTemplateData=tpl.data()+8;
 S9xRemasterDungeonStairsFromAttributes(c);
 auto placements=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,p,*placements);c.placements=placements->data();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 S9xRemasterDungeonBuildWork()={};
 for(int n=0;n<12;n++) {c.linkX=8055+n%3;
  if(!S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*map))throw std::runtime_error("solve");
  if(n==11){std::ofstream o(argv[4]);o<<dungeonMapInventory(*map);}
 }
 auto &w=S9xRemasterDungeonBuildWork();
 std::cout<<"movement_nodes="<<w.movementNodes<<"\n";
 for(int i=0;i<16;i++)std::cout<<"phase="<<i<<" calls="<<w.calls[i]<<" ms="<<w.ms[i]<<"\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
