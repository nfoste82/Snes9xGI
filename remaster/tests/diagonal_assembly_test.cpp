#include "../runtime/diagonal_assembly.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 int cases=0;
 for(int extent:{5,7,9,12,20})for(int width:{2,4,7})for(int rise:{37,60,83,147}) {
  std::vector<RemasterDiagonalAssemblyMember> members;
  for(int col=0;col<width;col++)for(int row=0;row<extent;row++) {
   // Repeated face identity/dispatch slot is intentionally absent from kernel
   // identity: one actual writer and placed phase supply the evidence instead.
   members.push_back({(5+col+row)*64+5+col,rise+7-row*8,17,true,true,true,false});
  }
  std::vector<RemasterDiagonalAssemblyGeometry> geometry;
  check(S9xRemasterSolveDiagonalAssembly({0,rise,true,true,true,false},members,geometry),"arbitrary extent");
  check(geometry.size()==members.size(),"all witnessed members");
  for(const auto &g:geometry)for(int hflip=0;hflip<2;hflip++)for(int vflip=0;vflip<2;vflip++)
   for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    const int xx=hflip?7-x:x,yy=vflip?7-y:y;
    check(g.height[yy*8+xx]==std::max(0,std::min(rise,g.intercept-xx-yy)),"transformed canonical model");
    for(int support:{-37,0,24,83})check(support+g.height[yy*8+xx]==support+std::max(0,std::min(rise,g.intercept-xx-yy)),"signed datum composition");
   }
  members[3].finalWordVerified=false;
  check(S9xRemasterSolveDiagonalAssembly({0,rise,true,true,true,false},members,geometry) && geometry.size()+1==members.size(),"partial overwrite excludes replaced member");
  members[4].ambiguous=true;
  check(!S9xRemasterSolveDiagonalAssembly({0,rise,true,true,true,false},members,geometry),"ambiguous assembly rejects");
  members[4].ambiguous=false;
  check(!S9xRemasterSolveDiagonalAssembly({0,rise,true,false,true,false},members,geometry),"missing named top rejects");
  check(!S9xRemasterSolveDiagonalAssembly({0,rise,false,true,true,false},members,geometry),"missing support rejects");
  cases++;
 }
 std::cout<<"PASS "<<cases<<" arbitrary extent/rise/width assemblies, four flips, signed datums, repeated writer slots, partial overwrite and ambiguity/top/support rejection\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
