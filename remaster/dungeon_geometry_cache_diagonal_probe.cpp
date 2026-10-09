// Reproduce the existing variable-diagonal test's FIRST assertion exactly.
// It is a direct solve, never EndFrame: cache ablation cannot affect this path.
#include "remaster.h"
#include "dungeon_geometry_cache_inventory.h"
#include <iostream>
#include <stdexcept>
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 if(argc!=3)throw std::runtime_error("usage: capture output-prefix");const std::string path=argv[1],out=argv[2];
 auto input=read(path+".dungeon-input.bin"),tmpl=read(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 if(!S9xRemasterLoadProfile(path+".profile.toml",profile,errors))throw std::runtime_error("profile");
 RemasterDungeonFloorContext c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=0x61;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.graphics=input.data()+24584;
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.dungeonTemplateData=tmpl.data()+8;
 c.linkX=649;c.linkY=3347;S9xRemasterDungeonStairsFromAttributes(c);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,profile,*tiles);c.placements=tiles->data();auto m=std::make_unique<RemasterDungeonHeightMap>();
 if(!S9xRemasterBuildDungeonHeightMap(c,50,*m))throw std::runtime_error("solve");
 std::ofstream inventory(out+".inventory");inventory<<dungeonMapInventory(*m);
 std::ofstream report(out+".json");report<<"{\"test_room\":97,\"test_link\":[649,3347],\"conflicts\":"<<m->structuralConflicts<<",\"unknown_walls\":"<<m->unknownWallCells<<",\"geometry_conflicts\":[";
 for(size_t k=0;k<m->geometryConflicts.size();k++){if(k)report<<',';const auto &r=m->geometryConflicts[k];report<<'['<<r.node<<','<<r.other<<','<<r.expected<<','<<r.actual<<']';}
 report<<"],\"issue_placements\":[";for(size_t k=0;k<m->geometryIssuePlacements.size();k++){if(k)report<<',';auto &r=m->geometryIssuePlacements[k];report<<'['<<r[0]<<','<<r[1]<<','<<r[2]<<','<<r[3]<<']';}report<<"]}\n";
 std::cout<<"conflicts="<<m->structuralConflicts<<" unknown_walls="<<m->unknownWallCells<<" equality_conflicts="<<m->geometryConflicts.size()<<'\n';
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
