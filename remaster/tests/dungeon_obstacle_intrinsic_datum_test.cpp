// Explicit grouping establishes support ownership, not equality of neighboring
// relief surfaces. A normalized-looking ramp must not borrow another member's
// datum, including when the other gameplay plane has reachable hidden support.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 auto gfx=std::make_unique<std::array<uint8_t,65536>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();RemasterProfile profile;
 RemasterTileContentId ids[4];
 for(int n=0;n<4;n++) {
  uint8_t pixels[64];std::fill_n(pixels,64,n);
  for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[ids[n]];a.tileId=ids[n];a.hasHeight=true;
  a.tileRole=n?RemasterTileRole::Obstacle:RemasterTileRole::Floor;
 }
 RemasterObstacleGroup group;group.name="sections";
 // One signed relief band and another independently authored zero-datum band.
 // A synthetic *opposite* per-member H flip intentionally keeps their displayed
 // uphill direction equal without making their canonical data equal.
 for(int y=0;y<2;y++)for(int x=0;x<3;x++)group.members.push_back({ids[x+1],x,y,0,x==2,bool(y),x==0});
 profile.obstacleGroups={group};
 C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=64;
 c.constructedMaps=words->data();c.graphics=gfx->data();c.collisionAttributes=attrs->data();c.placements=tiles->data();
 int cases=0;
 for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++)for(int sign:{-1,1})for(int mode=0;mode<4;mode++) {
  words->fill(0);attrs->fill(1);c.placementBackgroundIndex=bg;c.linkPlane=0;c.stairCount=mode==3?1:0;
  if(c.stairCount)c.stairs[0]={40,40,0,0,true,true};
  for(int y=6;y<25;y++)for(int x=6;x<30;x++)(*attrs)[y*64+x]=0;
  if(mode==3)for(int y=6;y<25;y++)for(int x=6;x<30;x++)(*attrs)[4096+y*64+x]=0;
  for(int k=0;k<64;k++) {
   profile.assets[ids[1]].height[k]=0;
   profile.assets[ids[2]].height[k]=sign*(12+k%8);
   profile.assets[ids[3]].height[k]=sign*(7-k%8);
  }
  for(const auto &m:group.members) {
   const int x=16+((flip&1)?-m.x:m.x),y=12+((flip&2)?-m.y:m.y),cell=y*64+x;
   const int word=int(m.tileId==ids[1]?1:m.tileId==ids[2]?2:3)|((m.hflip!=bool(flip&1))?0x4000:0)|((m.vflip!=bool(flip&2))?0x8000:0);
   (*words)[cell*2]=word;(*words)[cell*2+1]=word>>8;(*attrs)[cell]=1;
  }
  if(mode==1) {const int cell=12*64+16;(*words)[cell*2]=0;(*words)[cell*2+1]=0;} // overwritten ownership
  if(mode==2)profile.obstacleGroups.push_back(group),profile.obstacleGroups.back().name="competing";
  S9xRemasterDungeonPlacements(c,profile,*tiles);need(S9xRemasterBuildDungeonHeightMap(c,50,*map),"solve");
  if(mode==0 || (mode==3 && bg==1)) {
   need(map->solvedAuthoredObstacleCells==6,"complete transformed footprint");
   for(const auto &m:group.members) {
    const int cell=(12+((flip&2)?-m.y:m.y))*64+16+((flip&1)?-m.x:m.x);
    need(map->offsets[cell]==0 && !map->fallbackDirections[cell],"no manufactured per-member seam datum");
    need((*tiles)[cell].height==profile.assets.at(m.tileId).height,"preserve signed canonical relief");
   }
   if(mode==3)for(const auto &e:map->authoredObstacleEvidence)need(e.plane==0 && !e.conflict,"distinct hidden lower support is not the obstacle owner");
  }else need(map->solvedAuthoredObstacleCells==0,"overwritten/ambiguous ownership must not infer seam datum");
  profile.obstacleGroups={group};cases++;
 }
 std::cout<<"PASS "<<cases<<" intrinsic-datum cases: signed arrays, both BGs, H/V plus independent member flips, overwrites, competing claims and overlapping planes; no invented seam equality\n";
 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
