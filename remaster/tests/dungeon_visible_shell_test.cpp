#include "../runtime/remaster.h"
#include <cstdlib>
#include <iostream>
static void need(bool b,const char*s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 using S=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto peer=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.placements=tiles->data();c.collisionAttributes=attrs->data();
 c.linkPlane=0;c.linkX=48;c.linkY=136;c.placementBackgroundIndex=1;
 for(int y=17;y<22;y++)for(int x=6;x<10;x++){auto&t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;}
 const int i=18*64+11,j=i-1;
 auto&a=(*tiles)[i];a.surface=S::Wall;a.hasHeight=true;a.wallTemplateOffset=0x2d8;a.highPerimeterFace=true;
 auto&b=(*peer)[j];b.surface=S::Wall;b.hasHeight=true;b.wallTemplateOffset=0x318;
 for(int k=0;k<64;k++){a.height[k]=24+k%8;b.height[k]=16+k%8;}
 auto pm=std::make_unique<RemasterDungeonHeightMap>();pm->hasPlaneOffsets=true;for(auto&p:pm->planeOffsets)p.fill(255);pm->planeOffsets[0][j]=0;
 c.peerShellPlacements=peer->data();c.peerShellMap=pm.get();
 pm->authoredWallSupportRelations.push_back({j,18*64+8,0,0,b.height});
 auto m=std::make_unique<RemasterDungeonHeightMap>();need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"solve");
 need(m->visibleShellContacts==1,"complementary full unit-gradient shell contact consumed");
 need(m->planeOffsets[1][i]==255,"never borrow other gameplay plane");
 auto supports=m->requiredPlaneCells;
 b.height[7]++;need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"negative solve");need(m->visibleShellContacts==0,"malformed cross-section rejected");
 need(m->requiredPlaneCells==supports,"shell never rewrites physical support denominator");
 b.height[7]--;a.wallTemplateOffset=0xba6;b.wallTemplateOffset=0xc26;a.highCornerFace=true;
 need(S9xRemasterBuildDungeonHeightMap(c,50,*m)&&m->visibleShellContacts==1,"ordered high/low corner phase contact");
 b.wallTemplateOffset=0xc06;need(S9xRemasterBuildDungeonHeightMap(c,50,*m)&&m->visibleShellContacts==0,"incompatible corner ownership rejected");
 // Joint constraints must reach a true occupied support, not a normalized
  // peer wall datum. Exercise both axes, all flips and reversed BG indices.
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 c.constructedMaps=maps->data();
 const int support=18*64+8;
  for(int bg=0;bg<2;bg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int sign:{-1,1}) {
   c.placementBackgroundIndex=bg;
  tiles->fill({});peer->fill({});attrs->fill(1);
  for(int y=17;y<22;y++)for(int x=6;x<10;x++){auto&t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;}
  const int cell=18*64+11,other=cell+sign*(axis?64:1);
  auto &a=(*tiles)[cell];auto &b=(*peer)[other];
  a.surface=b.surface=S::Wall;a.hasHeight=b.hasHeight=true;
  a.highCornerFace=true;a.wallTemplateOffset=0xba6;b.wallTemplateOffset=0xc26;
  a.hflip=b.hflip=flip&1;a.vflip=b.vflip=flip&2;
  for(int k=0;k<64;k++)a.height[k]=b.height[k]=axis?k/8:k%8;
  pm->authoredWallSupportRelations.clear();pm->planeOffsets[0].fill(255);
  // Signed path says peer wall is eight below occupied floor. Its old
  // independently normalized base deliberately disagrees (must be ignored).
  pm->planeOffsets[0][other]=100;
   pm->authoredWallSupportRelations.reserve(4);
   pm->authoredWallSupportRelations.push_back({other,support,-8,0,b.height});
  auto &r=pm->authoredWallSupportRelations.back();r.terminalLowPath=true;r.source=b.wallTemplateOffset;
  r.hflip=b.hflip;r.vflip=b.vflip;r.path={7,0};
  (*peer)[support].surface=S::Floor;(*peer)[support].hasHeight=true;pm->planeOffsets[0][support]=0;
  uint64_t id=UINT64_C(14695981039346656037);for(auto byte:*maps)id=(id^byte)*UINT64_C(1099511628211);pm->shellMapIdentity=id;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"joint transformed solve");
  const int slope=axis?(b.vflip?-1:1):(b.hflip?-1:1);
  const int expected=-8-sign*8*slope;
  need(m->structuralPlaneOffsets[0][cell]==expected,"signed path, not peer normalized datum");
  need(m->planeOffsets[0][support]==0 && m->requiredPlaneCells[1]==0,"physical supports and plane denominator preserved");
   need(!m->visibleShellCandidates.empty(),"candidate samples retained");
   {auto duplicate=r;duplicate.rise++;pm->authoredWallSupportRelations.push_back(duplicate);
    need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"contradictory signed paths solve");
    need(m->structuralPlaneOffsets[0][cell]==255 && m->structuralConflicts>0,"contradictory anchors remain explicit");
    need(m->planeOffsets[0][support]==0 && m->unknownPlaneCells[0]==0,"contradictory shell preserves physical supports");
    pm->authoredWallSupportRelations.pop_back();}
  r.support+=4096;need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"cross plane solve");
  need(m->structuralPlaneOffsets[0][cell]==255,"cross plane path rejected");r.support-=4096;
  pm->shellMapIdentity++;need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"stale map solve");
  need(m->structuralPlaneOffsets[0][cell]==255,"stale complete map rejected");pm->shellMapIdentity--;
  r.word=1;need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"overwrite solve");
  need(m->structuralPlaneOffsets[0][cell]==255,"overwritten peer identity rejected");r.word=0;
  r.height[0]++;need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"changed art solve");
  need(m->structuralPlaneOffsets[0][cell]==255,"changed authored array rejected");r.height[0]--;
  r.path.clear();need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"unproved path solve");
  need(m->structuralPlaneOffsets[0][cell]==255,"missing signed path rejected");
 }
 // A wall foot belongs to its occupied floor even if the wall cell itself
 // has no movement component. An independently normalized peer is not owner.
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++) {
  c.placementBackgroundIndex=bg;tiles->fill({});peer->fill({});attrs->fill(1);
  for(int y=17;y<22;y++)for(int x=6;x<10;x++){auto&t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;}
  const int cell=18*64+10,other=cell+1;
  auto&a=(*tiles)[cell];auto&b=(*peer)[other];
  a.surface=b.surface=S::Wall;a.hasHeight=b.hasHeight=true;
  a.wallTemplateOffset=0xba6;b.wallTemplateOffset=0xc26;
  a.hflip=b.hflip=flip&1;a.vflip=b.vflip=flip&2;
  for(int k=0;k<64;k++)a.height[k]=b.height[k]=(flip&1)?7-k%8:k%8;
  pm->authoredWallSupportRelations.clear();pm->planeOffsets[0].fill(255);
  pm->planeOffsets[0][other]=40;
  pm->authoredWallSupportRelations.push_back({other,support,40,0,b.height});
  need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"independent graph solve");
  need(m->structuralPlaneOffsets[0][cell]==0,"occupied graph retains its own signed wall foot");
  need(m->structuralConflicts==0,"normalized peer does not invent physical graph conflict");
  need(std::any_of(m->visibleShellCandidates.begin(),m->visibleShellCandidates.end(),[&](const auto&r){return r.cell==cell && r.reason=="independent_physical_graph";}),"independent physical graph reason retained");
 }
 // Real producer -> reversed map order -> consumer. Nonidentical BG words
 // ensure a fixed-index or unswapped identity cannot accidentally pass.
 for(int donorBg=0;donorBg<2;donorBg++)for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++) {
  tiles->fill({});peer->fill({});attrs->fill(1);maps->fill(0);
  c.placementBackgroundIndex=donorBg;c.peerShellMap=nullptr;c.peerShellPlacements=nullptr;
  for(int y=17;y<22;y++)for(int x=6;x<10;x++){auto&t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;(*attrs)[y*64+x]=0;}
  const int low=axis?22*64+8:18*64+10,high=low+(axis?64:1);
  auto&b=(*tiles)[low];b.surface=S::Wall;b.hasHeight=true;b.wallTemplateOffset=0xc26;
  b.hflip=flip&1;b.vflip=flip&2;
  for(int k=0;k<64;k++){int x=k%8,y=k/8;b.height[k]=axis?(b.vflip?7-y:y):(b.hflip?7-x:x);}
  (*maps)[low*2]=0x31;(*maps)[8192+high*2]=0x52;
  auto donor=std::make_unique<RemasterDungeonHeightMap>();
  need(S9xRemasterBuildDungeonHeightMap(c,50,*donor),"real donor solve");
  auto it=std::find_if(donor->authoredWallSupportRelations.begin(),donor->authoredWallSupportRelations.end(),[&](const auto&r){return r.wall==low && r.terminalLowPath;});
  need(it!=donor->authoredWallSupportRelations.end(),"both BG indices export actual terminal paths");
  need(it->rise==0 && it->word==0x31 && !it->path.empty(),"real donor signed path and final word");
  *peer=*tiles;tiles->fill({});
  auto&a=(*tiles)[high];a=(*peer)[low];a.wallTemplateOffset=0xba6;a.highCornerFace=true;
  std::rotate(maps->begin(),maps->begin()+8192,maps->end());
  c.placementBackgroundIndex=1-donorBg;c.peerShellPlacements=peer->data();c.peerShellMap=donor.get();
  need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"real reversed BG receiver solve");
  need(m->structuralPlaneOffsets[0][high]==8,"real reversed BG contact has signed base eight");
  need(std::none_of(m->visibleShellCandidates.begin(),m->visibleShellCandidates.end(),[&](const auto&r){return r.cell==high && r.plane==1 && r.reason=="signed_graph_constraint";}),"real reversed BG preserves plane ownership");
  (*maps)[8192+low*2]^=1;
  need(S9xRemasterBuildDungeonHeightMap(c,50,*m) && m->structuralPlaneOffsets[0][high]==255,"real reversed BG changed map rejected");
 }
 std::cout<<"PASS visual-shell contacts and joint signed paths distinct from physical supports/planes\n";
}
