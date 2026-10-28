// Compile with -DREMASTER_OOB_REPLAY_SOURCE='"/absolute/pinned/probe.cpp"'.
// The replay adapter must run the actual classifier/EndFrame and leave its
// diagnostic cache in S9xRemasterState. No capture or profile is modified.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#define main pinnedReplayMain
#include REMASTER_OOB_REPLAY_SOURCE
#undef main
int main(int argc,char **argv) {
 const int rc=pinnedReplayMain(argc,argv);if(rc)return rc;
 const auto &s=S9xRemasterState();const auto &cache=*s.visibleFaceTemplateCache;
 std::ofstream events(std::string(argv[2])+".oob-events.jsonl");
 for(const auto &e:cache.diagnosticPlateaus)
  events<<"["<<e.kind<<','<<e.component<<','<<e.a<<','<<e.b<<','<<e.d<<','<<e.status<<','<<e.height<<','<<e.owner<<','<<e.plane<<"]\n";
#ifdef REMASTER_OOB_CREST_RANGE_DIAGNOSTICS
 std::ofstream ranges(std::string(argv[2])+".oob-ranges.jsonl");
 for(const auto &e:cache.diagnosticPlateaus)if(e.kind==3)
  ranges<<'['<<e.component<<','<<e.a<<','<<e.crestMin<<','<<e.crestMax<<','<<e.crestCount<<"]\n";
#endif
 std::ofstream terminals(std::string(argv[2])+".oob-terminals.jsonl");
 for(const auto &e:cache.diagnosticTerminals) {
  terminals<<'[';for(size_t k=0;k<e.size();k++){if(k)terminals<<',';terminals<<e[k];}terminals<<"]\n";
 }
 std::ofstream joins(std::string(argv[2])+".oob-joins.jsonl");
 for(const auto &e:cache.diagnosticSheetJoins) {
  joins<<'[';for(size_t k=0;k<e.size();k++){if(k)joins<<',';joins<<e[k];}joins<<"]\n";
 }
 std::ofstream equations(std::string(argv[2])+".wall-equations.jsonl");
 const auto &eq=cache.diagnosticEquations;
 for(size_t n=0;n<cache.diagnosticCells.size();n++) {
  auto c=cache.diagnosticCells[n];equations<<'['<<n<<','<<c.first<<','<<c.second<<','<<eq.relative[n]<<','<<eq.datum[n]<<','<<eq.invalid[n]<<','<<eq.cycle[n]<<','<<eq.anchorConflict[n]<<"]\n";
 }
 std::ofstream cells(std::string(argv[2])+".oob-cells.jsonl");
 for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
  const auto &t=(bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[i];
  const auto &m=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
  cells<<"{\"bg\":"<<bg<<",\"cell\":"<<i<<",\"surface\":"<<int(t.surface)
   <<",\"hash\":"<<t.tileHash<<",\"flips\":["<<t.hflip<<','<<t.vflip<<"],\"has_height\":"<<t.hasHeight
   <<",\"ambiguous\":"<<t.stairSourceAmbiguous<<",\"physical\":"<<m.offsets[i]
   <<",\"structural\":["<<m.structuralPlaneOffsets[0][i]<<','<<m.structuralPlaneOffsets[1][i]
   <<"],\"owners\":["<<m.occupiedFloorOwners[0][i]<<','<<m.occupiedFloorOwners[1][i]
   <<"],\"producer\":"<<m.producerHashes[i]<<",\"wall_source\":"<<t.wallTemplateOffset
   <<",\"height\":[";
  for(int k=0;k<64;k++){if(k)cells<<',';cells<<t.height[k];}cells<<"]}\n";
  }
  if(std::getenv("REMASTER_OOB_ASSERT_MINIMUM")) {
    // Independent oracle: reduce every actual transformed absolute band sample,
    // not the callback's reduced output or the sheet's published base.
    std::map<int,int> minimum,maximum,counts;
    for(const auto &e:cache.diagnosticPlateaus)if(e.kind==1 && e.status==int(RemasterVisualClaimStatus::Solved)) {
     auto terminal=std::find_if(cache.diagnosticTerminals.begin(),cache.diagnosticTerminals.end(),[&](const auto &t){return t[0]*4096+t[1]==e.b && t[2]*4096+t[3]==e.a && t[4]==(e.d^1);});
     check(terminal!=cache.diagnosticTerminals.end() && (*terminal)[21]==8,"full actual validated edge ledger");
     if(!counts[e.component]++)minimum[e.component]=INT_MAX,maximum[e.component]=INT_MIN;
     for(int k=24;k<32;k++){minimum[e.component]=std::min(minimum[e.component],int((*terminal)[k]));maximum[e.component]=std::max(maximum[e.component],int((*terminal)[k]));}
   }
   RemasterFrame packet;check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",packet),"minimum packet");
   RemasterFrameChannelResolver channels(packet);RemasterLightingFieldCache cpu;cpu.update(packet,true);
   size_t solved=0,unknown=0,selected=0;bool discrepancy=false;
   for(const auto &e:cache.diagnosticPlateaus)if(e.kind==3) {
    if(counts[e.component]) {
     const auto &t=(e.a/4096?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[e.a%4096];
      check(e.status==int(RemasterVisualClaimStatus::Solved),"independently accepted compatible edge samples must solve");
     check(e.height+(t.hasHeight?t.height[0]:0)==minimum[e.component],"minimum absolute before local origin");++solved;
     discrepancy|=minimum[e.component]!=maximum[e.component];
    } else {check(e.status==int(RemasterVisualClaimStatus::Unanchored),"unanchored role cells stay unknown");++unknown;}
   }
   for(size_t at=0;at<packet.scenePixels.size();at++) {
    const auto px=packet.scenePixels[at];if(!px.instanceId)continue;const auto &inst=packet.tileInstances[px.instanceId-1];
    if(inst.visualClaim.status!=RemasterVisualClaimStatus::Solved || channels.effective[px.instanceId-1].tileRole!=RemasterTileRole::OutOfBounds)continue;
    const auto &m=channels.effective[px.instanceId-1];int component=inst.visualClaim.component;
    check(counts[component] && inst.visualClaim.published,"selected solved minimum has independent terminals");
    check(inst.heightOffset+m.height[px.tilePixel]==minimum[component],"actual serialized local/base minimum");
    check(cpu.samples[at].known && std::abs(cpu.height[at*2]-minimum[component]/255.f)<1e-6,"actual CPU selected minimum");++selected;
   }
   check(discrepancy && solved && unknown && selected,"real unequal/anchored/unanchored denominators");
   std::cout<<"PASS actual terminal-derived minimum: solved="<<solved<<" unanchored="<<unknown<<" selected="<<selected<<'\n';
   for(auto [component,h]:minimum)std::cout<<"component="<<component<<" min="<<h<<" max="<<maximum[component]<<" callbacks="<<counts[component]<<'\n';
  }
  return 0;
}
