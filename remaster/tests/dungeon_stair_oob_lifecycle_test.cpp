// Own-input production reproduction. Controlled handler sequence, not a live
// transition recording. Strip ALL prior publication/placement overrides before
// every BeginFrame; a settled output clone is not a raw renderer occurrence.
#define ALTTP_TEST_CACHE_STAGES
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
#include "../runtime/lighting_field_cache.h"
int main(int argc,char **argv) {try {
 check(argc==3,"usage: own-capture output-prefix");
 check(captureFixtureMain(argc,argv)==0,"own-input authentication");
 const std::string path=argv[1];auto input=read(path+".dungeon-input.bin"),templates=read(path+".dungeon-templates.bin"),meta=read(path+".dungeon.json");
 const std::string text(meta.begin(),meta.end());
 auto number=[&](const char *key,int index=0){auto pos=text.find(std::string("\"")+key+"\":");check(pos!=std::string::npos,"state key");pos=text.find(':',pos)+1;if(text[pos]=='[')pos++;for(int i=0;i<index;i++)pos=text.find(',',pos)+1;return std::stoull(text.substr(pos));};
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.module=7;c.roomIndex=number("room");c.previousRoomIndex=number("previous_room");
 c.linkX=number("link_xy");c.linkY=number("link_xy",1);c.linkPlane=number("plane");c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
 c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");c.romIdentity=number("rom_identity");c.groundedEvidence=true;
 for(int l=0;l<2;l++){c.ppuMapBase[l]=number("ppu_map_base",l);c.ppuMapSize[l]=number("ppu_map_size",l);}
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
 check(S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c),"terrain");S9xRemasterDungeonStairsFromAttributes(c);
 auto captured=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(path,*captured),"packet");
 auto result=std::make_unique<RemasterFrame>();auto &s=S9xRemasterState();RemasterLightingFieldCache fields;
  auto run=[&](bool forceVisual=false){
   auto retained=forceVisual?std::move(s.visibleFaceTemplateCache):nullptr;
  S9xRemasterBeginFrame(captured->width*captured->height,captured->width,captured->width,captured->height);
  s.tileInstances=captured->tileInstances;
  for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=i.hasPlacementNormals=i.hasPlacementOcclusion=false;i.geometryStatus=RemasterOccurrenceGeometryStatus::Inherit;i.placementTileRole=RemasterTileRole::Unknown;i.visualClaim={};}
  for(const auto &a:captured->assets){auto &t=s.observedTiles[a.tileId.hash];t.hash=a.tileId.hash;t.bitDepth=a.tileId.bitDepth;std::copy_n(a.indices,64,t.indices);}
  for(size_t at=0;at<captured->mainPixels.size();at++) {
   s.mainOwners[at]=captured->mainPixels[at].owner;s.subOwners[at]=captured->subPixels[at].owner;
   s.mainInstanceIds[at]=captured->mainPixels[at].instanceId;s.subInstanceIds[at]=captured->subPixels[at].instanceId;
   s.mainTilePixels[at]=captured->mainPixels[at].tilePixel;s.subTilePixels[at]=captured->subPixels[at].tilePixel;
  }
  s.scenePixels=captured->scenePixels;s.sceneRgb=captured->sceneRgb555;
  const auto start=std::chrono::steady_clock::now();
  S9xRemasterEndFrame(captured->originalRgb555.data(),captured->width,captured->width,captured->height,result.get(),&c);
  const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  fields.update(*result,true);RemasterLightingFieldCache forced;forced.update(*result,true,true);
  check(fields.height==forced.height && fields.authoredNormals==forced.authoredNormals && fields.surface==forced.surface,"incremental/full CPU fields");
   if(forceVisual)s.visibleFaceTemplateCache=std::move(retained);
   return ms;
 };
 run();
 std::vector<size_t> solved,unknown;
 RemasterFrameHeightResolver initial(*result);
 for(size_t at=0;at<result->scenePixels.size();at++) {
  const auto px=S9xRemasterSelectVisibleOccurrence(*result,at,true,initial.alttp);if(!px.instanceId)continue;
  const auto &i=result->tileInstances[px.instanceId-1];
  if(i.placementTileRole==RemasterTileRole::OutOfBounds || (S9xRemasterFrameMetadataForTile(*result,i.tileId) && S9xRemasterFrameMetadataForTile(*result,i.tileId)->tileRole==RemasterTileRole::OutOfBounds))
   (i.visualClaim.published?solved:unknown).push_back(at);
 }
 check(!solved.empty(),"settled solved OOB denominator");
  const auto inventory=dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap);
  const auto owners1=s.runtimeDungeonBg1HeightMap.occupiedFloorOwners,owners2=s.runtimeDungeonHeightMap.occupiedFloorOwners;
  const std::vector<uint8_t> collision(c.collisionAttributes,c.collisionAttributes+8192);
 const auto at=solved.front();
 auto sample=[&](bool expected){
  RemasterFrameHeightResolver resolver(*result);const auto px=S9xRemasterSelectVisibleOccurrence(*result,at,true,resolver.alttp);check(px.instanceId!=0,"selected identity");
  const auto &i=result->tileInstances[px.instanceId-1];
  std::cout<<"sample handler="<<int(c.submodule)<<" phase="<<int(c.constructionPhase)<<" plane="<<int(c.linkPlane)<<" at="<<at<<" hash="<<std::hex<<i.tileId.hash<<std::dec<<" status="<<int(i.visualClaim.status)<<" published="<<i.visualClaim.published<<" base="<<i.heightOffset<<" cpu_height="<<fields.height[at*2]<<" cpu_known="<<fields.height[at*2+1]<<" normal="<<int(fields.authoredNormals[at])<<'\n';
  if(!std::getenv("ALTTP_OOB_BASELINE_OBSERVE"))check(i.visualClaim.published==expected,"stair lost published OOB claim");
  if(expected && i.visualClaim.published)check(i.visualClaim.status==RemasterVisualClaimStatus::Solved && i.heightOffset==45 && fields.authoredNormals[at] && fields.height[at*2+1]==1 && fields.height[at*2]==S9xRemasterSignedHeightField(45,0),"solved numerical/normal sample");
 };
 std::cout<<"settled selected_solved="<<solved.size()<<" selected_unknown="<<unknown.size()<<'\n';sample(true);
 for(int handler:{8,16}) {
  c.submodule=0;c.constructionPhase=0;run();sample(true);
  const auto epoch=s.runtimeDungeonHeightEpoch;
   const auto builds=s.visibleFaceTemplateCache->modelBuilds,hits=s.visibleFaceTemplateCache->modelHits;
   const auto graphs=s.visibleFaceTemplateCache->graphBuilds,oobs=s.visibleFaceTemplateCache->oobBuilds,solveHits=s.visibleFaceTemplateCache->preparationHits;
  S9xRemasterDungeonCacheStages()={};
  std::vector<double> times;
  for(int step=0;step<60;step++) {
   c.submodule=handler;c.constructionPhase=step?1:0;times.push_back(run());
   if(step<2)sample(true);
    check(inventory==dungeonMapInventory(s.runtimeDungeonBg1HeightMap)+dungeonMapInventory(s.runtimeDungeonHeightMap),"same-plane physical inventory changed");
    check(owners1==s.runtimeDungeonBg1HeightMap.occupiedFloorOwners && owners2==s.runtimeDungeonHeightMap.occupiedFloorOwners && std::equal(collision.begin(),collision.end(),c.collisionAttributes),"occupied owners/collision walkability changed");
    if(!std::getenv("ALTTP_OOB_BASELINE_OBSERVE"))for(auto pos:solved){const auto px=result->scenePixels[pos];check(px.instanceId && result->tileInstances[px.instanceId-1].visualClaim.published,"solved OOB sequence coverage lost");}
    std::vector<uint8_t> cachedBytes,forcedBytes;check(S9xSerializeRemasterFrame(*result,cachedBytes),"cached packet");
    run(true);check(S9xSerializeRemasterFrame(*result,forcedBytes) && cachedBytes==forcedBytes,"actual EndFrame cached/forced visual packet parity");
  }
  std::sort(times.begin(),times.end());
   std::cout<<"baseline_work handler="<<handler<<" frames=60 physical_publications="<<s.runtimeDungeonHeightEpoch-epoch<<" classifications="<<S9xRemasterDungeonCacheStages().classifications<<" physical_solves="<<S9xRemasterDungeonCacheStages().solves<<" candidate_builds="<<s.visibleFaceTemplateCache->modelBuilds-builds<<" candidate_hits="<<s.visibleFaceTemplateCache->modelHits-hits<<" endframe_median_ms="<<times[30]<<" endframe_p95_ms="<<times[57]<<'\n';
   std::cout<<"solution_work handler="<<handler<<" frames=60 graph_builds="<<s.visibleFaceTemplateCache->graphBuilds-graphs<<" oob_builds="<<s.visibleFaceTemplateCache->oobBuilds-oobs<<" hits="<<s.visibleFaceTemplateCache->preparationHits-solveHits<<" cache_allocations_total="<<s.visibleFaceTemplateCache->solutionAllocations<<" cache_bytes="<<sizeof(RemasterVisibleFaceSolutionCache)<<'\n';
   check(s.visibleFaceTemplateCache->graphBuilds==graphs && s.visibleFaceTemplateCache->oobBuilds==oobs && s.visibleFaceTemplateCache->preparationHits-solveHits==60,"static stairs must reuse full visual solution");
  c.submodule=0;c.constructionPhase=0;run();sample(true);
 }
  {
  const auto builds=s.visibleFaceTemplateCache->modelBuilds,hits=s.visibleFaceTemplateCache->modelHits;std::vector<double> times;
  for(int n=0;n<60;n++)times.push_back(run());
   std::sort(times.begin(),times.end());std::cout<<"settled_work frames=60 candidate_builds="<<s.visibleFaceTemplateCache->modelBuilds-builds<<" candidate_hits="<<s.visibleFaceTemplateCache->modelHits-hits<<" graphs_total="<<s.visibleFaceTemplateCache->graphBuilds<<" oobs_total="<<s.visibleFaceTemplateCache->oobBuilds<<" hits_total="<<s.visibleFaceTemplateCache->preparationHits<<" endframe_median_ms="<<times[30]<<" endframe_p95_ms="<<times[57]<<'\n';
 }
 check(S9xWriteRemasterFrame(*result,std::string(argv[2])+"-final.s9xrmf"),"production serialization");
 std::cout<<(std::getenv("ALTTP_OOB_BASELINE_OBSERVE")?"OBSERVATION ONLY (assertions disabled)":"PASS")<<" controlled own-input up/down OOB lifecycle\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
