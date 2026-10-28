// Diagnostics must expose distinct numeric and ownership failure classes and
// must not change a solve. Independent signed/zero/invalid-edge witnesses.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#include "../runtime/dungeon_visible_face.h"
#include <cassert>
#include <iostream>
using namespace S9xRemasterVisibleFace;
int main() {
 for(int flip=0;flip<4;flip++)for(int mode=0;mode<4;mode++) {
  Region a,b;a.established=b.established=true;
  a.hflip=b.hflip=flip&1;a.vflip=b.vflip=flip&2;
  a.height.fill(0);b.height.fill(-8);
  std::vector<Region> regions{a,b};
  std::vector<Contact> contacts{{0,1,1,0,0,false,255}};
  std::vector<FloorContact> floors{{0,0,0,16},{1,7,7,16+(mode==1)}};
  // Equal absolute floor samples on independent nodes imply the same datum16.
  // A genuinely inconsistent cycle is different from a bad admitted seam.
  std::vector<DatumContact> datums;
  if(mode==2)datums.push_back({0,1,7});
  if(mode==3)contacts[0].samples=0;
  const auto before=solve(regions,contacts,floors,datums);
  EquationTrace trace;equationTrace=&trace;
  const auto after=solve(regions,contacts,floors,datums);equationTrace=nullptr;
  assert(before.base==after.base && before.status==after.status);
  assert(trace.relative.size()==2 && trace.datum.size()==2);
  if(mode==0) {
   assert(after.status[0]==Status::Solved && after.base[0]==16 && after.base[1]==24);
   assert(trace.relative[0]==0 && trace.relative[1]==8 && trace.datum[0]==16);
   assert(!trace.cycle[0] && !trace.anchorConflict[0] && !trace.invalid[0]);
  }
  if(mode==1)assert(trace.anchorConflict[0] && !trace.cycle[0]);
  if(mode==2)assert(trace.cycle[0]);
  if(mode==3)assert(trace.invalid[0] && trace.invalid[1]);
 }
 std::cout<<"PASS 16 diagnostic/undecorated signed solver parity cases\n";
}
