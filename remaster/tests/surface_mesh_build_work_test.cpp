// Frozen pre-optimization algorithm is kept independently in the evidence closure.
// Define REFERENCE_HEADER to run exact differential checks against that closure.
#define REMASTER_MESH_COUNT_WORK 1
#include "../runtime/surface_mesh.h"
#ifdef REFERENCE_HEADER
#undef REMASTER_SURFACE_MESH_H
#define RemasterSurfaceMesh FrozenSurfaceMesh
#include REFERENCE_HEADER
#undef RemasterSurfaceMesh
#endif
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
 using namespace RemasterSurfaceMesh;
 unsigned seed=17171;
 auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
 for(unsigned iteration=0;iteration<320;iteration++) {
  const unsigned w=1+random()%71,h=1+random()%61;
  std::vector<Sample> samples(w*h);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
   auto &s=samples[y*w+x];s.known=random()%9!=0;s.coverage=float(random()%3)/2;
   s.height=iteration%2?float(int(random()%81)-40):float(x*2+y*3);
   s.sheet=random()%5!=0;s.domain=random()%4;s.solidWall=random()%7==0;
   s.wallBase=float(int(random()%19)-9);s.fallbackGround=random()%2;
  }
 auto cells=build(w,h,samples,iteration%3?0.25f:1.f);
  for(const auto &c:cells) {
   for(float corner:c.corners)assert(std::isfinite(corner));
   assert(std::isfinite(c.normal[0]) && std::isfinite(c.normal[1]) && std::isfinite(c.normal[2]));
  }
#ifdef REFERENCE_HEADER
  std::vector<FrozenSurfaceMesh::Sample> old(samples.size());
  for(size_t i=0;i<samples.size();i++) {auto &a=samples[i];auto &b=old[i];
   b.height=a.height;b.coverage=a.coverage;b.known=a.known;b.sheet=a.sheet;
   b.domain=a.domain;b.solidWall=a.solidWall;b.wallBase=a.wallBase;b.fallbackGround=a.fallbackGround;}
  auto expected=FrozenSurfaceMesh::build(w,h,old,iteration%3?0.25f:1.f);
  assert(cells.size()==expected.size());assert(!std::memcmp(cells.data(),expected.data(),cells.size()*sizeof(Cell)));
#endif
 }
#ifdef REMASTER_MESH_BUILD_WORK
 // Only the perimeter needs the generic vertex solver for a connected flat.
 std::vector<Sample> flat(64*64);for(auto &s:flat){s.known=true;s.coverage=1;s.domain=1;}
 RemasterSurfaceMesh::weldRootSearches=0;build(64,64,flat,.25f);
 assert(RemasterSurfaceMesh::weldRootSearches<64*128);
 // Independent plane oracle includes true border corners and every interior
 // vertex, with a steep continuation requiring the original edge evidence.
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)flat[y*64+x].height=float(2*x+3*y);
 const auto ramp=build(64,64,flat,.25f);
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++) {
  const auto &c=ramp[y*64+x];
  assert(c.corners[0]==float(2*x+3*y)-2.5f);
  assert(c.corners[2]==float(2*x+3*y)+2.5f);
 }
#else
 assert(!"baseline performs repeated vertex root searches; optimized work contract absent");
#endif
 std::puts("mesh build exact differential and bounded weld work PASS");
}
