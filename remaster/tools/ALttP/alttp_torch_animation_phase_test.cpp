// Exact complete room inputs + ROM/atlas-derived animation DMA, selected profile.
// Research phase replay, not a claim of fresh gameplay capture.
#include "../../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);need(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 need(argc==5,"capture profile three-phase-DMA output-prefix");std::string capture=argv[1];
 auto input=read(capture+".dungeon-input.bin"),tpl=read(capture+".dungeon-templates.bin"),phases=read(argv[3]);
 need(input.size()==90120 && tpl.size()==12884 && phases.size()==3*1024,"complete evidence size");
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;need(S9xRemasterLoadProfile(argv[2],p,errors),"selected profile");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=0x50;c.linkX=371;c.linkY=2691;c.linkPlane=1;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=tpl.data()+8;
 // Pin these actual world values to007 evidence, not a reusable room-specific rule.
 auto metadata=read(capture+".dungeon.json");std::string text(metadata.begin(),metadata.end());
 need(text.find("\"room\":80")!=std::string::npos && text.find("\"profile_serialized_fnv1a64\":\"8ce20b3a13ee3191\"")!=std::string::npos,"own007 input pin");
 std::string serialized;need(S9xRemasterSerializeProfile(p,serialized,errors),"profile serialization");
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>(),lowerMap=std::make_unique<RemasterDungeonHeightMap>();
 std::vector<int> reference;int rejected=0;
 for(int phase=0;phase<3;phase++) {
  // NMI copies0x400 bytes at VRAM WORD3b00 (byte7600). Both themed half
  // and pack5c half are supplied by independently decoded actual ROM phases.
  std::copy_n(phases.data()+phase*1024,1024,input.data()+24584+0x7600);
  std::rotate(input.begin()+8,input.begin()+8200,input.begin()+16392);c.placementBackgroundIndex=0;
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();need(S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*lowerMap),"BG1 solve");
  std::rotate(input.begin()+8,input.begin()+8200,input.begin()+16392);c.placementBackgroundIndex=1;
  c.wallSupportRelations=&lowerMap->authoredWallSupportRelations;c.supportRelations=&lowerMap->authoredSupportRelations;
  S9xRemasterDungeonPlacements(c,p,*tiles);need(S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*map),"BG2 solve");
  std::vector<int> effective;int missing=0;
  for(int origin:{7*64+48,15*64+59}) {
   int w=origin%64==48?4:2,h=origin%64==48?2:4;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
    int i=origin+y*64+x;const auto &t=(*tiles)[i];int base=map->offsets[i];missing+=base==255;
    const auto preview=std::find_if(map->localHeightPreviews.begin(),map->localHeightPreviews.end(),[&](const auto &v){return v.cell==i;});
    for(int py=0;py<8;py++)for(int px=0;px<8;px++){int k=(t.vflip?7-py:py)*8+(t.hflip?7-px:px);effective.push_back(base==255?255:base+(preview==map->localHeightPreviews.end()?t.height[k]:preview->height[k]));}
   }
  }
  RemasterFrame f;f.width=256;f.height=224;c.backgroundScrollX=256;c.backgroundScrollY=256;
  auto sources=S9xRemasterAppendOffscreenRoomLights(f,c,p,map.get(),lowerMap.get(),S9xRemasterHashTile);
  bool equal=phase==0 || effective==reference;if(phase==0)reference=effective;
  std::cout<<"phase="<<phase<<" fixture_missing="<<missing<<" source_unknown="<<sources.unknownBases<<" spheres="<<sources.lights<<" geometry_stable="<<equal<<" wall_unknown="<<map->unknownWallCells<<" conflicts="<<map->structuralConflicts<<'\n';
  rejected+=missing>0 || sources.unknownBases>0 || !equal;
 }
 std::string after;need(S9xRemasterSerializeProfile(p,after,errors) && after==serialized,"profile mutated by solve");
 std::cout<<"phase_failures="<<rejected<<'\n';return rejected?1:0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
