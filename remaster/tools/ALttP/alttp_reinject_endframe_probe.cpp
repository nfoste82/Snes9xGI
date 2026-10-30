// Reinject immutable stock raster observations and run PRODUCTION EndFrame.
// Derived from the local room062 probe; current schema and channel checks are
// generic. Historical fixture-specific assertions below remain independent.
// No solver shortcuts, precomputed review maps or historical entry invention.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#include "../../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);check(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 check(argc==3 || argc==4,"usage: immutable-capture output-prefix [explicit-research-profile]");std::string path=argv[1],out=argv[2];
 auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 check(input.size()==90120 && templates.size()==12884,"complete inputs");
 std::string text(meta.begin(),meta.end());
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"state key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
 auto captured=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*captured),"packet");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;check(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"profile");
 std::string serialized;check(S9xRemasterSerializeProfile(profile,serialized,errors),"serialize");
 uint64_t digest=UINT64_C(14695981039346656037);for(unsigned char b:serialized)digest=(digest^b)*UINT64_C(1099511628211);
 std::ostringstream hex;hex<<std::hex<<digest;check(text.find("\"profile_serialized_fnv1a64\":\""+hex.str()+"\"")!=std::string::npos,"profile pin");
 const bool exact004=argc==3 && hex.str()=="f7ff2ac1566b0251" && number("room")==98;
 if(argc==4){check(S9xRemasterLoadProfile(argv[3],profile,errors),"explicit research profile");check(S9xRemasterSerializeProfile(profile,serialized,errors),"research serialization");}
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
 c.submodule=number("submodule");c.collisionMode=number("collision_mode");c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");
 c.romIdentity=number("rom_identity");c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
 c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
 for(int l=0;l<2;l++){c.ppuMapBase[l]=number("ppu_map_base",l);c.ppuMapSize[l]=number("ppu_map_size",l);}
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
  S9xRemasterDungeonStairsFromAttributes(c);
#ifdef REMASTER_ENDFRAME_STATIC_INPUTS
  REMASTER_ENDFRAME_STATIC_INPUTS(c);
#endif
  auto &s=S9xRemasterState();S9xRemasterSetProfile(profile);S9xRemasterSetLiveFramesEnabled(true);S9xRemasterInvalidateDungeonContinuity();
#ifdef REMASTER_ENDFRAME_PRODUCER_RESEARCH
 REMASTER_ENDFRAME_PRODUCER_RESEARCH(c);
#endif
 auto frame=std::make_unique<RemasterFrame>();
 double lastMs=0;
 const auto originalContext=c;
 if(std::getenv("ALTTP_BENCHMARK_SYNTHETIC_SCENE")){captured->scenePixels=captured->mainPixels;captured->sceneRgb555=captured->originalRgb555;}
 auto run=[&]() {
  S9xRemasterBeginFrame(captured->width*captured->height,captured->width,captured->width,captured->height);
  s.tileInstances=captured->tileInstances;
  // Incoming SCN1 IDs can reference placement/visual clones. Reinject their
  // stock identity, never yesterday's solved metadata. EndFrame owns rebuilding
  // all per-occurrence channels (including scene-only copies).
  for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;i.hasPlacementNormals=false;i.geometryStatus=RemasterOccurrenceGeometryStatus::Inherit;i.visualClaim={};}
  for(const auto &a:captured->assets){auto &t=s.observedTiles[a.tileId.hash];t.hash=a.tileId.hash;t.bitDepth=a.tileId.bitDepth;std::copy_n(a.indices,64,t.indices);}
  for(size_t at=0;at<captured->mainPixels.size();at++){
   s.mainOwners[at]=captured->mainPixels[at].owner;s.subOwners[at]=captured->subPixels[at].owner;
   s.mainInstanceIds[at]=captured->mainPixels[at].instanceId;s.subInstanceIds[at]=captured->subPixels[at].instanceId;
   s.mainTilePixels[at]=captured->mainPixels[at].tilePixel;s.subTilePixels[at]=captured->subPixels[at].tilePixel;
  }
  s.scenePixels=captured->scenePixels;s.sceneRgb=captured->sceneRgb555;
  const auto started=std::chrono::steady_clock::now();
   RemasterAreaContext area;area.id=c.roomIndex;
   // These captures have a common BG origin pinned by their complete PPU replay.
   // Reconstruct occurrence bindings using the actual EndFrame location path.
   std::array<int,4> scrollX,scrollY;scrollX.fill(c.backgroundScrollX);scrollY.fill(c.backgroundScrollY);
   S9xRemasterEndFrame(captured->originalRgb555.data(),captured->width,captured->width,captured->height,frame.get(),&c,nullptr,&area,0,nullptr,&scrollX,&scrollY);
  lastMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
  check(!frame->mainPixels.empty(),"production frame finalized");
 };
 if(const char *benchmark=std::getenv("ALTTP_ENDFRAME_BENCHMARK")) {
  const int count=std::max(10,std::atoi(benchmark));
  for(int mode=0;mode<4;mode++) {
   c=originalContext;
   std::vector<double> times;uint64_t epochs=0;run();
   for(int n=0;n<count+10;n++) {
    const auto epoch=s.runtimeDungeonHeightEpoch;
    if(mode==1){c.linkX++;c.backgroundScrollX++;}
    if(mode==2){c.hasTerrainActors=true;c.terrainActors[4]={uint16_t(c.linkX),uint16_t(c.linkY+5),10,2,uint8_t(13+n%4),c.linkPlane};}
    if(mode==3){s.dungeonSnapshotValid=false;s.runtimeDungeonHeightKey=0;} // bounded explicit cache-miss ablation, not gameplay
    run();epochs+=s.runtimeDungeonHeightEpoch-epoch;if(n>=10)times.push_back(lastMs);
   }
   std::sort(times.begin(),times.end());double sum=0;for(double v:times)sum+=v;
   std::cout<<"benchmark mode="<<mode<<" frames="<<count<<" median_ms="<<times[count/2]<<" p95_ms="<<times[count*95/100]<<" mean_ms="<<sum/count<<" epochs="<<epochs<<" instances="<<frame->tileInstances.size()<<" lights="<<frame->lights.size()<<" scene="<<frame->scenePixels.size()<<'\n';
  }
  return 0;
 }
 auto image=[&](const RemasterFrame &f,const std::string &suffix,const std::vector<RemasterFramePixel> &pixels){
  std::ofstream ppm(out+suffix+".ppm",std::ios::binary);ppm<<"P6\n"<<f.width<<' '<<f.height<<"\n255\n";
  for(const auto &px:pixels){int h=0;if(px.instanceId){const auto &i=f.tileInstances[px.instanceId-1];h=i.heightOffset;
   if(px.tilePixel<64){if(i.hasPlacementHeight)h+=i.placementHeight[px.tilePixel];else for(const auto &a:f.assetMetadata)if(a.tileId==i.tileId && a.hasHeight){h+=a.height[px.tilePixel];break;}}}
   char b=std::max(0,std::min(255,h*255/100));for(int k=0;k<3;k++)ppm.put(b);}
 };
 image(*captured,"-captured-main",captured->mainPixels);image(*captured,"-captured-sub",captured->subPixels);
 if(!captured->scenePixels.empty())image(*captured,"-captured-scene",captured->scenePixels);
 if(exact004)for(size_t at=0;at<captured->mainPixels.size();at++) {
  const auto &p=captured->mainPixels[at];if(!p.instanceId)continue;const auto &i=captured->tileInstances[p.instanceId-1];
  int x=((c.backgroundScrollX+at%captured->width)&511)/8,y=((c.backgroundScrollY+at/captured->width)&511)/8;
  if(i.source==RemasterSourceType::Background && i.sourceIndex==1 && y==35 && x>=23 && x<=25)check(i.heightOffset==0,"004 original symptom changed");
 }
  run();const auto epoch=s.runtimeDungeonHeightEpoch;
  // Inventory every occurrence in the complete room, not only the previously
  // reported corner or currently visible pixels. Both gameplay planes remain
  // independent from BG layer and visible offset selection.
  for(int bg=0;bg<2;bg++) {
   auto maps=std::make_unique<std::array<uint8_t,16384>>();std::copy_n(c.constructedMaps,16384,maps->begin());
   auto cc=c;if(bg==0){std::rotate(maps->begin(),maps->begin()+8192,maps->end());std::swap(cc.graphicsBase,cc.bg1GraphicsBase);}
   cc.constructedMaps=maps->data();cc.placementBackgroundIndex=bg;
   auto placements=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();S9xRemasterDungeonPlacements(cc,profile,*placements);
   const auto &hm=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
   if(bg && argc==4) {
    const int corner=7*64+9;
    check(placements->at(corner).terminalCorner && hm.offsets[corner]==16 &&
      hm.structuralPlaneOffsets[0][corner]==16 && hm.structuralPlaneOffsets[1][corner]==255,"curved high-corner production ownership");
   }
   for(int at=0;at<4096;at++)if(hm.producerHashes[at]==UINT64_C(0xee78ea413516533b)) {
    const auto &t=placements->at(at);std::cout<<"corner BG"<<bg+1<<" xy="<<at%64<<','<<at/64<<" terminal="<<t.terminalCorner<<" template="<<t.wallTemplateOffset<<" flips="<<t.hflip<<t.vflip<<" visible="<<hm.offsets[at]<<" planes="<<hm.structuralPlaneOffsets[0][at]<<','<<hm.structuralPlaneOffsets[1][at]<<" attrs="<<int(c.collisionAttributes[at])<<','<<int(c.collisionAttributes[4096+at])<<'\n';
    for(int delta:{-64,-1,1,64})if(at+delta>=0 && at+delta<4096){const auto &u=placements->at(at+delta);std::cout<<" neighbor "<<delta<<" surface="<<int(u.surface)<<" template="<<u.wallTemplateOffset<<" high="<<u.highPerimeterFace<<" base="<<hm.structuralPlaneOffsets[0][at+delta]<<','<<hm.structuralPlaneOffsets[1][at+delta]<<" attr="<<int(c.collisionAttributes[at+delta])<<','<<int(c.collisionAttributes[4096+at+delta])<<'\n';}
   }
  }
 if(exact004){const auto &m=s.runtimeDungeonHeightMap;check(m.unknownPlaneCells[0]==0 && m.unknownPlaneCells[1]==0,"004 actual supports unresolved");check(m.planeOffsets[0][35*64+24]==24 && m.planeOffsets[1][19*64+35]==0,"004 production upper/lower split");}
 if(argc==3 && hex.str()=="dbe975eeca6fbe31" && c.roomIndex==98) {
  const auto &m=s.runtimeDungeonHeightMap;
  for(int y=43;y<=45;y++)for(int x=40;x<=41;x++)check(m.planeOffsets[0][y*64+x]==24 && m.planeOffsets[1][y*64+x]==0,"016 persistent pot support planes");
  for(int y=4;y<=6;y++)for(int x:{30,33}) {
   bool owned=false;for(const auto &a:m.doorframeAttachments)if(a.cell==y*64+x && a.plane==0 && a.visible && a.base==24)owned=true;
   check(owned,"016 complete north-door frame attachment");
  }
  for(auto xy:{std::array<int,2>{14,59},{17,59},{14,60},{17,60},{14,61},{15,61},{16,61},{17,61}}) {
   bool owned=false;for(const auto &a:m.doorframeAttachments)if(a.cell==xy[1]*64+xy[0] && a.plane==0 && a.visible && a.base==24)owned=true;
   check(owned,"016 complete south-door frame attachment");
  }
  size_t samples=0;
  for(const auto *pixels:{&frame->mainPixels,&frame->subPixels})for(size_t at=0;at<pixels->size();at++) {
   const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
   if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1)continue;
   const int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8;
   if(y<4 || y>6 || x<30 || x>33)continue;
   for(const auto &a:m.doorframeAttachments)if(a.cell==y*64+x && a.visible && a.base==24) {
    check(i.heightOffset==((a.mask&(UINT64_C(1)<<px.tilePixel))?24:0) && !i.hasPlacementHeight,"016 effective frame/opening application");samples++;break;
   }
  }
  check(samples>0,"016 north door not observed");std::cout<<"016 effective north-door canonical pixel samples="<<samples<<'\n';
 }
 if(exact004)for(size_t at=0;at<frame->mainPixels.size();at++) {
  const auto &p=frame->mainPixels[at];if(!p.instanceId)continue;const auto &i=frame->tileInstances[p.instanceId-1];
  int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8;
  if(i.source==RemasterSourceType::Background && i.sourceIndex==1 && y==35 && x>=23 && x<=25)check(i.heightOffset==24,"004 consumed upper walkway still zero");
 }
 auto first=std::make_unique<RemasterFrame>(*frame);run();check(s.runtimeDungeonHeightEpoch==epoch,"unchanged frame must reuse cache");
 check(frame->originalRgb555==captured->originalRgb555,"stock color changed");
 for(size_t n=0;n<captured->tileInstances.size();n++){const auto &a=captured->tileInstances[n],&b=frame->tileInstances[n];check(a.hasPaletteArtwork==b.hasPaletteArtwork && a.paletteRgb555==b.paletteRgb555,"palette artwork changed");}
 check(first->tileInstances.size()==frame->tileInstances.size(),"cache instance count");
 for(size_t n=0;n<first->tileInstances.size();n++){const auto &a=first->tileInstances[n],&b=frame->tileInstances[n];check(a.heightOffset==b.heightOffset && a.hasPlacementHeight==b.hasPlacementHeight && a.placementHeight==b.placementHeight,"cache changed consumed heights");}
 image(*frame,"-production-main",frame->mainPixels);image(*frame,"-production-sub",frame->subPixels);
 if(!frame->scenePixels.empty())image(*frame,"-production-scene",frame->scenePixels);
  check(S9xWriteRemasterFrame(*frame,out+".s9xrmf"),"packet write");
  // Signed complete-room review map, explicitly NOT a publishable export.
  {std::vector<uint8_t> bytes={'A','L','T','P','H','M','3',0};
   RemasterFrameSerialization::U16(bytes,c.roomIndex);
   const auto &hm=s.runtimeDungeonHeightMap;
   for(const auto *array:{&hm.offsets,&hm.planeOffsets[0],&hm.planeOffsets[1]})
    for(int32_t value:*array)RemasterFrameSerialization::U32(bytes,static_cast<uint32_t>(value));
   std::ofstream mapfile(out+"-review-map.bin",std::ios::binary);mapfile.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());}
  auto roundtrip=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(out+".s9xrmf",*roundtrip),"packet readback");
   check(roundtrip->schemaVersion==REMASTER_FRAME_SCHEMA_VERSION && roundtrip->tileInstances.size()==frame->tileInstances.size(),"current schema preservation");
  for(size_t n=0;n<frame->tileInstances.size();n++)check(roundtrip->tileInstances[n].heightOffset==frame->tileInstances[n].heightOffset &&
     roundtrip->tileInstances[n].placementHeight==frame->tileInstances[n].placementHeight,"effective serialization mismatch");
   // Compare the full canonical wire packet, not just offsets. This covers
   // selected scene IDs, masks, normals, channel presence and contextual layers.
   std::vector<uint8_t> wire,replayed;
   check(S9xSerializeRemasterFrame(*frame,wire) && S9xSerializeRemasterFrame(*roundtrip,replayed) && wire==replayed,"full scene/channel roundtrip preservation");
   std::ofstream placements(out+"-placements.jsonl");
   if(s.visibleFaceTemplateCache) {
    std::ofstream seams(out+"-oob-terminals.jsonl");
    for(const auto &row:s.visibleFaceTemplateCache->diagnosticTerminals) {
     seams<<'[';for(size_t k=0;k<row.size();k++){if(k)seams<<',';seams<<row[k];}seams<<"]\n";
    }
   }
   for(int bg=0;bg<2;bg++) {
    const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
    for(int cell=0;cell<4096;cell++) {
     const auto &t=tiles[cell];int at=(bg?0:8192)+2*cell,word=c.constructedMaps[at]|c.constructedMaps[at+1]<<8;
     placements<<"{\"bg\":"<<bg+1<<",\"cell\":"<<cell<<",\"word\":"<<word<<",\"surface\":"<<int(t.surface)<<",\"ambiguous_source\":"<<t.stairSourceAmbiguous
      <<",\"door_assembly\":"<<t.doorAssembly<<",\"door_template\":"<<t.doorTemplateOffset<<",\"door_direction\":"<<int(t.doorDirection)
      <<",\"door_column\":"<<t.doorwayColumnContact<<",\"support\":"<<t.doorwaySupport<<",\"support_direction\":"<<int(t.doorwaySupportDirection)<<"}\n";
    }
   }
 std::ofstream report(out+".json");const auto &m=s.runtimeDungeonHeightMap;
 report<<"{\"room\":"<<c.roomIndex<<",\"profile_pin\":\""<<hex.str()<<"\",\"epoch\":"<<epoch<<",\"main_pixels\":"<<frame->mainPixels.size()<<",\"scene_pixels\":"<<frame->scenePixels.size()<<",\"required\":["<<m.requiredPlaneCells[0]<<','<<m.requiredPlaneCells[1]<<"],\"missing\":["<<m.unknownPlaneCells[0]<<','<<m.unknownPlaneCells[1]<<"],\"walls\":["<<m.solvedWallCells<<','<<m.unknownWallCells<<"],\"conflicts\":"<<m.structuralConflicts<<",\"samples\":[";
 bool comma=false;for(auto *pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(size_t at=0;at<pixels->size();at++){
  const auto &px=(*pixels)[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
  if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
  int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8;
  if((x==23 && y==35) || (x==24 && y==35) || (x==25 && y==35)) {
   if(comma)report<<',';comma=true;report<<"{\"buffer\":"<<(pixels==&frame->mainPixels?0:pixels==&frame->subPixels?1:2)<<",\"bg\":"<<int(i.sourceIndex)<<",\"xy\":["<<x<<','<<y<<"],\"base\":"<<i.heightOffset<<'}';
  }
 }report<<"]}\n";
 // Plane change and camera movement are distinct lifecycle events. Solve must
 // retain signed floors; camera alone must never rebuild room geometry.
  c.linkPlane^=1;run();check(s.runtimeDungeonHeightEpoch>epoch,"plane rebuild");
  c.linkPlane^=1;run();auto planeEpoch=s.runtimeDungeonHeightEpoch;c.backgroundScrollX++;run();check(s.runtimeDungeonHeightEpoch==planeEpoch,"camera cache");
  if(argc==4) {
   // Explicit synthetic stock observations at the OFFSCREEN missed placement,
   // not a new gameplay capture. Exercise the actual EndFrame scene swap,
   // main/sub application, transformed tilePixel and authored effective source.
   c.backgroundScrollX=9*8;c.backgroundScrollY=7*8;
   RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
   instance.tileId={UINT64_C(0xee78ea413516533b),1,4};
   captured->tileInstances.push_back(instance);const uint32_t id=captured->tileInstances.size();
   captured->scenePixels=captured->mainPixels;captured->sceneRgb555=captured->originalRgb555;
   for(auto *pixels:{&captured->mainPixels,&captured->subPixels,&captured->scenePixels})for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    auto &px=pixels->at(y*captured->width+x);px.instanceId=id;px.tilePixel=y*8+x;
   }
   run();check(s.runtimeDungeonHeightEpoch==planeEpoch,"offscreen camera caused solve");
   for(auto *pixels:{&frame->mainPixels,&frame->subPixels,&frame->scenePixels})for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    const auto &px=pixels->at(y*frame->width+x);const auto &i=frame->tileInstances.at(px.instanceId-1);
    const auto *a=S9xRemasterFrameMetadataForTile(*frame,i.tileId);
    check(i.heightOffset==16 && !i.hasPlacementHeight && a && a->hasHeight &&
      i.heightOffset+a->height[px.tilePixel]==16+std::max(7-x,7-y),"offscreen production effective source");
   }
  }
 //004 has no SCN1, so preserve that fact in artifacts. Independently exercise
 // the production scene path using the captured stock raster, explicitly
 // synthetic, to reject scene-only height omissions without fabricating GI.
 if(exact004 && captured->scenePixels.empty()) {
  c.backgroundScrollX--;captured->scenePixels=captured->mainPixels;captured->sceneRgb555=captured->originalRgb555;
  run();check(frame->scenePixels.size()==captured->mainPixels.size(),"synthetic production SCN1");
  for(size_t at=0;at<frame->scenePixels.size();at++) {
   const auto &p=frame->scenePixels[at];if(!p.instanceId)continue;const auto &i=frame->tileInstances[p.instanceId-1];
   int x=((c.backgroundScrollX+at%frame->width)&511)/8,y=((c.backgroundScrollY+at/frame->width)&511)/8;
   if(i.source==RemasterSourceType::Background && i.sourceIndex==1 && y==35 && x>=23 && x<=25)check(i.heightOffset==24,"production scene upper support");
  }
  image(*frame,"-synthetic-production-scene",frame->scenePixels);
 }
 std::string after;check(S9xRemasterSerializeProfile(s.activeProfile,after,errors) && after==serialized,"authored profile changed");
 std::cout<<"production EndFrame room "<<c.roomIndex<<" missing "<<m.unknownPlaneCells[0]<<'/'<<m.unknownPlaneCells[1]<<" walls "<<m.solvedWallCells<<'/'<<m.unknownWallCells<<" scene "<<first->scenePixels.size()<<" cache/plane/camera PASS\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
