#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){if(captureProbeMain(argc,argv))return 1;
 auto input=read(std::string(argv[1])+".dungeon-input.bin"),templates=read(std::string(argv[1])+".dungeon-templates.bin");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;c.graphicsBase=c.bg1GraphicsBase=16384;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(c,S9xRemasterState().activeProfile,*tiles);
 const auto &m=S9xRemasterState().runtimeDungeonHeightMap;std::ofstream report(std::string(argv[2])+"-parts.json");report<<'[';bool first=true;
 for(int i=0;i<4096;i++)if((*tiles)[i].tileHash==UINT64_C(0xeff6f17a9f4420d9)) {
  const int x=i%64,y=i/64;for(int yy=y-3;yy<=y+1;yy++)for(int xx=x;xx<=x;xx++) {
   const int cell=yy*64+xx;const auto &t=(*tiles)[cell];if(!first)report<<',';first=false;
   report<<"{\"x\":"<<xx<<",\"y\":"<<yy<<",\"surface\":"<<int(t.surface)<<",\"assembly\":"<<t.obstacleAssembly<<",\"template\":"<<t.obstacleTemplate<<",\"base\":"<<m.offsets[cell]<<",\"floor\":"<<m.planeOffsets[0][cell]<<",\"fallback\":"<<int(m.fallbackDirections[cell])<<",\"attrs\":["<<int(c.collisionAttributes[cell])<<','<<int(c.collisionAttributes[4096+cell])<<"],\"height\":[";
   for(int k=0;k<64;k++){if(k)report<<',';report<<int(t.height[k]);}report<<"]}";
   std::cout<<xx<<','<<yy<<" surface="<<int(t.surface)<<" assembly="<<t.obstacleAssembly<<" template="<<std::hex<<t.obstacleTemplate<<std::dec<<" base="<<m.offsets[cell]<<" floor="<<m.planeOffsets[0][cell]<<" fallback="<<int(m.fallbackDirections[cell])<<'\n';
  }
 }report<<"]\n";
 auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"produced packet");
 size_t usages=0,overrides=0;std::map<int,std::set<int>> rowBases;std::set<uint64_t> targetIds;
 for(int x:{12,13,16,17,20,21,42,43,46,47,50,51})for(int y=28;y<32;y++)targetIds.insert((*tiles)[y*64+x].tileHash);
 for(const auto *pixels:{&packet->mainPixels,&packet->subPixels})for(size_t at=0;at<pixels->size();at++) {
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=packet->tileInstances[px.instanceId-1];
  if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || !targetIds.count(i.tileId.hash))continue;
  usages++;overrides+=i.hasPlacementHeight;
  const int y=((3230+at/packet->width)&511)/8;rowBases[y].insert(i.heightOffset);
#ifndef ALTTP_TEST_PRIOR_OBSTACLE_SYMMETRY
  check(!i.hasPlacementHeight && i.heightOffset==0,"actual column relief not preserved/shared footbase0");
#endif
 }
 std::cout<<"actual_column_pixel_usages="<<usages<<" replaced_local_usages="<<overrides<<'\n';
 std::ofstream effective(std::string(argv[2])+"-effective.json");effective<<"{\"pixel_usages\":"<<usages<<",\"local_overrides\":"<<overrides<<",\"bases_by_row\":{";bool initial=true;for(auto &r:rowBases){if(!initial)effective<<',';initial=false;effective<<'\"'<<r.first<<"\":[";bool a=true;for(int b:r.second){if(!a)effective<<',';a=false;effective<<b;}effective<<']';}effective<<"}}\n";
 for(int off:{0x84a,0x85a}){std::cout<<"template "<<std::hex<<off;for(int n=0;n<8;n++)std::cout<<' '<<(c.dungeonTemplateData[off+n*2]|c.dungeonTemplateData[off+n*2+1]<<8);std::cout<<std::dec<<'\n';}
}
