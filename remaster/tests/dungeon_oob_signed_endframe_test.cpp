// Explicit research raster/maps with real ROM/VRAM; synthetic signed geometry
// exists only in this test's in-memory profile. No authored input is rewritten.
#define main originalResearchMain
#include "dungeon_out_of_bounds_end_to_end_test.cpp"
#undef main

int main(int argc,char **argv) {try {
 const char *input=argc==2?argv[1]:std::getenv("REMASTER_OOB_E2E_INPUT");
  if(!input){std::cout<<"SKIP signed EndFrame requires REMASTER_OOB_E2E_INPUT complete capture prefix\n";return 0;}
  for(bool research:{false,true}) {
  S9xRemasterSetDungeonResearchEnabled(research);
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int plane=0;plane<2;plane++)for(int conflict=0;conflict<2;conflict++) {
  auto f=std::make_unique<Research>(input,1,bg,flip,plane);
  auto &source=f->original.assets.at(f->decode(f->sheetWord).tileId);
  source.hasHeight=true;source.height.fill(0);source.height[27]=-16;
  source.hasNormals=true;source.normalXyz.fill(0); // authored valid zero bytes
  if(conflict)source.height[0]=-3; // breaks complete seam, not an unknown pixel
  const auto expected=source.height;const auto normals=source.normalXyz;
  auto frame=f->run(true,false,bg==0);auto &state=S9xRemasterState();
  const auto planes1=state.runtimeDungeonBg1HeightMap.planeOffsets,planes2=state.runtimeDungeonHeightMap.planeOffsets;
  RemasterLightingFieldCache fields;fields.update(*frame,true);
  int count=0;
  for(size_t at=0;at<frame->scenePixels.size();at++) {
   auto px=frame->scenePixels[at];if(!px.instanceId)continue;const auto &i=frame->tileInstances[px.instanceId-1];
   if(!(i.tileId==source.tileId))continue;
   auto effective=RemasterFrameChannelResolver(*frame).effective[px.instanceId-1];
   require(effective.hasHeight && effective.height==expected && effective.hasNormals && effective.normalXyz==normals,"EndFrame selected source/normal bytes preserved");
    if(!research)require(!i.visualClaim.published && !fields.samples[at].known && i.visualClaim.status==RemasterVisualClaimStatus::Unsupported,"default EndFrame nonflat is unresolved, not numeric or guessed conflict");
    else if(!conflict)require(i.visualClaim.published && fields.samples[at].known && std::abs(fields.height[at*2]*255-(47+expected[px.tilePixel]))<1e-5,"EndFrame nonflat independent crest47/local-16 source consumed");
   else require(!i.visualClaim.published && !fields.samples[at].known && i.visualClaim.status==RemasterVisualClaimStatus::Conflict,"EndFrame contradictory signed edge explicit conflict, no numeric fill");
   count++;
  }
  require(count==256,"EndFrame complete selected sheet denominator");
  std::vector<uint8_t> bytes;RemasterFrame replay;
  require(S9xSerializeRemasterFrame(*frame,bytes) && S9xDeserializeRemasterFrame(bytes,replay),"EndFrame signed capture/replay");
  RemasterLightingFieldCache replayFields;replayFields.update(replay,true);
  require(fields.height==replayFields.height && fields.surface==replayFields.surface && fields.authoredNormals==replayFields.authoredNormals,"EndFrame selected CPU height/normal replay parity");
  auto repeated=f->run(true,false,bg==0);
  require(planes1==state.runtimeDungeonBg1HeightMap.planeOffsets && planes2==state.runtimeDungeonHeightMap.planeOffsets,"signed terminal never alters physical support planes");
  }
  }
  S9xRemasterSetDungeonResearchEnabled(false);
  std::cout<<"PASS actual EndFrame signed OOB default/opt-in sources/conflicts BG/HV/planes 64 cases\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
