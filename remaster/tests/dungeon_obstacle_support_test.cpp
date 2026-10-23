#include "../runtime/remaster.h"
#include <cassert>
int main(){
 using Surface=RemasterDungeonFloorContext::Surface;
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 // Unknown artwork with authored relief over ordinary support is NOT a
 // classified floor. Two landing components are related by a signed stair.
 for(int y=10;y<=24;y++)for(int x=10;x<=20;x++){
  int i=y*64+x;(*attrs)[i]=0;(*tiles)[i].hasHeight=true;(*tiles)[i].height.fill(3);
 }
 for(int y=15;y<=18;y++)for(int x=10;x<=20;x++)(*attrs)[y*64+x]=1;
 int obstacle=12*64+15;(*attrs)[obstacle]=0x27;
 auto &t=(*tiles)[obstacle];t.surface=Surface::Obstacle;t.hasHeight=true;
 for(int k=0;k<64;k++)t.height[k]=k%8;
 const auto relief=t.height;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.linkX=12*8;c.linkY=20*8;c.collisionAttributes=attrs->data();c.placements=tiles->data();
 c.stairCount=1;c.stairs[0].x=13;c.stairs[0].y=15;c.stairs[0].highIsNorth=true;
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 for(int flip=0;flip<4;flip++){
  t.hflip=flip&1;t.vflip=flip&2;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
  assert(m->planeOffsets[0][obstacle]==50 && m->offsets[obstacle]==50);
  assert(!m->fallbackDirections[obstacle] && t.height==relief);
  RemasterFrame frame;frame.width=frame.height=8;frame.mainPixels.resize(64);frame.subPixels.resize(64);
  RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
  frame.tileInstances.push_back(instance);for(auto &px:frame.mainPixels)px.instanceId=1;
  c.backgroundScrollX=15*8;c.backgroundScrollY=12*8;
  S9xRemasterApplyDungeonHeightMap(frame,c,*m);
  assert(frame.tileInstances[0].heightOffset==50 && !frame.tileInstances[0].hasPlacementHeight);
 }
 // An overlapping nonblocking lower plane retains its own support0.
 for(int y=10;y<=24;y++)for(int x=10;x<=20;x++)(*attrs)[4096+y*64+x]=0;
 c.stairs[0].changesPlane=true;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->planeOffsets[0][obstacle]==50 && m->planeOffsets[1][obstacle]==0);
 assert(m->offsets[obstacle]==50 && !m->fallbackDirections[obstacle]);
 // Solid rail above two independently solved contacts: its geometric solve
 // owns visible base50; lower-plane contact0 must not erase that result.
 (*attrs)[obstacle]=2;(*attrs)[4096+obstacle]=1;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->planeOffsets[0][obstacle]==50 && m->planeOffsets[1][obstacle]==0);
 assert(m->offsets[obstacle]==50 && !m->fallbackDirections[obstacle]);
 (*attrs)[obstacle]=0x27;(*attrs)[4096+obstacle]=0;
 // Unknown usage is not obstacle evidence: no automatic lift through artwork.
 t.surface=Surface::Unknown;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->planeOffsets[0][obstacle]==255 && m->planeOffsets[1][obstacle]==0 && m->offsets[obstacle]==0);
 // No wider obstacle flood: an isolated authored tile remains unsupported.
 int isolated=2*64+2;(*tiles)[isolated]=t;(*tiles)[isolated].surface=Surface::Obstacle;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->offsets[isolated]==255);
}
