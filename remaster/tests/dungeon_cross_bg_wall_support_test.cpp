// Transport authored support relationships through a paired complementary foot.
#include "../runtime/remaster.h"
#include <cassert>
int main(){
 using S=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 auto words=std::make_unique<std::array<uint8_t,16384>>();
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.placements=tiles->data();c.collisionAttributes=attrs->data();c.constructedMaps=words->data();
 c.linkPlane=1;c.linkX=80;c.linkY=160;c.graphicsBase=c.bg1GraphicsBase=0;
 c.stairCount=1;c.stairs[0].x=40;c.stairs[0].y=40;c.stairs[0].changesPlane=true;
 for(int y=17;y<22;y++)for(int x=6;x<10;x++) {
  auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;
  (*attrs)[4096+y*64+x]=0;
 }
 const int foot=18*64+10,face=foot+1;
 auto &f=(*tiles)[foot];f.surface=S::Wall;f.hasHeight=true;f.wallTemplateOffset=0x318;
 for(int k=0;k<64;k++)f.height[k]=k%8;
 (*words)[foot*2]=(*words)[8192+foot*2]=9;
 c.placementBackgroundIndex=0;
 auto lower=std::make_unique<RemasterDungeonHeightMap>();
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*lower));
 assert(!lower->authoredWallSupportRelations.empty());
 // Receiver: high wall contacts the same physical foot, while the support
 // belongs to another BG. No configured stair or visible floor-base copy.
 auto &h=(*tiles)[face];h.surface=S::Wall;h.hasHeight=true;h.wallTemplateOffset=0x2d8;h.highPerimeterFace=true;
 for(int k=0;k<64;k++)h.height[k]=8+k%8;
 for(int y=17;y<22;y++)for(int x=6;x<10;x++)(*tiles)[y*64+x].surface=S::Unknown;
 c.placementBackgroundIndex=1;c.wallSupportRelations=&lower->authoredWallSupportRelations;
 // Explicit relation is signed evidence; authored local samples stay pinned.
 auto m=std::make_unique<RemasterDungeonHeightMap>();
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]!=255);
 const auto accepted=m->structuralPlaneOffsets[0][foot];
 assert(accepted==0 && m->offsets[face]==0);
 // Split-door terminal cover keeps local relief and cannot bridge support nodes.
 const int cover=face-64;auto &door=(*tiles)[cover];door.surface=S::Obstacle;
 door.hasHeight=true;door.lowerDoorWallPart=door.supportingTrim=true;
 auto savedFoot=f;
 // The cover's east neighbor is a second high face attached tangentially.
 auto &next=(*tiles)[cover+1];next=h;
 next.height.fill(8); // edge does not agree with the original varying face
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->offsets[cover]==255); // incompatible/missing structural seam, no guessed cover
 door={};next={};f=savedFoot;
 f.height[0]++;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 f.height[0]--;
 for(auto &v:h.height)v=7-v%8;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 for(int k=0;k<64;k++)h.height[k]=8+k%8;
 (*words)[8192+foot*2]=8;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 (*words)[8192+foot*2]=9;c.bg1GraphicsBase=32;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 c.bg1GraphicsBase=0;h.wallTemplateOffset=0;h.highPerimeterFace=false;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 h.wallTemplateOffset=0x2d8;h.highPerimeterFace=true;(*attrs)[foot]=0;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->structuralPlaneOffsets[0][foot]==255);
 (void)accepted;
}
