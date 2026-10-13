// Exact own-input regression for local-top cardinal cross-section calibration.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);need(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 need(argc==2,"usage: complete own capture");std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),tmpl=read(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;need(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"profile");
 RemasterDungeonFloorContext c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=97;c.linkX=597;c.linkY=3190;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.constructedMaps=input.data()+8;c.graphics=input.data()+24584;c.collisionAttributes=input.data()+16392;c.dungeonTemplateData=tmpl.data()+8;
 S9xRemasterDungeonStairsFromAttributes(c);auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto solve=[&]{S9xRemasterDungeonPlacements(c,profile,*tiles);c.placements=tiles->data();auto m=std::make_unique<RemasterDungeonHeightMap>();need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"solve");return m;};
 std::string before,after;need(S9xRemasterSerializeProfile(profile,before,errors),"serialize");
 auto map=solve();need(!map->structuralConflicts && !map->unknownPlaneCells[0],"room solve conflicts");
 size_t count=0;for(int y=0;y<64;y++)for(int x=0;x<64;x++)if((*tiles)[y*64+x].diagonalHeightPreview){count++;need(map->offsets[y*64+x]!=255,"diagonal unresolved");}
 need(count>100,"complete diagonal coverage");
 for(int x=6;x<12;x++)for(int row=0;row<4;row++) {
  const int cell=(15-x+row)*64+x;const auto &t=(*tiles)[cell];need(t.diagonalHeightPreview,"northwest matched shell");
  need(map->offsets[cell]+t.height[0]==24+std::min(24,31-row*8),"northwest exact placement ramp");
 }
 // One top axis may be placement-local0..7, never arbitrary or misclassified.
 const auto word=tmpl[8+0x2c8]|tmpl[9+0x2c8]<<8;uint8_t pixels[64];
 for(int y=0;y<8;y++)for(int x=0;x<8;x++){int n=0;for(int b=0;b<4;b++)n|=((c.graphics[uint16_t(c.graphicsBase+(word&1023)*32+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;pixels[y*8+x]=n;}
 RemasterTileContentId id={S9xRemasterHashTile(4,pixels),1,4};auto saved=profile.assets.at(id);
 profile.assets[id].height[0]++;solve();need(!(*tiles)[5*64+11].diagonalHeightPreview,"malformed local top accepted");
 profile.assets[id]=saved;profile.assets[id].tileRole=RemasterTileRole::Unknown;solve();need(!(*tiles)[5*64+11].diagonalHeightPreview,"unclassified local top accepted");
 profile.assets[id]=saved;auto original=input;input[8+(4*64+11)*2]=255;input[9+(4*64+11)*2]=3;solve();need(!(*tiles)[4*64+11].diagonalHeightPreview,"overwritten member accepted");input=original;
 need(S9xRemasterSerializeProfile(profile,after,errors) && before==after,"authored arrays changed");
 std::cout<<"PASS exact own cardinal local-top/both-axis calibration, oblique seam single-owner, complete NW/SW diagonals, malformed/unknown/overwrite rejection, immutable authored profile\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
