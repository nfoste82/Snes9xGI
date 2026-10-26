// Exact own-input production replay and complete placed-doorway inventory.
// Coordinates select no runtime behavior; every doorway on both BGs is reported.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) { try {
 check(captureFixtureMain(argc,argv)==0,"exact replay");
 auto &s=S9xRemasterState();
 auto packet=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"replayed packet");
 // Geometry lives in the unoccluded scene buffer when present. Main is the
 // unchanged stock presentation and is not an effective-height denominator.
 const auto &geometry=packet->scenePixels.empty()?packet->mainPixels:packet->scenePixels;
 std::ofstream pixels(std::string(argv[2])+".pixels.csv");pixels<<"bg,x,y,offset,placement,min,max,count\n";
 std::map<std::tuple<int,int,int,int,bool,int,int>,int> uses;
 const auto meta=read(std::string(argv[1])+".dungeon.json");const std::string text(meta.begin(),meta.end());
 auto pos=text.find("\"scroll\":");pos=text.find('[',pos);int sx=std::atoi(text.c_str()+pos+1);pos=text.find(',',pos);int sy=std::atoi(text.c_str()+pos+1);
 size_t checked=0;
 for(size_t at=0;at<geometry.size();at++) {
  const auto &px=geometry[at];if(!px.instanceId)continue;const auto &t=packet->tileInstances[px.instanceId-1];
  if(t.source!=RemasterSourceType::Background || t.sourceIndex>1)continue;
  const int cell=(((sy+at/packet->width)&511)/8)*64+((sx+at%packet->width)&511)/8;
  const auto &map=t.sourceIndex?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  if(map.producerOwnedWall[cell])continue;
  int expected=255;bool conflict=false;
  for(const auto &a:map.doorframeAttachments)if(a.cell==cell && a.visible){int value=(a.mask&(UINT64_C(1)<<px.tilePixel))?a.base:a.openingBase;if(expected!=255 && expected!=value)conflict=true;expected=value;}
  if(expected==255 || conflict)continue;
  check(t.heightOffset==expected && !t.hasPlacementHeight,"effective doorway geometry differs from complete-room attachment");checked++;
 }
 std::cout<<"PASS effective scene doorway pixels="<<checked<<" (main-only stock pixels excluded)\n";
 for(const auto *buffer:{&packet->mainPixels,&packet->subPixels,&packet->scenePixels})for(size_t at=0;at<buffer->size();at++) {
  const auto &px=(*buffer)[at];if(!px.instanceId)continue;const auto &t=packet->tileInstances[px.instanceId-1];
  if(t.source!=RemasterSourceType::Background || t.sourceIndex>1)continue;
  int x=((sx+at%packet->width)&511)/8,y=((sy+at/packet->width)&511)/8;
  const auto &placed=(t.sourceIndex?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[y*64+x];
  if(placed.surface!=RemasterDungeonFloorContext::Surface::Door && placed.surface!=RemasterDungeonFloorContext::Surface::Wall)continue;
  int lo=0,hi=0;for(const auto &m:packet->assetMetadata)if(m.tileId==t.tileId){lo=*std::min_element(m.height.begin(),m.height.end());hi=*std::max_element(m.height.begin(),m.height.end());}
  if(t.hasPlacementHeight){lo=*std::min_element(t.placementHeight.begin(),t.placementHeight.end());hi=*std::max_element(t.placementHeight.begin(),t.placementHeight.end());}
  uses[{t.sourceIndex+1,x,y,t.heightOffset,t.hasPlacementHeight,lo,hi}]++;
 }
 for(const auto &u:uses){const auto &[bg,x,y,o,p,lo,hi]=u.first;pixels<<bg<<','<<x<<','<<y<<','<<o<<','<<p<<','<<lo<<','<<hi<<','<<u.second<<'\n';}
 std::ofstream f(std::string(argv[2])+".doorways.csv");
 f<<"bg,x,y,hash,assembly,source,direction,split,mask_present,mask,support,hflip,vflip,local_min,local_max,map_base,plane0,plane1,attachment0,attachment1,opening0,opening1\n";
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  for(int i=0;i<4096;i++) { const auto &t=tiles[i];
   if(t.surface!=RemasterDungeonFloorContext::Surface::Door && !t.doorAssembly)continue;
   int base[2]={255,255},opening[2]={255,255};
   for(const auto &a:m.doorframeAttachments)if(a.cell==i && a.visible){base[a.plane]=a.base;opening[a.plane]=a.openingBase;}
   f<<bg+1<<','<<i%64<<','<<i/64<<','<<std::hex<<t.tileHash<<std::dec<<','<<t.doorAssembly<<','<<t.doorTemplateOffset<<','<<int(t.doorDirection)<<','<<t.splitDoorPart<<','<<t.hasDoorframeMask<<','<<t.doorframeMask<<','<<t.doorwaySupport<<','<<t.hflip<<','<<t.vflip<<','<<*std::min_element(t.height.begin(),t.height.end())<<','<<*std::max_element(t.height.begin(),t.height.end())<<','<<m.offsets[i]<<','<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]<<','<<base[0]<<','<<base[1]<<','<<opening[0]<<','<<opening[1]<<'\n';
  }
 }
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} }
