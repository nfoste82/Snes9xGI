// Complete ROM structural footprints, independent of opening contents.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> bytes(const std::string &p){std::ifstream f(p,std::ios::binary);need(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
  need(argc==2,"usage: complete own079/083 capture");const std::string path=argv[1];
 auto in=bytes(path+".dungeon-input.bin"),tpl=bytes(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> e;need(S9xRemasterLoadProfile(path+".profile.toml",profile,e),"profile");
 std::string before,after;need(S9xRemasterSerializeProfile(profile,before,e),"serialize");
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 C c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=97;c.linkX=832;c.linkY=3128;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.constructedMaps=in.data()+8;c.graphics=in.data()+24584;
 c.collisionAttributes=in.data()+16392;c.dungeonTemplateData=tpl.data()+8;S9xRemasterDungeonStairsFromAttributes(c);
 auto t=std::make_unique<std::array<C::Placement,4096>>();
 auto solve=[&]{S9xRemasterDungeonPlacements(c,profile,*t);c.placements=t->data();auto m=std::make_unique<RemasterDungeonHeightMap>();need(S9xRemasterBuildDungeonHeightMap(c,50,*m),"solve");return m;};
  auto m=solve();int count=0;
  // West ordinary 3x4 frame beside a high curved wall's clipped cap.
  // This complete room fixture is also present offscreen in own079.
  auto west=[&](bool accepted){int seen=0;
   for(const auto &a:m->doorframeAttachments)if(a.cell%64>=2 && a.cell%64<=4 && a.cell/64>=15 && a.cell/64<=18){
    need(accepted,"ambiguous/noncanonical cap attached");need(a.visible && a.plane==0 && a.base==56-a.cell%64*8,"west signed band base");seen++;
    const auto &p=(*t)[a.cell];need(a.mask==p.doorframeMask,"west authored mask replaced");
   }
   need(seen==(accepted?8:0),"west attachment denominator");};
  west(true);
  const auto placed=*t;
  // Complete-profile ablations at BOTH independent cap contacts. No authored
  // source is changed by runtime; this deliberately malformed input must not
  // acquire a guessed floor/nearest-wall offset.
  const auto capId=RemasterTileContentId{(*t)[14*64+2].tileHash,1,4};
  const auto savedCap=profile.assets.at(capId);
  profile.assets.at(capId).height[0]++;m=solve();west(false);
  profile.assets.at(capId)=savedCap;m=solve();west(true);
  for(int i=0;i<4096;i++)need((*t)[i].height==placed[i].height,"runtime changed intrinsic source");
 for(int y=4;y<7;y++)for(int x=30;x<34;x++)if(y==4 || x==30 || x==33){
  const int i=y*64+x;need((*t)[i].doorAssembly==4*64+31 && (*t)[i].doorDirection==0,"north frame ownership");
  bool found=false;for(const auto &a:m->doorframeAttachments)if(a.cell==i && a.visible){need(a.base==24+(6-y)*8,"frame seam base");found=true;}
  need(found,"frame effective attachment");count++;
 }
 for(int x:{11,52})for(int y=4;y<8;y++){
  const int i=y*64+x;need((*t)[i].surface==S::Wall && m->offsets[i]==24,"NW/NE contextual +24");
  const auto &a=profile.assets.at({(*t)[i].tileHash,1,4});need(a.height==(*t)[i].height,"authored diagonal unchanged");
 }
 const auto original=in;
 // Canonical lower cardinal phases and shaped top are required on BOTH axes.
 // A changed seam, absent role, or mixed signed datum cannot activate diagonals.
 auto cardinal=[&](int off){int w=tpl[8+off]|tpl[9+off]<<8;uint8_t p[64];
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(c.graphicsBase+(w&1023)*32+b/2*16+y*2+b%2)]>>(7-x))&1)<<b;p[y*8+x]=v;}
  return RemasterTileContentId{S9xRemasterHashTile(4,p),1,4};};
 for(int off:{0x2c8,0x2ca,0x2cc,0x2ea,0x2ec}){
  auto id=cardinal(off);const auto saved=profile.assets.at(id);
  profile.assets[id].height[0]++;solve();need(!(*t)[4*64+11].diagonalHeightPreview,"malformed cross-section accepted");
  profile.assets[id]=saved;profile.assets[id].tileRole=RemasterTileRole::Unknown;
  solve();need(!(*t)[4*64+11].diagonalHeightPreview,"unowned local cross-section accepted");profile.assets[id]=saved;
 }
 // Changing opening local height must not affect ANY frame attachment base.
 m=solve(); // metadata must reflect the restored profile, not the last ablation
 const auto openingId=RemasterTileContentId{(*t)[5*64+31].tileHash,1,4};
 const auto opening=profile.assets.find(openingId);const bool existed=opening!=profile.assets.end();
 RemasterAssetMetadata savedOpening;if(existed)savedOpening=opening->second;
 for(int relief:{-31,0,39}){
  auto &a=profile.assets[openingId];a.tileId=openingId;a.hasHeight=true;a.height.fill(relief);
  m=solve();for(int y=4;y<7;y++)for(int x=30;x<34;x++)if(y==4 || x==30 || x==33){
   bool found=false;for(const auto &attachment:m->doorframeAttachments)if(attachment.cell==y*64+x && attachment.visible){need(attachment.base==24+(6-y)*8,"opening relief changes frame");found=true;}
   need(found,"opening relief vetoes frame");
  }
 }
 if(existed)profile.assets[openingId]=savedOpening;else profile.assets.erase(openingId);
 // Signed intrinsic datum is legal when all cardinal/diagonal sections agree.
 std::set<RemasterTileContentId> signedIds;
 for(int off:{0x2c8,0x2ca,0x2cc,0x2e8,0x2ea,0x2ec})signedIds.insert(cardinal(off));
 for(int y=4;y<8;y++)signedIds.insert({(*t)[y*64+11].tileHash,1,4});
 for(auto id:signedIds)for(auto &h:profile.assets.at(id).height)h-=32;
 solve();for(int y=4;y<8;y++)need((*t)[y*64+11].diagonalAuthoredPreserved,"signed section rejected");
 for(auto id:signedIds)for(auto &h:profile.assets.at(id).height)h+=32;
 // Ordinary and 4x4 frame identities; all H/V transforms and arbitrary opening
 // words/heights. Use a clear complete map so unrelated templates cannot own it.
 for(int family=0;family<2;family++)for(int flip=0;flip<4;flip++)for(int variant=0;variant<3;variant++){
  std::fill(in.begin()+8,in.begin()+16392,0xff);
  const int off=family?0x11a8:0x2990,h=family?4:3,origin=20*64+20;
  auto put=[&](int i,int w){in[8+i*2]=w;in[9+i*2]=w>>8;};
  for(int x=0;x<4;x++)for(int y=0;y<h;y++){
   int w=tpl[8+off+2*(x*h+y)]|tpl[9+off+2*(x*h+y)]<<8;
   if(x>0 && x<3 && y>0)w=variant==0?0xcac:variant==1?0x807:0x9ef;
   put(origin+(flip&2?h-1-y:y)*64+(flip&1?3-x:x),w^((flip&1?0x4000:0)|(flip&2?0x8000:0)));
  }
  S9xRemasterDungeonPlacements(c,profile,*t);
  const int frameOrigin=origin+((family && flip&2)?64:0);
  for(int x=0;x<4;x++)for(int y=0;y<3;y++)if(x==0 || x==3 || y==0){
   const int i=origin+(flip&2?h-1-y:y)*64+(flip&1?3-x:x);
   need((*t)[i].doorAssembly==frameOrigin+1 && (*t)[i].doorDirection==((flip&2)?1:0),"transformed frame recognition");
  }
  // One overwritten outer word breaks the WHOLE footprint, not only that tile.
  put(origin+(flip&2?h-1:0)*64,0xffff);S9xRemasterDungeonPlacements(c,profile,*t);
  for(const auto &p:*t)need(p.doorAssembly!=frameOrigin+1,"partial frame accepted");
 }
 in=original;need(S9xRemasterSerializeProfile(profile,after,e) && before==after,"profile bytes changed");
 std::cout<<"PASS "<<count<<" own079 seam attachments, NW/NE +24 authored preservation; ordinary/4x4 all flips, floor/stair/opening independence and overwritten-frame rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
