// Work-count regression: no timing threshold and no alteration of the frozen
// complete-output/memory oracle in visible_face_work_test.cpp.
#include <cstddef>
#ifndef REMASTER_VISIBLE_FACE_IDENTITY_CHECK
static size_t annotationIdentityChecks=0;
#define REMASTER_VISIBLE_FACE_IDENTITY_CHECK() (++annotationIdentityChecks)
#endif
#define REMASTER_OOB_NO_MAIN
#include "dungeon_out_of_bounds_test.cpp"
int main() {
 for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++) {
  auto f=std::make_unique<PlateauFixture>(axis,flip,bg,0);
  std::set<RemasterTileContentId> ids;
  auto &metadata=f->frame.assetMetadata;
  metadata.erase(std::remove_if(metadata.begin(),metadata.end(),[&](const auto &m){return !ids.insert(m.tileId).second;}),metadata.end());
  f->frame.subPixels=f->frame.mainPixels;
  for(auto &r:f->frame.observedBgPixels)r=f->frame.mainPixels;
  for(auto &r:f->frame.observedBgDepth)r.assign(f->frame.mainPixels.size(),1);
  // Distinct occurrences in one cell evict each other. A matching source may
  // reappear later; never assume a cell has one permanent raster owner.
  auto original=f->frame.mainPixels[f->at(f->far)].instanceId;
  auto other=f->frame.tileInstances[original-1];other.tileNumber^=1;
  f->frame.tileInstances.push_back(other);auto invalid=uint32_t(f->frame.tileInstances.size());
  f->frame.observedBgPixels[0][f->at(f->far)].instanceId=invalid;
  // Same actual occurrence in two different cells: the second placement has a
  // different identity. Instance-only memoization would wrongly annotate it.
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)
   f->frame.observedBgPixels[1][f->at(f->plateau)+y*f->frame.width+x].instanceId=original;
  f->frame.tileInstances.shrink_to_fit();
  annotationIdentityChecks=0;f->run();
  std::cout<<"axis="<<axis<<" flip="<<flip<<" bg="<<bg<<" identity_checks="<<annotationIdentityChecks<<'\n';
  need(annotationIdentityChecks<200,"annotation repeats invariant identity checks per pixel");
  need(f->frame.observedBgPixels[0][f->at(f->far)].instanceId==invalid,"multiple instances per cell cannot reuse matching identity");
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)
   need(f->frame.observedBgPixels[1][f->at(f->plateau)+y*f->frame.width+x].instanceId==original,"same instance across cells cannot reuse matching identity");
  // Also print a complete ordered-output digest for separately compiled frozen
  // baseline differential runs. Padding is excluded by frame serialization.
  std::vector<uint8_t> bytes;need(S9xSerializeRemasterFrame(f->frame,bytes),"serialize annotation cases");
  uint64_t digest=UINT64_C(14695981039346656037);
  for(auto b:bytes)digest=(digest^b)*UINT64_C(1099511628211);
  for(const auto &r:f->frame.observedBgPixels)for(auto p:r)for(uint64_t v:{uint64_t(p.owner),uint64_t(p.instanceId),uint64_t(p.tilePixel)})
   for(int k=0;k<8;k++){digest=(digest^uint8_t(v))*UINT64_C(1099511628211);v>>=8;}
  std::cout<<"ordered_output="<<digest<<'\n';
 }
}
