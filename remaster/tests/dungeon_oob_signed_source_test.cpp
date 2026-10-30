// Failure classes: inapplicable named contexts and signed nonflat terminal seams.
#define REMASTER_OOB_NO_MAIN 1
#include "dungeon_out_of_bounds_test.cpp"

static void contextApplicability() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 for(int y=0;y<8;y++)(*gfx)[y*2]=255;
 uint8_t pixels[64];std::fill_n(pixels,64,1);
 RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
 RemasterProfile p;p.assets[id].tileRole=RemasterTileRole::OutOfBounds;
 p.assets[id].hasHeight=true;p.assets[id].height.fill(0);
 p.tileContexts[id]["a"].hasHeight=true;p.tileContexts[id]["b"].hasHeight=true;
 RemasterContextLocation loc;loc.area=320;loc.background=1;
 p.contextBindings[loc]={id,"a"};
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=8;c.placementBackgroundIndex=1;
 c.constructedMaps=maps->data();c.graphics=gfx->data();
 auto placed=std::make_unique<std::array<C::Placement,4096>>();
 S9xRemasterDungeonPlacements(c,p,*placed);
 need(!placed->at(0).stairSourceAmbiguous,"foreign-area context bindings must not veto actual default source");
 loc.area=8;p.contextBindings[loc]={id,"a"};
 S9xRemasterDungeonPlacements(c,p,*placed);
 need(placed->at(0).stairSourceAmbiguous && !placed->at(1).stairSourceAmbiguous,"only matching occurrence binding can replace default");
 loc.background=0;p.contextBindings.clear();p.contextBindings[loc]={id,"b"};
 S9xRemasterDungeonPlacements(c,p,*placed);
 need(!placed->at(0).stairSourceAmbiguous,"peer BG binding cannot replace source");
 p.tileContexts[id].erase("b");
 S9xRemasterDungeonPlacements(c,p,*placed);
 need(placed->at(0).stairSourceAmbiguous,"single context is globally selected even without binding");
 std::cout<<"PASS context applicability\n";
}

static void signedSeams() {
  // Ordinary production must not infer contact from opaque adjacency. This
  // fixture used to publish a new numeric guess even without research opt-in.
  {auto f=std::make_unique<PlateauFixture>(0,0,0,0);f->local(-16,true);f->run();
   for(auto member:f->added) {
    const auto &i=f->frame.tileInstances[f->frame.scenePixels[f->at(member.first)].instanceId-1];
    need(!i.visualClaim.published && i.visualClaim.status==RemasterVisualClaimStatus::Unsupported,
      "default nonflat OOB must remain explicitly unsupported, not numeric");
   }}
#ifdef REMASTER_DUNGEON_RESEARCH_CONTAINMENT
  S9xRemasterSetDungeonResearchEnabled(true);
#endif
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int plane=0;plane<2;plane++)for(int bad=0;bad<2;bad++) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,plane);
  for(auto member:f->added) {
   int cell=member.first;auto &t=f->tiles[bg][cell];t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    int screen=axis?y:x;
    t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)]=-16+2*screen;
   }
   if(bad && cell==f->plateau) { // One contradictory paired contact, not a new datum.
    int x=axis?3:(f->down>0?7:0),y=axis?(f->down>0?7:0):3;
    t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)]++;
   }
   auto &inst=f->frame.tileInstances[member.second-1];inst.hFlip=t.hflip;inst.vFlip=t.vflip;
   for(auto &m:f->frame.assetMetadata)if(m.tileId==inst.tileId){m.hasHeight=true;m.height=t.height;m.hasNormals=true;m.normalXyz.fill(0);}
   auto &map=f->maps;int at=(bg?0:8192)+cell*2;map[at+1]=(flip&1?0x40:0)|(flip&2?0x80:0);
   for(auto *r:{&f->frame.mainPixels,&f->frame.scenePixels})for(auto &px:*r)if(px.instanceId==member.second) {
    int x=px.tilePixel%8,y=px.tilePixel/8;px.tilePixel=(t.vflip?7-y:y)*8+(t.hflip?7-x:x);
   }
  }
  const auto physical=f->heights[bg].planeOffsets;
  f->run();RemasterLightingFieldCache fields;fields.update(f->frame,true);
  for(auto member:f->added) {
   auto px=f->frame.scenePixels[f->at(member.first)];const auto &i=f->frame.tileInstances[px.instanceId-1];
   need(i.visualClaim.status==(bad?RemasterVisualClaimStatus::Conflict:RemasterVisualClaimStatus::Solved),"signed nonflat contact equations must solve or explicitly conflict");
   need(i.visualClaim.published==!bad,"conflicting seam must never publish numeric height");
   if(!bad) {
    int contact=f->down>0?7:0,local=-16+2*contact;
    // Moving away from the wall: near local -16 meets far local -2
    // for +axis, hence far base = near base -14 (reverse: +14).
    int relative=member.first==f->plateau?0:(f->down>0?-14:14);
    need(i.heightOffset==77-local+relative,"one placement base per signed paired seam; never uniform crest fill");
    auto effective=RemasterFrameChannelResolver(f->frame).effective[px.instanceId-1];
    need(effective.hasNormals && effective.normalXyz==std::array<uint8_t,192>{},"valid authored zero normals preserved");
    std::vector<uint8_t> wire;RemasterFrame replay;
    need(S9xSerializeRemasterFrame(f->frame,wire) && S9xDeserializeRemasterFrame(wire,replay),"signed source serialization");
    RemasterLightingFieldCache replayFields;replayFields.update(replay,true);
    need(replayFields.height==fields.height,"selected signed CPU source survives replay");
   }
  }
  need(physical==f->heights[bg].planeOffsets,"visual terminal cannot alter physical planes");
 }
  std::cout<<"PASS signed seams 128 transformed/BG/plane/contact cases\n";
#ifdef REMASTER_DUNGEON_RESEARCH_CONTAINMENT
  S9xRemasterSetDungeonResearchEnabled(false);
#endif
}
int main(int argc,char **argv) {
 if(argc<2 || std::string(argv[1])=="context")contextApplicability();
 if(argc<2 || std::string(argv[1])=="seams")signedSeams();
}
