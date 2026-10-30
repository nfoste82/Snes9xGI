// Explicit synthetic CPU-write witness over pinned real source/map/graphics.
// This validates the production consumer, NOT historical live writer identity.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#include "../runtime/remaster.h"
static void researchWrites(RemasterDungeonFloorContext &c) {
#ifdef REMASTER_LIVE_PRODUCER_CONTRACT
 auto ram=std::make_unique<std::array<uint8_t,131072>>();
 auto rom=std::make_unique<std::array<uint8_t,1048576>>();
 std::copy_n(c.dungeonTemplateData,12876,rom->data()+0x1b52);
 (*ram)[0x1b]=1;(*ram)[0x10]=7;(*ram)[0xa0]=c.roomIndex;(*ram)[0xa1]=c.roomIndex>>8;
 auto &trace=S9xRemasterDungeonWrites();trace.enable(true);trace.clear();
 auto emit=[&](int origin,int source,bool both) {
  (*ram)[0]=both?0:0x81;(*ram)[1]=both?0:0x0e;
  for(int x=0;x<4;x++)for(int y=0;y<4;y++)for(int bg=both?0:1;bg<2;bg++) {
   const uint32_t single[]={0x0197f7,0x0197fc,0x019801,0x019806};
   const uint32_t paired[]={0x019822,0x01982d,0x019838,0x019843};
   const auto pc=both?paired[y]+(bg?4:0):single[y];
   (*rom)[(pc&0xffff)-(both?4:2)]=both?0x9f:0x97;if(both)(*rom)[(pc&0xffff)-1]=0x7e;
   (*ram)[0xe]=4-x;
   const int n=source+2*(x*4+y),value=c.dungeonTemplateData[n]|c.dungeonTemplateData[n+1]<<8;
   trace.write(ram->data(),rom->data(),(bg?0x2000:0x4000)+2*(origin+y*64+x),value,2,pc,both?2*x:source+8*x,both?source+8*x:0);
  }
 };
 for(int si=0;si<c.stairCount;si++) {
  const auto &s=c.stairs[si];emit(s.y*64+s.x,s.highIsNorth?0x10c8:0x10e8,true);
 }
 // Research door witness derives all complete 26f6 final-map matches; explicit
 // test injection only. Production must observe CPU stores, never do this scan.
 for(int y=0;y<61;y++)for(int x=0;x<61;x++) {
  bool exact=true;for(int xx=0;xx<4;xx++)for(int yy=0;yy<4;yy++) {
   int i=(y+yy)*64+x+xx,n=0x26f6+2*(xx*4+yy);
   exact &= (c.constructedMaps[2*i]|c.constructedMaps[2*i+1]<<8)==(c.dungeonTemplateData[n]|c.dungeonTemplateData[n+1]<<8);
  }if(exact)emit(y*64+x,0x26f6,false);
 }
 // Revoke every observed member subsequently replaced in the final maps.
 for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
  int n=(bg?0:8192)+i*2,word=c.constructedMaps[n]|c.constructedMaps[n+1]<<8;
  if(trace.cells[bg*4096+i].owner && trace.cells[bg*4096+i].word!=word)
   trace.write(ram->data(),rom->data(),(bg?0x2000:0x4000)+2*i,word,2,0,0,0);
 }
 c.producerWrites=&trace;
#endif
}
#define REMASTER_ENDFRAME_PRODUCER_RESEARCH(c) researchWrites(c)
#define main reinjectionMain
#include "../tools/ALttP/alttp_reinject_endframe_probe.cpp"
#undef main
int main(int argc,char **argv) {
  if(argc!=3){std::cerr<<"usage: capture output-prefix (synthetic witness; not live history)\n";return 2;}
#ifdef REMASTER_DUNGEON_RESEARCH_CONTAINMENT
  check(!S9xRemasterDungeonResearchEnabled(),"research must default off");
#endif
  const int ordinary=reinjectionMain(argc,argv);if(ordinary)return ordinary;
  auto noClaims=[] {
   const auto &s=S9xRemasterState();
   for(const auto *m:{&s.runtimeDungeonBg1HeightMap,&s.runtimeDungeonHeightMap})
    for(auto owned:m->producerOwnedStair)check(!owned,"default accepted synthetic producer stair");
   check(s.runtimeDungeonHeightMap.doorframeAttachments.empty(),"default accepted cave producer door");
   check(s.runtimeDungeonHeightMap.planeOffsets[0][55*64+43]-s.runtimeDungeonHeightMap.planeOffsets[1][50*64+43]==50,
    "default must preserve inherited no-witness relation");
  };
  noClaims();
#ifdef REMASTER_DUNGEON_RESEARCH_CONTAINMENT
  S9xRemasterSetDungeonResearchEnabled(true);
#endif
 const int result=reinjectionMain(argc,argv);if(result)return result;
 const auto &s=S9xRemasterState();
 const auto &bg1=s.runtimeDungeonBg1HeightMap,&bg2=s.runtimeDungeonHeightMap;
 check(bg2.planeOffsets[0][55*64+43]-bg2.planeOffsets[1][50*64+43]==24,"early signed quantized stair relation");
 check(bg1.offsets[54*64+43]==bg2.planeOffsets[1][50*64+43] && bg2.offsets[54*64+43]==bg1.offsets[54*64+43],"shared producer base through both BGs");
 check(!bg2.producerOwnedStair[51*64+43] && bg1.producerOwnedStair[51*64+43],"overwritten BG2 member acquired stair ownership");
 for(int y=59;y<=61;y++) {
  bool attached=false;for(const auto &a:bg2.doorframeAttachments)if(a.cell==y*64+46 && a.plane==0 && a.base==bg2.planeOffsets[0][58*64+46]+8*(y-59))attached=true;
  check(attached,"cave producer frame band/support attachment");
 }
 for(int bg=0;bg<2;bg++)for(int y=50;y<=55;y++) {
  const auto &t=(bg?*s.dungeonRetainedBg2:*s.dungeonRetainedBg1)[y*64+43];
  std::cout<<"T "<<bg<<' '<<y<<" surface="<<int(t.surface)<<" height="<<t.hasHeight<<':'<<t.height[0]<<" planes="<<(bg?bg2:bg1).planeOffsets[0][y*64+43]<<','<<(bg?bg2:bg1).planeOffsets[1][y*64+43]<<'\n';
 }
 std::cout<<"RESEARCH stair BG1="<<bg1.offsets[54*64+43]<<" BG2="<<bg2.offsets[54*64+43]<<" landing="<<bg2.planeOffsets[0][55*64+43]<<'\n';
 for(const auto *m:{&bg1,&bg2}) {
  for(const auto &a:m->stairAttachments)std::cout<<"ATTACH "<<a.base<<" conflict="<<a.conflict<<" low="<<a.lowPlane<<':'<<m->planeOffsets[a.lowPlane][a.low]<<" high="<<a.highPlane<<':'<<m->planeOffsets[a.highPlane][a.high]<<'\n';
 }
  for(int y=59;y<=61;y++)for(const auto &a:bg2.doorframeAttachments)if(a.cell==y*64+46)std::cout<<"RESEARCH door row="<<y<<" base="<<a.base<<" plane="<<a.plane<<'\n';
#ifdef REMASTER_DUNGEON_RESEARCH_CONTAINMENT
  S9xRemasterSetDungeonResearchEnabled(false);
  const int disabled=reinjectionMain(argc,argv);if(disabled)return disabled;
  noClaims();
  std::cout<<"PASS default/opt-in/disabled actual EndFrame, synthetic stale trace cannot grant claims\n";
#endif
  return 0;
}
