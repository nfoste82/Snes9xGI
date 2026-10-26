// Exact own-input diagnostic; coordinates below are audit queries, not rules.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){try {
 check(captureFixtureMain(argc,argv)==0,"own replay");auto &s=S9xRemasterState();
 RemasterFrame f,old;check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",f),"replay");check(S9xReadRemasterFrame(argv[1],old),"original");
 for(auto *pixels:{&f.mainPixels,&f.scenePixels})for(int slot:{68,69}){int x0=999,y0=999,x1=-1,y1=-1;for(size_t at=0;at<pixels->size();at++)if((*pixels)[at].instanceId){const auto &i=f.tileInstances[(*pixels)[at].instanceId-1];if(i.source==RemasterSourceType::Object && i.sourceIndex==slot){x0=std::min(x0,int(at%f.width));x1=std::max(x1,int(at%f.width));y0=std::min(y0,int(at/f.width));y1=std::max(y1,int(at/f.width));}}std::cout<<"BOUNDS "<<(pixels==&f.mainPixels?"main":"scene")<<" slot="<<slot<<' '<<x0<<','<<y0<<':'<<x1<<','<<y1<<'\n';}
 for(size_t id=0;id<old.tileInstances.size();id++){const auto &i=old.tileInstances[id];if(i.source!=RemasterSourceType::Object || (i.sourceIndex!=68 && i.sourceIndex!=69))continue;
  auto *m=S9xRemasterFrameMetadataForTile(old,i.tileId);auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},old,id);
  std::cout<<"OLDOBJ "<<id+1<<" slot="<<int(i.sourceIndex)<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" base="<<i.heightOffset<<" placement="<<i.hasPlacementHeight<<" context="<<old.contextGeometry.count(id+1)<<" local="<<*std::min_element(g.height.begin(),g.height.end())<<":"<<*std::max_element(g.height.begin(),g.height.end())<<" normal="<<g.hasNormals<<'\n';
 }
 for(int bg=0;bg<2;bg++)for(int y:{4,5,6,7,10,11,12,13,14,15,16,17,18,55,56,57,58,59})for(int x=30;x<34;x++) {
  const int i=y*64+x;const auto &t=(bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[i];const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  std::cout<<"CELL "<<bg+1<<' '<<x<<' '<<y<<" hash="<<std::hex<<t.tileHash<<std::dec<<" surf="<<int(t.surface)<<" local="<<*std::min_element(t.height.begin(),t.height.end())<<":"<<*std::max_element(t.height.begin(),t.height.end())<<" assembly="<<t.doorAssembly<<" source="<<t.doorTemplateOffset<<" dir="<<int(t.doorDirection)<<" col="<<t.doorwayColumnContact<<" support="<<t.doorwaySupport<<" facing="<<int(t.doorwaySupportDirection)<<" offset="<<m.offsets[i];
  for(int p=0;p<2;p++){auto d=m.roomDiagnostics->cells[p*4096+i];std::cout<<" p"<<p<<"="<<m.planeOffsets[p][i]<<",floor="<<d.floorId<<",observed="<<d.planeObserved;}
  for(const auto &a:m.doorframeAttachments)if(a.cell==i)std::cout<<" attach="<<a.plane<<":"<<a.base<<":"<<a.visible;
  std::cout<<'\n';
 }
 for(size_t id=0;id<f.tileInstances.size();id++){const auto &i=f.tileInstances[id];if(i.source!=RemasterSourceType::Object)continue;
  int x0=999,y0=999,x1=-1,y1=-1;size_t count=0;for(size_t at=0;at<f.mainPixels.size();at++)if(f.mainPixels[at].instanceId==id+1){count++;x0=std::min(x0,int(at%f.width));x1=std::max(x1,int(at%f.width));y0=std::min(y0,int(at/f.width));y1=std::max(y1,int(at/f.width));}
  auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId);auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},f,id);
  if(std::string(argv[1]).find("room051_103")!=std::string::npos && (i.sourceIndex==68 || i.sourceIndex==69)){check(i.heightOffset==24 && !i.hasPlacementHeight && g.hasHeight && g.hasNormals,"own103 follower effective source");if(id<old.tileInstances.size()){auto *om=S9xRemasterFrameMetadataForTile(old,i.tileId);auto og=S9xRemasterResolveFrameGeometry(om?*om:RemasterFrameAssetMetadata{},old,id);check(g.height==og.height && g.normalXyz==og.normalXyz,"follower authored arrays changed");}}
  std::cout<<"OBJ "<<id+1<<" slot="<<int(i.sourceIndex)<<" tile="<<i.tileNumber<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" base="<<i.heightOffset<<" pixels="<<count<<" bounds="<<x0<<","<<y0<<":"<<x1<<","<<y1<<'\n';
 }
 return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
