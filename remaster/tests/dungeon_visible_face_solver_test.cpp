#include "../runtime/dungeon_visible_face.h"
#include <cstdlib>
#include <iostream>
using namespace S9xRemasterVisibleFace;
static void need(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}

// Six separately owned bands; the first two are BG2 and the others BG1.
// Both physical planes may use identical artwork: they remain independent
// because only established visible-region contacts enter this graph.
int main(){
 for(int axis=0;axis<2;axis++)for(int reverse=0;reverse<2;reverse++)for(int flip=0;flip<4;flip++)for(int firstBg=0;firstBg<2;firstBg++) {
  std::vector<Region> r(7); // node 6 is a same-art underpass, not this face
  for(auto &t:r){t.established=true;t.hflip=flip&1;t.vflip=flip&2;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++){
    int displayed=axis?(t.vflip?7-y:y):(t.hflip?7-x:x);
    t.height[y*8+x]=reverse?displayed:7-displayed;
   }}
  const int step=reverse?1:-1,dx=axis?0:1,dy=axis?1:0;
  std::vector<Contact> edges;
  for(int i=0;i<5;i++)edges.push_back({i,i+1,dx,dy,step});
  // Last wall pixel meets approved occupied floor 0; reversed construction
  // anchors the opposite end. firstBg is deliberately not used as an owner.
  int foot=reverse?0:5, fx=axis?0:(reverse?0:7),fy=axis?(reverse?0:7):0;
  auto result=solve(r,edges,{{foot,fx,fy,0},{6,fx,fy,24}});
  for(int i=0;i<6;i++){
   need(result.status[i]==Status::Solved,"full multi-band face solved");
   need(result.base[i]==8*(reverse?i:5-i),"signed floor-derived band");
  }
  need(result.base[6]==24,"same-art physical underpass stays independent");
  auto contradiction=solve(r,edges,{{foot,fx,fy,0},{foot,fx,fy,12},{6,fx,fy,24}});
  need(contradiction.status[0]==Status::Conflict && contradiction.base[0]==INT_MIN &&
       contradiction.status[6]==Status::Solved && contradiction.base[6]==24,"contradictory exposed anchors do not alter independent plane");
  auto unanchored=solve(r,edges,{{6,fx,fy,24}});
  need(unanchored.status[0]==Status::Unanchored && unanchored.base[0]==INT_MIN,"unanchored face is explicit");
  r[2].established=false;
  auto overwritten=solve(r,edges,{{foot,fx,fy,0},{6,fx,fy,24}});
  need(overwritten.status[0]==Status::Conflict && overwritten.status[6]==Status::Solved,"ambiguous or overwritten owner blocks whole face");
  r[2].established=true;r[2].height[0]++;
  auto malformed=solve(r,edges,{{foot,fx,fy,0}});
  need(malformed.status[0]==Status::Conflict,"malformed authored edge rejected");
 }
 // Perpendicular corner branch: its edge must agree with the solved tangent,
 // not merely a matching word or a single selected sample.
 std::vector<Region> corner(3);for(auto &r:corner)r.established=true;
 for(int y=0;y<8;y++)for(int x=0;x<8;x++){
  corner[0].height[y*8+x]=7-x;
  corner[1].height[y*8+x]=7-x;
  corner[2].height[y*8+x]=std::max(7-x,7-y);
 }
 auto result=solve(corner,{{0,1,1,0,-1},{1,2,0,-1,0}},{{1,7,0,0}});
 need(result.status[0]==Status::Solved && result.base[0]==8 && result.base[2]==0,"corner branch contact and signed floor anchor");
 std::cout<<"PASS exposed-face signed solver; no physical floor updates\n";
}
