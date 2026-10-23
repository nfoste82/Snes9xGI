// Complete-input production investigation; no fixture-specific solver assertions.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) {
 if(captureProbeMain(argc,argv))return 1;
 const auto &s=S9xRemasterState();
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  std::ofstream out(std::string(argv[2])+"-bg"+std::to_string(bg)+".json");out<<'[';
  for(int i=0;i<4096;i++) {
   const auto &t=tiles[i];if(i)out<<',';
   out<<"{\"cell\":"<<i<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"surface\":"<<int(t.surface)
    <<",\"height\":"<<t.hasHeight<<",\"base\":"<<m.offsets[i]<<",\"planes\":["<<m.planeOffsets[0][i]<<','<<m.planeOffsets[1][i]
    <<"],\"authored_assembly\":"<<t.authoredObstacleAssembly<<",\"template\":"<<t.wallTemplateOffset<<'}';
  }out<<"]\n";
 }
 auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*f),"output");
 std::map<std::string,int> omitted;
 for(size_t k=0;k<f->mainPixels.size();k++)if(!f->scenePixels[k].instanceId && !f->mainPixels[k].instanceId && f->subPixels[k].instanceId) {
  const auto &px=f->subPixels[k];const auto &i=f->tileInstances[px.instanceId-1];const auto *m=S9xRemasterFrameMetadataForTile(*f,i.tileId);if(!m)continue;
  const auto e=S9xRemasterResolveFrameGeometry(*m,*f,px.instanceId-1);std::ostringstream key;
  key<<int(i.sourceIndex)<<':'<<std::hex<<i.tileId.hash<<std::dec<<":role="<<int(e.tileRole)<<":fields="<<e.hasHeight<<e.hasNormals<<":art="<<i.hasPaletteArtwork;
  omitted[key.str()]++;
 }
 for(const auto &v:omitted)std::cout<<"omitted "<<v.first<<" pixels="<<v.second<<'\n';
 std::ofstream image(std::string(argv[2])+".ppm",std::ios::binary);image<<"P6\n"<<f->width<<' '<<f->height<<"\n255\n";
 for(auto color:f->originalRgb555){const char rgb[]={char((color&31)*255/31),char(((color>>5)&31)*255/31),char(((color>>10)&31)*255/31)};image.write(rgb,3);}
}
