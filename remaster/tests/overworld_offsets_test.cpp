#include "../runtime/profile_merge.h"
#include "../runtime/remaster.h"
#include "../runtime/lighting_field_cache.h"
#include "../runtime/placement_offset_editor.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <chrono>
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; std::exit(1); } } while(0)
uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = 0;
int main(int argc, char **argv) {
 const std::string text="schema_version = 37\n[game]\ntitle = \"offset test\"\nrom_sha256 = \"66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb\"\n[overworld_placement]\nmode = \"authored_overworld_offsets\"\n[[overworld_offsets]]\nworld = \"light\"\nbackground = 0\nx = 192\ny = 193\noffset = -40\n";
 RemasterProfile p; std::vector<RemasterProfileDiagnostic> d;
 CHECK(S9xRemasterParseProfile(text,p,d));
 std::string saved; CHECK(S9xRemasterSerializeProfile(p,saved,d));
 RemasterProfile q; CHECK(S9xRemasterParseProfile(saved,q,d));
 CHECK(saved.find("offset = -40")!=std::string::npos);
 for (const auto &bad : {std::string("offset = [1,2]"), std::string("offset = 32768"), std::string("offset = -32768"), std::string("x = 8192"), std::string("world = \"other\""), std::string("background = 2")}) {
  auto invalid=text;
  const auto key=bad.substr(0,bad.find(" = "));
  const auto start=invalid.find("\n"+key+" = ")+1;
  invalid.replace(start,invalid.find('\n',start)-start,bad);
  CHECK(!S9xRemasterParseProfile(invalid,q,d));
 }
 CHECK(!S9xRemasterParseProfile(text+text.substr(text.find("[[overworld_offsets]]")),q,d));
 RemasterProfileMergeResult merged;
 auto edited=p; edited.originalSceneContribution=.2f;
 CHECK(S9xRemasterMergeProfiles(p,edited,p,{},false,merged,d));
 CHECK(merged.saved.authoredOverworldOffsets && merged.saved.overworldOffsets.size()==1);
 RemasterContextLocation light; light.area=320;light.x=192;light.y=193;
 auto dark=light;dark.area=321;p.overworldOffsets[dark]=80;
 RemasterOverworldOffsetLookup lookup;lookup.compile(p);
 CHECK(lookup.get(light)==-40 && lookup.get(dark)==80);
 auto missing=light;missing.x++;CHECK(lookup.get(missing)==0);
 auto bg=light;bg.background=1;CHECK(lookup.get(bg)==0);
 auto alias=light;alias.x+=64;p.overworldOffsets[alias]=120;lookup.compile(p);
 CHECK(lookup.get(light)==-40 && lookup.get(alias)==120);
 const auto begin=std::chrono::steady_clock::now();volatile int64_t sum=0;
 for(unsigned i=0;i<1000000;++i)sum+=lookup.get(i&1?light:dark);
 const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
 std::cout<<"lookup ns/operation "<<double(ns)/1000000<<" checksum "<<sum<<"\n";
 CHECK(S9xRemasterUnwrapRenderedScroll(2048,1016)==2040);
 CHECK(S9xRemasterUnwrapRenderedScroll(65535,0)==65536);
 // Rendered line scroll is authoritative across tilemap wrap/seam; flips
 // affect canonical samples only. Conflicting ownership observations fail closed.
 for(unsigned flip=0;flip<4;++flip) {
  RemasterFrame f;f.area.id=320;f.width=16;f.height=8;
  RemasterFrameTileInstance in;in.source=RemasterSourceType::Background;in.sourceIndex=1;
  in.hFlip=flip&1;in.vFlip=flip&2;f.tileInstances={in,in};f.mainPixels.resize(128);
  std::array<int,4> sx{},sy{};sx[1]=2040;sy[1]=1536;
  std::array<std::array<int,4>,8> lineX{},lineY{};
  for(unsigned y=0;y<8;++y) {lineX[y]=sx;lineY[y]=sy;}
  for(unsigned i=0;i<128;++i) {
   f.mainPixels[i].instanceId=(i%16)/8+1;
   f.mainPixels[i].tilePixel=(in.vFlip?7-i/16:i/16)*8+(in.hFlip?7-i%8:i%8);
  }
  S9xRemasterLocateContextTiles(f,sx,sy,lineX.data(),lineY.data(),8);
  CHECK(f.contextLocations.at(1).x==255 && f.contextLocations.at(2).x==256);
  lineX[7][1]+=8;
  S9xRemasterLocateContextTiles(f,sx,sy,lineX.data(),lineY.data(),8);
  CHECK(f.contextLocations.empty());
 }
 const RemasterTileContentId tile{12345,1,4};
 RemasterAssetMetadata metadata;metadata.tileId=tile;metadata.hasHeight=true;
 metadata.height.fill(0);metadata.height[1]=-8;p.assets[tile]=metadata;
 S9xRemasterSetLiveFramesEnabled(true);
 RemasterLightingFieldCache cached, forced;
 light.background=1;dark.background=1;
 p.overworldOffsets[dark]=80;
  for (unsigned world=0;world<2;++world) for(unsigned flip=0;flip<4;++flip) {
   p.overworldOffsets[dark]=80;
  RemasterAreaContext area;area.id=320+world;
  RemasterDungeonFloorContext context;context.verifiedAlttpRom=true;context.indoors=false;
  std::array<int,4> sx{},sy{};sx[0]=1536;sy[0]=1544;
   for(unsigned revision=0;revision<6;++revision) {
   p.overworldOffsets[light]=-40+int(revision);
   p.authoredOverworldOffsets=revision!=2;
    context.indoors=revision==3;
    if(revision==5) { p.overworldOffsets.erase(light);p.overworldOffsets.erase(dark); }
   S9xRemasterSetProfile(p);S9xRemasterBeginFrame(64,8,8,8);
   auto &s=S9xRemasterState();RemasterFrameTileInstance in;
   in.tileId=tile;in.source=RemasterSourceType::Background;in.sourceIndex=1;in.hFlip=flip&1;in.vFlip=flip&2;
   s.tileInstances.push_back(in);
   auto actor=in;actor.source=RemasterSourceType::Object;actor.heightOffset=7;s.tileInstances.push_back(actor);
   auto hud=in;hud.sourceIndex=2;hud.heightOffset=11;s.tileInstances.push_back(hud);
   for(unsigned i=0;i<64;++i) {
    s.mainOwners[i]=0x02000000;s.mainInstanceIds[i]=1;
    s.mainTilePixels[i]=(in.vFlip?7-i/8:i/8)*8+(in.hFlip?7-i%8:i%8);
   }
   uint16_t screen[64]{};RemasterFrame f;
   sx[1]=sx[0];sy[1]=sy[0];
   S9xRemasterEndFrame(screen,8,8,8,&f,&context,nullptr,&area,0,nullptr,revision==4?nullptr:&sx,revision==4?nullptr:&sy);
   if(revision!=4)CHECK(f.contextLocations.at(1).x==192 && f.contextLocations.at(1).y==193);
    CHECK(f.tileInstances[0].heightOffset==(revision>=2?0:world?80:-40+int(revision)));
    CHECK(f.tileInstances[0].savedPlacementOffsetStatus==(revision==5?1:revision>=2?0:2));
    CHECK(f.tileInstances[0].savedPlacementOffset==(revision>=2?0:world?80:-40+int(revision)));
    CHECK(f.tileInstances[1].savedPlacementOffsetStatus==0 && f.tileInstances[2].savedPlacementOffsetStatus==0);
   CHECK(f.tileInstances[1].heightOffset==7 && f.tileInstances[2].heightOffset==11);
   CHECK(f.assetMetadata[0].hasHeight && f.assetMetadata[0].height[0]==0 && f.assetMetadata[0].height[1]==-8);
   CHECK(!f.assetMetadata[0].hasNormals);
   cached.update(f,true);forced.update(f,true,true);
   CHECK(cached.height==forced.height && cached.surface==forced.surface && cached.occlusion==forced.occlusion);
   cached.update(f,true);CHECK(cached.selectedPixels==0 && cached.resolvedInstances==0);
  }
 }
 if(argc==4) {
  std::ifstream pf(argv[1]);std::string pt((std::istreambuf_iterator<char>(pf)),{});
  CHECK(S9xRemasterParseProfile(pt,p,d));CHECK(p.overworldOffsets.size()==16384);
  CHECK(S9xRemasterSerializeProfile(p,saved,d));CHECK(S9xRemasterParseProfile(saved,q,d));
  CHECK(q.overworldOffsets.size()==p.overworldOffsets.size());
  for(const auto &e:p.overworldOffsets)CHECK(q.overworldOffsets.at(e.first)==e.second);
  RemasterFrame capture;CHECK(S9xReadRemasterFrame(argv[2],capture));
  std::ifstream bf(argv[3],std::ios::binary);std::vector<uint8_t> bin((std::istreambuf_iterator<char>(bf)),{});
  CHECK(bin.size()>264552);
  auto word=[&](size_t at){CHECK(at+1<bin.size());return unsigned(bin[at]|bin[at+1]<<8);};
  const int hx=word(8+0xe2),hy=word(8+0xe8)+1;
  std::array<std::array<int,4>,224> linesX{},linesY{};
  for(unsigned y=0;y<224;++y) {
   linesX[y][1]=S9xRemasterUnwrapRenderedScroll(hx,word(260968+y*16+4));
   linesY[y][1]=S9xRemasterUnwrapRenderedScroll(hy,word(260968+y*16+6));
  }
  S9xRemasterSetProfile(p);S9xRemasterBeginFrame(256*224,256,256,224);
  auto &state=S9xRemasterState();state.tileInstances=capture.tileInstances;
  for(size_t i=0;i<capture.mainPixels.size();++i) {
   const auto &px=capture.mainPixels[i];
   if(!px.instanceId || px.instanceId>capture.tileInstances.size())continue;
   const auto &in=capture.tileInstances[px.instanceId-1];
    state.mainOwners[i]=(in.source==RemasterSourceType::Background?0x02000000:0x03000000)|(uint32_t(in.sourceIndex)<<16);
   state.mainInstanceIds[i]=px.instanceId;state.mainTilePixels[i]=px.tilePixel;
  }
  RemasterAreaContext area;area.id=320;RemasterDungeonFloorContext ctx;ctx.verifiedAlttpRom=true;
  RemasterFrame f;std::vector<uint16_t> screen(256*224);
  S9xRemasterEndFrame(screen.data(),256,256,224,&f,&ctx,nullptr,&area,0,nullptr,
   &linesX[0],&linesY[0],nullptr,nullptr,linesX.data(),linesY.data(),224);
  unsigned checked=0,mismatch=0,nonzero=0;
  for(const auto &loc:f.contextLocations) {
   const auto &in=f.tileInstances[loc.first-1];if(in.sourceIndex!=1)continue;
   // Actual SNES 64x64 tilemap page ordering, NOT a world-coordinate source.
   const unsigned x=loc.second.x&63,y=loc.second.y&63;
   const unsigned address=((y/32)*2+x/32)*2048+((y%32)*32+x%32)*2;
   const unsigned w=word(131080+address);
   ++checked;
   mismatch+=in.tileNumber!=(w&1023) || in.palette!=((w>>10)&7) || in.hFlip!=bool(w&0x4000) || in.vFlip!=bool(w&0x8000);
   const auto off=p.overworldOffsets.find(loc.second);
   const int expected=off==p.overworldOffsets.end()?0:off->second;
   CHECK(in.heightOffset==expected);nonzero+=expected!=0;
  }
  std::cout<<"capture world camera "<<hx<<","<<hy<<" checked "<<checked<<" mapword mismatches "<<mismatch<<" nonzero origins "<<nonzero<<"\n";
   CHECK(checked>500 && mismatch==0 && nonzero>0);
   unsigned picked=0,aliases=0;
   for(size_t n=0;n<f.mainPixels.size();++n) {
    const auto id=S9xRemasterPlacementOffsetPick(f,n);if(!id)continue;
    ++picked;aliases+=id!=f.mainPixels[n].instanceId;
    std::set<uint32_t> selection;CHECK(S9xRemasterPickPlacementOffset(f,n,false,selection)==id);
    CHECK(selection.count(S9xRemasterPlacementOffsetPick(f,n))==1);
   }
    CHECK(picked>0);std::cout<<"real outdoor exact picker/highlight "<<picked<<" pixels; scene/stock aliases "<<aliases<<"\n";
    // Exercise actual type-click expansion against independently enumerated
    // picker truth in the frozen outdoor EndFrame result, not vector presence.
    std::map<RemasterTileContentId,std::set<uint32_t>> visibleTypes;
    std::map<RemasterTileContentId,size_t> firstPixels;
    for(size_t n=0;n<f.mainPixels.size();++n)if(auto id=S9xRemasterPlacementOffsetPick(f,n)) {
     const auto tile=f.tileInstances[id-1].tileId;visibleTypes[tile].insert(id);firstPixels.emplace(tile,n);
    }
    size_t expandedTypes=0,expandedIds=0;
    for(const auto &entry:visibleTypes) {
     std::set<uint32_t> ids;std::set<RemasterTileContentId> types;
     auto representative=S9xRemasterPickPlacementOffset(f,firstPixels.at(entry.first),false,ids,&types);
     CHECK(ids==entry.second && ids.count(representative));
     std::set<RemasterContextLocation> locations;std::string error;CHECK(S9xRemasterPlacementTargets(p,f,ids,locations,error));
     auto edit=p;CHECK(S9xRemasterEditPlacementOffsets(edit,f,ids,1,true,error));
     for(const auto &at:locations) {auto old=p.overworldOffsets.find(at);CHECK(edit.overworldOffsets.at(at)==(old==p.overworldOffsets.end()?0:old->second)+1);}
     expandedIds+=ids.size();++expandedTypes;
    }
    std::cout<<"real visible type expansion "<<expandedTypes<<" types / "<<expandedIds<<" occurrences; exact picker/highlight membership and coordinate edits\n";
   const auto evidence=f;RemasterLightingFieldCache incremental,forced;incremental.update(f,true);
   for(bool enabled:{false,true,false,true}) {
    auto preview=p;preview.authoredOverworldOffsets=enabled;
    S9xRemasterApplyProfileToFrame(preview,f);S9xRemasterPreviewPlacementOffsets(preview,f);
    incremental.update(f,true);forced.update(f,true,true);CHECK(incremental.height==forced.height);
    for(const auto &loc:evidence.contextLocations) {
     const auto &old=evidence.tileInstances[loc.first-1];if(!old.savedPlacementOffsetStatus)continue;
     const auto &now=f.tileInstances[loc.first-1];
     CHECK(now.heightOffset==old.heightOffset-(enabled?0:old.savedPlacementOffset));
     CHECK(now.savedPlacementOffsetStatus==(enabled?old.savedPlacementOffsetStatus:0));
    }
   }
  S9xRemasterApplyProfileToFrame(p,capture); // current arrays, not embedded old profile
  unsigned currentCompared=0;
  for(const auto &m:f.assetMetadata) {
   const auto a=p.assets.find(m.tileId);CHECK(a!=p.assets.end());
   CHECK(m.hasHeight==a->second.hasHeight && m.height==a->second.height);
   CHECK(m.hasNormals==a->second.hasNormals && m.normalXyz==a->second.normalXyz);
   ++currentCompared;
  }
  std::cout<<"current-profile exact channel records "<<currentCompared<<"\n";
 }
 std::cout << "overworld scalar offsets PASS\n";
}
