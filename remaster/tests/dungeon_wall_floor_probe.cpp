// Complete pinned EndFrame replay and placed wall/floor seam inventory.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
#include "../runtime/lighting_field_cache.h"
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 auto &s=S9xRemasterState();
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  std::ofstream out(std::string(argv[2])+"-bg"+std::to_string(bg)+".json");out<<'[';
  for(int i=0;i<4096;i++) {
   if(i)out<<',';const auto &t=tiles[i];
   out<<"{\"cell\":"<<i<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"surface\":"<<int(t.surface)
    <<",\"height\":"<<t.hasHeight<<",\"base\":"<<m.offsets[i]<<",\"structural\":["<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]
    <<"],\"planes\":["<<m.planeOffsets[0][i]<<','<<m.planeOffsets[1][i]<<"],\"flip\":["<<t.hflip<<','<<t.vflip<<"],\"template\":"<<t.wallTemplateOffset<<",\"stack\":"<<t.wallStackAssembly<<",\"local\":[";
   for(int k=0;k<64;k++){if(k)out<<',';out<<t.height[k];}out<<"]}";
  }out<<"]\n";
 }
 if(std::string(argv[1]).find("frame_room051_061.s9xrmf")!=std::string::npos) {
  const auto &tiles=*s.dungeonRetainedBg2;const auto &m=s.runtimeDungeonHeightMap;size_t walls=0;
  auto expected=[](int x){return x<=19?24+8*std::max(0,18-x):24+8*std::max(0,x-45);};
  for(int y:{16,17,18,19,22,23,24,25,28,29,30,31})for(int x:{16,17,18,19,44,45,46,47}) {
   const int i=y*64+x;
   check(tiles[i].hasHeight && tiles[i].wallStackAssembly,"complete exact wall-stack ownership/source");
   check(m.offsets[i]==expected(x) && m.structuralPlaneOffsets[0][i]==expected(x) && m.structuralPlaneOffsets[1][i]==255,"whole stack signed bases40/32/24/24; no lower-wall claim");walls++;
  }
  const auto metaBytes=read(std::string(argv[1])+".dungeon.json");const std::string meta(metaBytes.begin(),metaBytes.end());
  const auto scrollPos=meta.find("\"scroll\":[")+10;const int scrollX=std::stoi(meta.substr(scrollPos)),scrollY=std::stoi(meta.substr(meta.find(',',scrollPos)+1));
  auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*f),"production packet");
  RemasterLightingFieldCache fields;fields.update(*f,true,true);size_t pixels=0;std::set<int> bands;
  for(size_t at=0;at<f->scenePixels.size();at++) {
   const auto &px=f->scenePixels[at];if(!px.instanceId)continue;const auto &inst=f->tileInstances[px.instanceId-1];
   if(inst.source!=RemasterSourceType::Background || inst.sourceIndex!=1)continue;
   const int x=((scrollX+at%f->width)&511)/8,y=((scrollY+at/f->width)&511)/8;
   if(!tiles[y*64+x].wallStackAssembly)continue;
   const auto *metadata=S9xRemasterFrameMetadataForTile(*f,inst.tileId);check(metadata,"metadata");
   const auto effective=S9xRemasterResolveFrameGeometry(*metadata,*f,px.instanceId-1);
   check(effective.hasHeight && !inst.hasPlacementHeight && inst.heightOffset==expected(x),"actual EndFrame authored array plus solved offset");
   check(fields.height[at*2+1] && fields.height[at*2]==S9xRemasterSignedHeightField(expected(x),effective.height[px.tilePixel]),"CPU lighting field equals base plus authored local");
   bands.insert(x);pixels++;
  }
  check(pixels>0 && bands.size()>=4,"visible whole-stack field denominator");
  std::cout<<"PASS own061 complete walls="<<walls<<" scene field samples="<<pixels<<" bands="<<bands.size()<<'\n';
 }
 if(std::string(argv[1]).find("frame_room051_059.s9xrmf")!=std::string::npos) {
  const auto &tiles=*s.dungeonRetainedBg2;const auto &m=s.runtimeDungeonHeightMap;
  size_t feet=0;
  for(int i=0;i<4096;i++)if(tiles[i].tileHash==UINT64_C(0x966069b2157ea21f)) {
   check(tiles[i].hasHeight && *std::max_element(tiles[i].height.begin(),tiles[i].height.end())==5,"active authored foot relief");
#ifndef ALTTP_TEST_NO_WALL_FLOOR_FEET
   check(m.offsets[i]==24 && m.structuralPlaneOffsets[0][i]==24 && m.structuralPlaneOffsets[1][i]==255,"direct foot base24, no underpass wall");
#endif
   check(m.planeOffsets[0][i]==24 && m.planeOffsets[1][i]==0,"independent overlapping support24/0");feet++;
  }check(feet==24,"complete foot denominator");
  auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*f),"production packet");
  RemasterLightingFieldCache fields;fields.update(*f,true,true);size_t pixels=0;
  for(size_t at=0;at<f->scenePixels.size();at++) {
   const auto &px=f->scenePixels[at];if(!px.instanceId)continue;const auto &inst=f->tileInstances[px.instanceId-1];
   if(inst.source!=RemasterSourceType::Background || inst.sourceIndex!=1 || inst.tileId.hash!=UINT64_C(0x966069b2157ea21f))continue;
   const auto *metadata=S9xRemasterFrameMetadataForTile(*f,inst.tileId);check(metadata,"metadata");
   const auto effective=S9xRemasterResolveFrameGeometry(*metadata,*f,px.instanceId-1);
   check(effective.hasHeight && effective.hasNormals && !inst.hasPlacementHeight,"actual effective authored source, no generated replacement");
#ifndef ALTTP_TEST_NO_WALL_FLOOR_FEET
   check(inst.heightOffset==24 && fields.height[at*2+1] && fields.height[at*2]==S9xRemasterSignedHeightField(24,effective.height[px.tilePixel]),"actual EndFrame/lighting field base+local");
#endif
   pixels++;
  }check(pixels>0,"visible foot denominator");std::cout<<"PASS own059 active feet="<<feet<<" actual scene field pixels="<<pixels<<" independent planes24/0\n";
 }
}
