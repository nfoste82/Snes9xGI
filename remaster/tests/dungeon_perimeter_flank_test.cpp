// Intact ROM faces beside authored foot witnesses; not hash/neighbor copying.
#include "../runtime/remaster.h"
#include <cassert>
int main() {
 using S=RemasterDungeonFloorContext::Surface;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto templates=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 RemasterProfile profile;
 std::array<RemasterTileContentId,9> ids;
 for(int n=0;n<9;n++) {
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*graphics)[n*32+(b/2)*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  auto &a=profile.assets[ids[n]];a.hasHeight=true;
  for(int k=0;k<64;k++)a.height[k]=n==8?23-k%8:24+(n%4)*8+7-k%8;
 }
 profile.assets[ids[8]].tileRole=RemasterTileRole::Wall;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.dungeonTemplateData=templates->data();
 auto put=[&](int i,int word){(*maps)[i*2]=word;(*maps)[i*2+1]=word>>8;};
 for(int kind:{2,3})for(int flip=0;flip<4;flip++) {
  // Both ROM orientations, canonical H/V samples, and priority-only writes.
  maps->fill(0);templates->fill(0xff);
  const int offset=kind==2?0x2c8:0x2d8,start=kind==2?0:1;
  const int faceX=10+start,footX=kind==2?13:10;
  for(int n=0;n<8;n++) {
   int word=n|(flip<<14);
   (*templates)[offset+n*2]=word;(*templates)[offset+n*2+1]=word>>8;
   put((20+n/4)*64+10+n%4,word^0x2000);
  }
  for(int yy=0;yy<2;yy++)put((20+yy)*64+footX,8|(flip<<14));
  // Authored canonical arrays are transformed by placement, not rewritten.
  for(int n=0;n<9;n++)for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
   int displayedX=flip&1?7-x:x;
   profile.assets[ids[n]].height[y*8+x]=n==8?
    (kind==2?23-displayedX:16+displayedX):
    (kind==2?24+(2-n%4)*8+7-displayedX:24+(n%4-1)*8+displayedX);
  }
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+faceX].surface==S::Wall);
  assert((*tiles)[20*64+faceX].wallTemplateOffset==offset);
  assert((*tiles)[20*64+faceX].height==profile.assets[ids[start]].height);
  // Changed identity/flip and explicit floor ownership veto the full footprint.
  put(21*64+faceX,0x3ff);
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+faceX].surface==S::Unknown);
  put(21*64+faceX,(4+start)|(flip<<14));
  profile.assets[ids[start]].tileRole=RemasterTileRole::Floor;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+faceX+1].surface!=S::Wall);
  profile.assets[ids[start]].tileRole=RemasterTileRole::Unknown;
  // Incompatible authored seam cannot witness ownership, even with matching art.
  auto footRelief=profile.assets[ids[8]].height;
  const int edge=kind==2?0:7;
  profile.assets[ids[8]].height[flip&1?7-edge:edge]++;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+faceX].surface==S::Unknown);
  profile.assets[ids[8]].height=footRelief;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  // Without an authored wall witness, an intact artwork sequence is unknown.
  put(21*64+footX,0x3ff);
  profile.assets[ids[8]].tileRole=RemasterTileRole::Unknown;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  assert((*tiles)[20*64+faceX].surface==S::Unknown);
  profile.assets[ids[8]].tileRole=RemasterTileRole::Wall;
 }
 // Template evidence is never borrowed from the other BG.
 std::swap_ranges(maps->begin(),maps->begin()+8192,maps->begin()+8192);
 S9xRemasterDungeonPlacements(c,profile,*tiles);
 assert((*tiles)[20*64+11].surface!=S::Wall);
 // Split lower-door operation: exact two BG1 columns + BG2 foot, with six
 // rows of preserved high perimeter. A partial/mis-layered door is not proof.
 maps->fill(0);templates->fill(0xff);c.constructedMaps=maps->data();
 profile.assets[ids[8]].tileRole=RemasterTileRole::Unknown;
 for(int n=0;n<8;n++) {
  (*templates)[0x2d8+n*2]=n;(*templates)[0x2d9+n*2]=0;
 }
 for(int n=0;n<12;n++) {
  const int word=(n%8)|0x2000;
  (*templates)[0x2e82+n*2]=word;(*templates)[0x2e83+n*2]=word>>8;
  int i=(20+n%4)*64+10+n/4,bg=n/4==2?0:8192;
  (*maps)[bg+i*2]=word;(*maps)[bg+i*2+1]=word>>8;
 }
 for(int yy=-1;yy<=4;yy++)for(int xx=1;xx<=3;xx++)put((20+yy)*64+12+xx,((yy+2)%2)*4+xx|0x2000);
 S9xRemasterDungeonPlacements(c,profile,*tiles);
 assert((*tiles)[21*64+14].highPerimeterFace);
 assert((*tiles)[21*64+12].surface==S::Obstacle && (*tiles)[21*64+12].lowerDoorWallPart); // cover, never ramp
 (*maps)[8192+(21*64+10)*2]^=1;
 S9xRemasterDungeonPlacements(c,profile,*tiles);
 assert(!(*tiles)[21*64+14].highPerimeterFace);
 (*maps)[8192+(21*64+10)*2]^=1;
 put(19*64+14,999);
 S9xRemasterDungeonPlacements(c,profile,*tiles);
 assert(!(*tiles)[21*64+14].highPerimeterFace);
}
