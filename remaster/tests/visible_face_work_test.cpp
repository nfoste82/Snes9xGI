// Frozen complete-output oracle generated at clean 32a59dc69c958e65ce11a8f6f6b26d9a6e2bdba0.
// Fingerprints include serialized frame (all ordered instances/claims/contexts
// and rasters), explicit observed BG/depth buffers, and physical maps.
#define REMASTER_OOB_NO_MAIN
#include "dungeon_out_of_bounds_test.cpp"
#include <new>
#include "height_selection_reference.h"
static size_t allocatedBytes=0;
void *operator new(size_t n) {allocatedBytes+=n;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,size_t) noexcept {std::free(p);}
static uint64_t fingerprint(const PlateauFixture &f) {
 std::vector<uint8_t> bytes;need(S9xSerializeRemasterFrame(f.frame,bytes),"serialize oracle");
 uint64_t h=UINT64_C(14695981039346656037);
 auto add=[&](uint64_t n){for(int k=0;k<8;k++){h=(h^uint8_t(n))*UINT64_C(1099511628211);n>>=8;}};
 for(auto b:bytes)add(b);
 for(int bg=0;bg<4;bg++) {
  add(f.frame.observedBgPixels[bg].size());
  for(auto p:f.frame.observedBgPixels[bg]){add(p.owner);add(p.instanceId);add(p.tilePixel);}
  for(auto d:f.frame.observedBgDepth[bg])add(d);
 }
 for(const auto &map:f.heights) {
  for(auto n:map.offsets)add(n);
  for(const auto &a:map.planeOffsets)for(auto n:a)add(n);
  for(const auto &a:map.occupiedFloorOwners)for(auto n:a)add(n);
  for(const auto &a:map.structuralPlaneOffsets)for(auto n:a)add(n);
 }
 return h;
}
int main() {
 { // Independent full metadata source and frozen visible-precedence checks.
  auto f=std::make_unique<RemasterFrame>();f->width=64;f->height=1;
  f->profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
  f->assetMetadata.resize(8);f->tileInstances.resize(1024);
  for(size_t n=0;n<8;n++) {
   auto &m=f->assetMetadata[n];m.tileId={n+1,1,4};m.tileRole=n&1?RemasterTileRole::OutOfBounds:RemasterTileRole::Wall;
   m.hasHeight=n%3;m.height.fill(n);m.hasNormals=n%2;m.geometryByFlip=n&1;m.geometryByFacing=true;
   for(int k=0;k<4;k++)for(auto *g:{&m.flipGeometry[k],&m.facingGeometry[k]}){g->hasHeight=k&1;g->height.fill(20+k);g->hasNormals=k&2;}
  }
  for(size_t n=0;n<1024;n++) {
   auto &i=f->tileInstances[n];i.tileId={n%9+1,1,4};i.source=n%7?RemasterSourceType::Background:RemasterSourceType::Object;
   i.sourceIndex=n%3;i.geometryFacing=n%6;i.hFlip=n&1;i.vFlip=n&2;i.geometryStatus=RemasterOccurrenceGeometryStatus(n%4);
   i.placementTileRole=n&8?RemasterTileRole::OutOfBounds:RemasterTileRole::Unknown;
   i.hasPlacementHeight=n%3==0;i.placementHeight.fill(0);i.hasPlacementNormals=n%5==0;
   if(n%5==0){auto &c=f->contextGeometry[n+1];c.hasHeight=n&1;c.height.fill(0);c.hasNormals=n&2;}
  }
  const auto compact=S9xRemasterVisibleFaceSources(*f);RemasterFrameChannelResolver full(*f);
  for(size_t n=0;n<1024;n++) {
   const auto &a=compact[n];const auto &b=full.effective[n];
   need(a.tileRole==b.tileRole && a.hasHeight==b.hasHeight && a.hasNormals==b.hasNormals,"compact source presence/role");
   if(a.hasHeight)need(a.height==b.height,"compact source height");
  }
  f->mainPixels.resize(64);f->subPixels.resize(64);f->scenePixels.resize(64);
  for(int b=0;b<4;b++){f->observedBgPixels[b].resize(64);f->observedBgDepth[b].resize(64);}
  uint32_t rng=123;
  for(int pass=0;pass<256;pass++)for(size_t at=0;at<64;at++) {
   auto sample=[&](){rng=rng*1664525+1013904223;RemasterFramePixel p;p.instanceId=rng%1027;p.tilePixel=(rng>>12)%67;
    if(p.instanceId && p.instanceId<=1024){const auto &i=f->tileInstances[p.instanceId-1];p.owner=uint32_t(i.source)<<24|uint32_t(i.sourceIndex)<<16;}return p;};
   f->mainPixels[at]=sample();f->subPixels[at]=sample();f->scenePixels[at]=pass&1?sample():f->mainPixels[at];
   for(int b=0;b<4;b++){f->observedBgPixels[b][at]=sample();f->observedBgDepth[b][at]=rng%4;}
   for(bool scene:{false,true}) {
    const auto a=heightSelectionReference(*f,at,scene,full.metadata).visible;
    const auto b=S9xRemasterSelectVisibleOccurrence(*f,at,scene,true);
    need(a.owner==b.owner && a.instanceId==b.instanceId && a.tilePixel==b.tilePixel,"frozen visible precedence");
   }
  }
  std::cout<<"source_comparisons=1024 frozen_visible_comparisons=32768\n";
 }
 uint64_t total=0;size_t worst=0;
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int mode=0;mode<12;mode++) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,mode&1);
  std::set<RemasterTileContentId> ids;
  auto &metadata=f->frame.assetMetadata;
  metadata.erase(std::remove_if(metadata.begin(),metadata.end(),[&](const auto &m){return !ids.insert(m.tileId).second;}),metadata.end());
  for(int b=0;b<4;b++){f->frame.observedBgPixels[b]=f->frame.mainPixels;f->frame.observedBgDepth[b].assign(f->frame.mainPixels.size(),b%2);}
  if(mode==1)f->local(0);
  if(mode==2)f->attrs.fill(1);
  if(mode==3)for(auto [cell,id]:f->added){auto &c=f->frame.contextGeometry[id];c.hasHeight=true;c.height.fill(0);auto &l=f->frame.contextLocations[id];l.area=7;l.background=bg;}
  if(mode==4)for(auto [cell,id]:f->added){auto &i=f->frame.tileInstances[id-1];i.hasPlacementHeight=true;i.placementHeight.fill(0);}
  if(mode==5 || mode==6)for(auto [cell,id]:f->added)f->frame.tileInstances[id-1].geometryStatus=mode==5?RemasterOccurrenceGeometryStatus::Unresolved:RemasterOccurrenceGeometryStatus::Conflict;
  if(mode==7)for(auto [cell,id]:f->added)f->tiles[bg][cell].stairSourceAmbiguous=true;
  if(mode==8 || mode==9) {
   auto i=f->frame.tileInstances.front();i.source=mode==8?RemasterSourceType::Background:RemasterSourceType::Object;i.sourceIndex=2;
   f->frame.tileInstances.push_back(i);
   f->frame.subPixels=f->frame.mainPixels;
   for(auto &p:f->frame.mainPixels)if(p.instanceId){p.owner=uint32_t(i.source)<<24|2u<<16;p.instanceId=f->frame.tileInstances.size();}
  }
  if(mode==10)for(size_t at=0;at<f->frame.mainPixels.size();at+=7)f->frame.mainPixels[at]=f->frame.scenePixels[at]={};
  if(mode==11)for(auto &m:f->frame.assetMetadata){m.geometryByFlip=true;m.flipGeometry[flip].hasHeight=true;m.flipGeometry[flip].height=m.height;}
  // Force growth during clone publication, rather than accidentally relying on spare capacity.
  f->frame.tileInstances.shrink_to_fit();
  const auto before=allocatedBytes;f->run();worst=std::max(worst,allocatedBytes-before);
  const auto h=fingerprint(*f);total=total*UINT64_C(1099511628211)^h;
 }
 std::cout<<"frozen_complete_output="<<total<<" worst_invocation_allocated_bytes="<<worst<<std::endl;
 // Fill from the pre-change run, never regenerate after production changes.
 need(total==UINT64_C(18198169831120924552),"frozen complete-output parity");
 need(worst<950000,"visible-face consumer materializes full per-instance metadata");
}
