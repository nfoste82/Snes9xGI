// Hidden lower artwork cannot join upper-plane structural seams.
#include "remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 std::array<uint8_t,8192> attr;attr.fill(1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.placementBackgroundIndex=0;c.linkPlane=1;c.linkX=80;c.linkY=160;
 c.placements=tiles->data();c.collisionAttributes=attr.data();
 c.stairCount=1;c.stairs[0].x=10;c.stairs[0].y=15;c.stairs[0].highIsNorth=true;c.stairs[0].changesPlane=true;
 // Two support planes with a signed stair relationship; hidden BG1 wall
 // art would incorrectly provide a second, 32-unit upper/lower relationship.
 for(int p=0;p<2;p++)for(int y:{12,19})for(int yy=y;yy<y+3;yy++)for(int x=8;x<18;x++) {
  int i=yy*64+x;attr[p*4096+i]=0;auto &t=(*tiles)[i];t.surface=Surface::Floor;t.hasHeight=true;
  t.distinctLowerUsage=true;
 }
 for(int y=15;y<19;y++)for(int x=8;x<18;x++) {
  auto &t=(*tiles)[y*64+x];t.surface=Surface::Wall;t.hasHeight=true;t.distinctLowerUsage=true;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
 }
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][17*64+10]==255);
 assert(m->structuralPlaneOffsets[1][17*64+10]==8);
 assert(m->structuralPlaneOffsets[1][18*64+10]==0);
 const auto authored=(*tiles)[17*64+10].height;
 // Runtime application uses the actual solved base, not a distance overlay.
 RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
 RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=0;
 f.tileInstances.push_back(instance);for(auto &px:f.mainPixels)px.instanceId=1;
 c.backgroundScrollX=80;c.backgroundScrollY=17*8;
 S9xRemasterApplyDungeonHeightMap(f,c,*m,nullptr,nullptr,0);
 assert(f.tileInstances[0].heightOffset==8 && !f.tileInstances[0].hasPlacementHeight);
 assert((*tiles)[17*64+10].height==authored);
 // Transform preserves displayed authored seams.
 for(int y=15;y<19;y++)for(int x=8;x<18;x++) {
  auto &t=(*tiles)[y*64+x];t.vflip=true;
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=yy;
 }
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[1][17*64+10]==8);
 // Authored local rebasing must change only its contextual offset, not
 // manufacture a fixed distance-derived height or edit the local samples.
 auto saved=(*tiles)[17*64+10];
 for(auto &h:(*tiles)[17*64+10].height)h+=1;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 // Uniform local rebasing is consistent and must simply change its offset.
 assert(m->structuralPlaneOffsets[1][17*64+10]==7);
 (*tiles)[17*64+10]=saved;
 // No lower support evidence: do not assign this wall an upper base.
 for(int y:{12,19})for(int yy=y;yy<y+3;yy++)for(int x=8;x<18;x++) {
  attr[4096+yy*64+x]=1;(*tiles)[yy*64+x].structuralFloorPlaneMask=1;
 }
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->offsets[17*64+10]==255);
 // Identical paired artwork is eligible on the upper plane; BG2 is not
 // restricted by lower-usage flags. No room/hash exceptions in the rule.
 for(auto &t:*tiles)t.distinctLowerUsage=false;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][17*64+10]!=255);
 // A same-plane room must retain ordinary BG1 assemblies even if BG2
 // differs: no independently observed lower gameplay plane owns that art.
 c.linkPlane=0;c.stairCount=0;
 for(auto &t:*tiles)t.distinctLowerUsage=true;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][17*64+10]!=255);
 std::cout<<"PASS BG ownership, independent planes, authored transforms, runtime offsets, unsupported rejection\n";
}
