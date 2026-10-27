// Bounded BG2 closed-assembly replay. NOT a whole-frame parity substitute:
// each admitted sample verifies the final word, decoded hash and canonical HV.
#include "../runtime/remaster.h"
#include "../runtime/lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);check(bool(f),"missing input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 check(argc==3 || argc==5,"capture output [--research-profile profile]");
 const std::string path=argv[1],out=argv[2];
 auto input=read(path+".dungeon-input.bin"),tpl=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 check(input.size()==90120 && !std::memcmp(input.data(),"ALTPDI1\0",8),"complete input");
 check(tpl.size()==12884 && !std::memcmp(tpl.data(),"ALTPDT1\0",8),"templates");
 std::string text(meta.begin(),meta.end());
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"missing key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;while(index--)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
 check(text.find("66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb")!=std::string::npos,"ROM pin");
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;
 if(argc==5)check(std::string(argv[3])=="--research-profile","explicit research mode");
 check(S9xRemasterLoadProfile(argc==5?argv[4]:(path+".profile.toml").c_str(),p,errors),"profile");
 auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*f),"packet");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
 c.graphics=input.data()+24584;c.dungeonTemplateData=tpl.data()+8;c.collisionAttributes=input.data()+16392;
 c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
 auto maps=std::make_unique<std::array<uint8_t,16384>>();std::copy_n(input.data()+8,16384,maps->data());c.constructedMaps=maps->data();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto lower=std::make_unique<RemasterDungeonHeightMap>(),upper=std::make_unique<RemasterDungeonHeightMap>();
 std::rotate(maps->begin(),maps->begin()+8192,maps->end());std::swap(c.graphicsBase,c.bg1GraphicsBase);c.placementBackgroundIndex=0;
 S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();check(S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*lower),"BG1 solve");
 std::rotate(maps->begin(),maps->begin()+8192,maps->end());std::swap(c.graphicsBase,c.bg1GraphicsBase);c.placementBackgroundIndex=1;
 c.supportRelations=&lower->authoredSupportRelations;c.wallSupportRelations=&lower->authoredWallSupportRelations;c.doorwaySupportRelations=&lower->doorwaySupportRelations;
 S9xRemasterDungeonPlacements(c,p,*tiles);check(S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*upper),"BG2 solve");
 std::set<int> admitted;size_t samples=0;
 for(auto pixels:{&f->mainPixels,&f->subPixels,&f->scenePixels})for(size_t at=0;at<pixels->size();at++) {
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=f->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1)continue;
  int x=(c.backgroundScrollX+at%f->width)&511,y=(c.backgroundScrollY+at/f->width)&511,cell=y/8*64+x/8;
  const auto &t=(*tiles)[cell];if(!t.closedDoorAssembly)continue;
  check(i.tileId.hash==t.tileHash && i.hFlip==t.hflip && i.vFlip==t.vflip && px.tilePixel==(t.vflip?7-y%8:y%8)*8+(t.hflip?7-x%8:x%8),"stale BG2 target identity");
  admitted.insert(cell);samples++;
 }
 check(samples>0,"no target samples");
 // Reapply immutable selected metadata, never transport baked old placement bases.
 for(auto &i:f->tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
 f->contextGeometry.clear();
 S9xRemasterApplyProfileToFrame(p,*f);
 // Production EndFrame applies to the physical scene in place of main, then
 // restores presentation. ApplyDungeonHeightMap intentionally visits main/sub.
 if(!f->scenePixels.empty())f->mainPixels.swap(f->scenePixels);
 S9xRemasterApplyDungeonHeightMap(*f,c,*upper,nullptr,nullptr,1);
 if(!f->scenePixels.empty())f->mainPixels.swap(f->scenePixels);
 RemasterLightingFieldCache fields;fields.update(*f,!f->scenePixels.empty(),true);
 size_t checked=0;for(size_t at=0;at<f->scenePixels.size();at++) {
  const auto &px=f->scenePixels[at];if(!px.instanceId)continue;const auto &i=f->tileInstances.at(px.instanceId-1);
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1)continue;
  int cell=(((c.backgroundScrollY+at/f->width)&511)/8)*64+((c.backgroundScrollX+at%f->width)&511)/8;if(!admitted.count(cell))continue;
  const auto *md=S9xRemasterFrameMetadataForTile(*f,i.tileId);check(md,"metadata");auto g=S9xRemasterResolveFrameGeometry(*md,*f,px.instanceId-1);
  std::cout<<"{\"x\":"<<cell%64<<",\"y\":"<<cell/64<<",\"k\":"<<int(px.tilePixel)<<",\"base\":"<<i.heightOffset<<",\"local\":"<<g.height[px.tilePixel]<<",\"height\":"<<i.heightOffset+g.height[px.tilePixel]<<",\"field\":"<<fields.height[at*2]*255<<",\"known\":"<<(fields.height[at*2+1]>.5)<<",\"normal\":["<<int(g.normalXyz[px.tilePixel*3])<<','<<int(g.normalXyz[px.tilePixel*3+1])<<','<<int(g.normalXyz[px.tilePixel*3+2])<<"]}\n";checked++;
 }
 check(checked>0,"no physical scene samples");check(S9xWriteRemasterFrame(*f,out),"write review packet");
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
