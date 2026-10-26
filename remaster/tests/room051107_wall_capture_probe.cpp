// Own-input production replay and complete placement inventory; no fabricated support.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
#include "../runtime/lighting_field_cache.h"
int main(int argc,char **argv){try {
 check(captureFixtureMain(argc,argv)==0,"production replay");
 const auto &s=S9xRemasterState();
 auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*f),"output packet");
 auto original=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[1],*original),"original packet");
 for(const auto &a:original->assetMetadata) {
  const auto *b=S9xRemasterFrameMetadataForTile(*f,a.tileId);check(b,"preserved asset identity");
  check(a.hasHeight==b->hasHeight && a.height==b->height && a.hasNormals==b->hasNormals && a.normalXyz==b->normalXyz,"all authored height/normal sources preserved");
 }
 RemasterLightingFieldCache fields;fields.update(*f,true,true);
 size_t targets=0,pixels=0;std::set<std::pair<int,int>> positions;
 std::ofstream o(std::string(argv[2])+".walls.jsonl");
 for(int bg=0;bg<2;bg++) {
  const auto &tiles=bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1;
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  for(int i=0;i<4096;i++) {const auto &t=tiles[i];
   if(t.tileHash==UINT64_C(0xa2af9baf8d303df7) || t.tileHash==UINT64_C(0xf91826cad69954c9)) {
    // Pin the observed defect, not a guessed repair: support IS independently
    // solved; the authored zero-datum member ramps are different from the
    // counterpart's 4..11/12..19. Never change arrays to pass this probe.
    check(bg==1 && t.authoredObstacleAssembly && !t.authoredObstacleContact && t.authoredObstacleDirection==2,"target complete west ownership");
    check(m.offsets[i]==24 && m.planeOffsets[1][i]==0 && m.planeOffsets[0][i]==255,"target structural base versus independent hidden support");
    for(int k=0;k<64;k++)check(t.height[k]==k%8,"intrinsic target ramp");
    targets++;positions.insert({i%64,i/64});
   }
   o<<"{\"bg\":"<<bg+1<<",\"x\":"<<i%64<<",\"y\":"<<i/64<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"surface\":"<<int(t.surface)<<",\"hf\":"<<t.hflip<<",\"vf\":"<<t.vflip
    <<",\"height\":"<<t.hasHeight<<",\"lower\":"<<t.distinctLowerUsage<<",\"mask\":"<<int(t.floorPlaneMask)<<",\"assembly\":"<<t.authoredObstacleAssembly<<",\"contact\":"<<t.authoredObstacleContact<<",\"direction\":"<<int(t.authoredObstacleDirection)<<",\"stack\":"<<t.wallStackAssembly<<",\"base\":"<<m.offsets[i]
    <<",\"plane\":["<<m.planeOffsets[0][i]<<','<<m.planeOffsets[1][i]<<"],\"structural\":["<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]<<"],\"local\":[";
   for(int k=0;k<64;k++)o<<(k?",":"")<<t.height[k];o<<"]}\n";
  }
  for(const auto &e:m.authoredObstacleEvidence)std::cout<<"obstacle bg="<<bg+1<<" assembly="<<e.assembly<<" plane="<<e.plane<<" component="<<e.component<<" base="<<e.floorBase<<" contacts="<<e.witnessedContacts<<'/'<<e.contacts<<" conflict="<<e.conflict<<'\n';
 }
 for(size_t at=0;at<f->scenePixels.size();at++) {
  const auto &px=f->scenePixels[at];if(!px.instanceId)continue;
  const auto &i=f->tileInstances.at(px.instanceId-1);
  if(i.tileId.hash!=UINT64_C(0xa2af9baf8d303df7) && i.tileId.hash!=UINT64_C(0xf91826cad69954c9))continue;
  const auto *a=S9xRemasterFrameMetadataForTile(*f,i.tileId);check(a,"target metadata");
  const auto g=S9xRemasterResolveFrameGeometry(*a,*f,px.instanceId-1);
  check(i.sourceIndex==1 && i.heightOffset==24 && !i.hasPlacementHeight && !f->contextGeometry.count(px.instanceId),"actual scene source/base");
  check(g.hasHeight && g.hasNormals && g.height==a->height && g.normalXyz==a->normalXyz,"immutable intrinsic geometry");
  check(fields.height[at*2+1] && fields.authoredNormals[at] && fields.height[at*2]==S9xRemasterSignedHeightField(24,a->height[px.tilePixel]),"actual lighting consumption");pixels++;
 }
 check(targets==12 && positions.size()==12 && pixels==768,"complete room and exact scene target denominators");
 std::cout<<"PASS own107: 12 complete members / 768 scene pixels; plane0 support24, hidden plane1 support0; immutable intrinsic0..7; missing member datum NOT guessed\n";
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
