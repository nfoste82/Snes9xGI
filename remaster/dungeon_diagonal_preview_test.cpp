// Failure-class replay: exact stepped ownership, transformed seams, no authored writes.
#include "remaster.h"
#include <cassert>
#include <iostream>
static std::vector<uint8_t> bytes(const std::string &p) {
 std::ifstream f(p,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char **argv) {
 assert(argc==2);std::string p=argv[1];auto input=bytes(p+".dungeon-input.bin"),tmpl=bytes(p+".dungeon-templates.bin");
 assert(input.size()==90120 && tmpl.size()==12884);
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 assert(S9xRemasterLoadProfile(p+".profile.toml",profile,errors));
 std::string before,after;assert(S9xRemasterSerializeProfile(profile,before,errors));
 RemasterDungeonFloorContext c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=0x61;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.constructedMaps=input.data()+8;c.graphics=input.data()+24584;
 c.collisionAttributes=input.data()+16392;c.dungeonTemplateData=tmpl.data()+8;
 c.linkX=594;c.linkY=3167;S9xRemasterDungeonStairsFromAttributes(c);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto solve=[&](){S9xRemasterDungeonPlacements(c,profile,*tiles);c.placements=tiles->data();
  auto m=std::make_unique<RemasterDungeonHeightMap>();assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));return m;};
 auto m=solve();assert(m->localHeightPreviews.size()==122);
 for(const auto &preview:m->localHeightPreviews) {
  const auto &t=(*tiles)[preview.cell];assert(m->offsets[preview.cell]!=255);
  auto f=std::make_unique<RemasterFrame>();f->width=f->height=8;f->mainPixels.resize(64);f->subPixels.resize(64);
  RemasterFrameTileInstance instance;instance.tileId={t.tileHash,1,4};instance.source=RemasterSourceType::Background;
  instance.sourceIndex=1;instance.hFlip=t.hflip;instance.vFlip=t.vflip;f->tileInstances.push_back(instance);
  RemasterFrameAssetMetadata a;a.tileId=instance.tileId;a.hasHeight=true;a.height=profile.assets.at(a.tileId).height;
  f->assetMetadata.push_back(a);
  for(int y=0;y<8;y++)for(int x=0;x<8;x++){auto &px=f->mainPixels[y*8+x];px.instanceId=1;px.tilePixel=(t.vflip?7-y:y)*8+(t.hflip?7-x:x);}
  auto ac=c;ac.backgroundScrollX=preview.cell%64*8;ac.backgroundScrollY=preview.cell/64*8;
  S9xRemasterApplyDungeonHeightMap(*f,ac,*m);
  assert(f->assetMetadata[0].height==a.height);
  for(const auto &px:f->mainPixels){const auto &applied=f->tileInstances[px.instanceId-1];
   assert(applied.hasPlacementHeight && applied.placementHeight==preview.height && applied.heightOffset==m->offsets[preview.cell]);}
 }
 // Canonical diagonal is an exact unit ramp, not the old half-speed repeated bands.
 for(auto xy:{std::array<int,2>{11,5},{52,5},{11,46},{52,46}}) {
  const int i=xy[1]*64+xy[0];const auto &t=(*tiles)[i];
  assert(t.diagonalHeightPreview && t.height[0]==47 && t.height[63]==33);
  assert(m->offsets[i]!=255);
 }
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)assert((*tiles)[7*64+11].height[y*8+x]==24+std::max(0,7-x-y));
 // Two-column six-word ownership survives ONE end overwrite, never claims the
 // replacement door or a lone tile. Both flip signs retain canonical arrays.
 auto original=input;input[8+(4*64+11)*2]=0xff;input[9+(4*64+11)*2]=3;
 auto overwritten=solve();assert(!(*tiles)[4*64+11].diagonalHeightPreview);
 assert((*tiles)[5*64+11].diagonalHeightPreview);
 input=original;
 const int isolated=30*64+30;input[8+isolated*2]=0x81;input[9+isolated*2]=8;
 solve();assert(!(*tiles)[isolated].diagonalHeightPreview);input=original;
 // A disagreeing authored cardinal cross-section invalidates every proposal.
 auto id=RemasterTileContentId{UINT64_C(0x62112d6380887711),1,4};auto saved=profile.assets.at(id).height;
 profile.assets.at(id).height[0]++;solve();for(const auto &t:*tiles)assert(!t.diagonalHeightPreview);
 profile.assets.at(id).height=saved;
 // Explicit opening semantics veto local preview; do not reclassify a door.
 auto diagonalId=RemasterTileContentId{UINT64_C(0x503ab068dfa941d3),1,4};
 auto role=profile.assets.at(diagonalId).tileRole;profile.assets.at(diagonalId).tileRole=RemasterTileRole::Door;
 solve();assert((*tiles)[5*64+11].surface==RemasterDungeonFloorContext::Surface::Door && !(*tiles)[5*64+11].diagonalHeightPreview);
 profile.assets.at(diagonalId).tileRole=role;
 // Repeated source words cannot determine which structural band owns pixels.
 auto savedTemplate=tmpl;for(int off:{0x420,0x448,0x470})for(int k=1;k<4;k++) {
  tmpl[8+off+k*2]=tmpl[8+off+2];tmpl[9+off+k*2]=tmpl[9+off+2];
 }
 solve();assert(!(*tiles)[5*64+11].diagonalHeightPreview);tmpl=savedTemplate;
 assert(S9xRemasterSerializeProfile(profile,after,errors));assert(before==after);
 std::cout<<"PASS 122 ordered transformed diagonal previews, exact ramps, overwritten/isolated/door/cardinal disagreement rejection, immutable profile\n";
}
