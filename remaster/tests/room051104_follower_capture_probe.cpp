// Exact own-input follower/source inventory. Audit coordinates never select runtime rules.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv){try {
 check(captureFixtureMain(argc,argv)==0,"own replay");auto &s=S9xRemasterState();
 RemasterFrame f,old;check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",f),"replay");check(S9xReadRemasterFrame(argv[1],old),"original");
 RemasterDungeonFloorContext c;check(S9xRemasterReadTerrainEvidence(std::string(argv[1])+".terrain-input.bin",c),"terrain");
 const bool exact104=std::string(argv[1]).find("frame_room051_104.s9xrmf")!=std::string::npos;
 if(exact104) {
  size_t checked=0;std::array<size_t,3> counts={};
  for(size_t id=0;id<f.tileInstances.size();id++) {
   const auto &i=f.tileInstances[id];if(i.source!=RemasterSourceType::Object || (i.sourceIndex!=48 && i.sourceIndex!=49))continue;
   check(id<old.tileInstances.size(),"follower instance ancestry");const auto &oi=old.tileInstances[id];
   auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId),*om=S9xRemasterFrameMetadataForTile(old,oi.tileId);
   auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},f,id),og=S9xRemasterResolveFrameGeometry(om?*om:RemasterFrameAssetMetadata{},old,id);
   check(oi.heightOffset==0 && i.heightOffset==24,"104 baked0 corrected24");
   check(!i.hasPlacementHeight && !f.contextGeometry.count(id+1) && g.hasHeight && g.hasNormals,"104 active intrinsic source");
   check(g.height==og.height && g.normalXyz==og.normalXyz && i.hFlip==oi.hFlip && i.vFlip==oi.vFlip,"104 immutable arrays/transforms");checked++;
  }
  int b=0;for(auto *pixels:{&f.mainPixels,&f.subPixels,&f.scenePixels}){for(auto &px:*pixels)if(px.instanceId){const auto &i=f.tileInstances[px.instanceId-1];if(i.source==RemasterSourceType::Object && (i.sourceIndex==48 || i.sourceIndex==49))counts[b]++;}b++;}
  check(checked==128 && counts==std::array<size_t,3>{264,0,264},"104 full OAM and visible scene denominator");
  std::cout<<"PASS exact104 128 follower records base0->24, main/scene264, intrinsic heights/normals immutable\n";
 }
 for(int slot=0;slot<128;slot++){const auto &o=c.terrainOam[slot];if(o.y<240)std::cout<<"OAM "<<slot<<" xy="<<o.x<<','<<int(o.y)<<" name="<<int(o.name)<<" flags="<<int(o.flags)<<" size="<<int(o.size)<<'\n';}
 std::ofstream out(std::string(argv[2])+".actors.jsonl");
 for(int which=0;which<2;which++){const auto &v=which?f:old;
  for(size_t id=0;id<v.tileInstances.size();id++){const auto &i=v.tileInstances[id];if(i.source!=RemasterSourceType::Object)continue;
   auto *m=S9xRemasterFrameMetadataForTile(v,i.tileId);auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},v,id);
   out<<"{\"replayed\":"<<which<<",\"id\":"<<id+1<<",\"slot\":"<<int(i.sourceIndex)<<",\"tile\":"<<i.tileNumber<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"base\":"<<i.heightOffset<<",\"hflip\":"<<i.hFlip<<",\"vflip\":"<<i.vFlip<<",\"palette\":"<<int(i.palette)<<",\"vram\":"<<i.vramAddress<<",\"placement\":"<<i.hasPlacementHeight<<",\"context\":"<<v.contextGeometry.count(id+1)<<",\"height\":"<<g.hasHeight<<",\"normals\":"<<g.hasNormals<<",\"local\":[";
   for(int p=0;p<64;p++){if(p)out<<',';out<<g.height[p];}out<<"],\"normal\":[";for(int p=0;p<192;p++){if(p)out<<',';out<<int(g.normalXyz[p]);}out<<"],\"buffers\":[";
   int b=0;for(auto *pixels:{&v.mainPixels,&v.subPixels,&v.scenePixels}){if(b++)out<<',';int x0=999,y0=999,x1=-1,y1=-1,n=0;for(size_t at=0;at<pixels->size();at++)if((*pixels)[at].instanceId==id+1){n++;x0=std::min(x0,int(at%v.width));x1=std::max(x1,int(at%v.width));y0=std::min(y0,int(at/v.width));y1=std::max(y1,int(at/v.width));}out<<"["<<n<<','<<x0<<','<<y0<<','<<x1<<','<<y1<<']';}out<<"]}\n";
  }
 }
 for(int y=12;y<21;y++)for(int x=29;x<35;x++){int at=y*64+x;const auto &m=s.runtimeDungeonHeightMap;std::cout<<"SUPPORT "<<x<<','<<y;for(int p=0;p<2;p++){const auto &d=m.roomDiagnostics->cells[p*4096+at];std::cout<<" p"<<p<<" base="<<m.planeOffsets[p][at]<<" floor="<<d.floorId<<" wall="<<d.wall<<" observed="<<d.planeObserved<<" conflict="<<d.heightConflict;}std::cout<<'\n';}
 return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
