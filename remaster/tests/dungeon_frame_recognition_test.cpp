// Fixture-free complete ROM-handler frame ownership; opening art is independent.
#include "../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
static void need(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try {
 using C=RemasterDungeonFloorContext;
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();
 auto templates=std::make_unique<std::array<uint8_t,12876>>();
 auto tiles=std::make_unique<std::array<C::Placement,4096>>();
 RemasterProfile profile;
 // Distinct graphics identities, independent of tile word/placement flips.
 for(int tile=0;tile<64;tile++){
  uint8_t p[64];for(int k=0;k<64;k++)p[k]=1;
  p[0]=1+(tile&7);p[1]=1+(tile>>3);p[2]=2;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   (*graphics)[tile*32+b/2*16+y*2+b%2]|=((p[y*8+x]>>b)&1)<<(7-x);
  RemasterTileContentId id{S9xRemasterHashTile(4,p),1,4};auto &a=profile.assets[id];
  a.tileId=id;a.tileRole=RemasterTileRole::Doorway;a.hasHeight=a.hasDoorframeMask=true;
  a.doorframeMask=UINT64_MAX;for(int k=0;k<64;k++)a.height[k]=7-k/8;
 }
 C c;c.indoors=c.verifiedAlttpRom=true;c.graphics=graphics->data();
 c.constructedMaps=maps->data();c.dungeonTemplateData=templates->data();c.bg1GraphicsBase=0;
 const int ordinary[]={0x2716,0x29f0,0x2c6a,0x2e6a};int cases=0;
 for(int family=0;family<2;family++)for(int d=0;d<(family?2:4);d++)
 for(int variant=0;variant<(family?2:1);variant++)for(int split=0;split<(family?1:2);split++)
 for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++)for(int opening=0;opening<3;opening++){
  templates->fill(0xff);maps->fill(0xfe);c.placementBackgroundIndex=bg;
  const int off=family?0x11a8+d*64+variant*32:ordinary[d];
  const int w=d<2?4:3,h=family?4:d<2?3:4;
  auto put=[&](int address,int word){(*maps)[address]=word;(*maps)[address+1]=word>>8;};
  for(int x=0;x<w;x++)for(int y=0;y<h;y++){
   const int n=off+2*(x*h+y),word=1+x*h+y;
   (*templates)[n]=word;(*templates)[n+1]=0;
   const bool frame=family?(d==0?y<3 && (x==0 || x==3 || y==0):y>0 && (x==0 || x==3 || y==3)):
    (d<2?(x==0 || x==w-1 || (d==0?y==0:y==h-1)):(y==0 || y==h-1 || (d==2?x==0:x==w-1)));
   const int px=flip&1?w-1-x:x,py=flip&2?h-1-y:y;
   const int owner=split?((d==0?y==0:d==1?y==h-1:d==2?x==0:x==w-1)?1:0):bg;
   const int actual=frame?word:opening==0?30:opening==1?31:32;
   put((owner==bg?0:8192)+2*((20+py)*64+20+px),actual^((flip&1?0x4000:0)|(flip&2?0x8000:0)));
  }
  const auto original=*maps;
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  const int startY=family?((d==0)==bool(flip&2)?1:0):0;
  const int origin=(20+startY)*64+20;
  const int direction=d<2?d^((flip&2)?1:0):d^((flip&1)?1:0);
  int expected=0,seen=0;
  for(int x=0;x<w;x++)for(int y=0;y<h;y++){
   const bool frame=family?(d==0?y<3 && (x==0 || x==3 || y==0):y>0 && (x==0 || x==3 || y==3)):
    (d<2?(x==0 || x==w-1 || (d==0?y==0:y==h-1)):(y==0 || y==h-1 || (d==2?x==0:x==w-1)));
   const int owner=split?((d==0?y==0:d==1?y==h-1:d==2?x==0:x==w-1)?1:0):bg;
   const int px=flip&1?w-1-x:x,py=flip&2?h-1-y:y;
   const auto &t=(*tiles)[(20+py)*64+20+px];
   if(frame && owner==bg){expected++;if(!(t.doorAssembly==origin+1 && t.doorDirection==direction && t.splitDoorPart==bool(split)))std::cerr<<"case "<<family<<','<<d<<','<<variant<<','<<split<<','<<flip<<','<<bg<<','<<opening<<" part "<<x<<','<<y<<" got "<<t.doorAssembly<<','<<int(t.doorDirection)<<','<<t.splitDoorPart<<" expected "<<origin+1<<','<<direction<<'\n';need(t.doorAssembly==origin+1 && t.doorDirection==direction && t.splitDoorPart==bool(split),"frame axis/part/BG/flip ownership");}
   else need(!t.doorTemplateOffset,"opening/opposite BG became structural frame");
  }
  for(const auto &t:*tiles)seen+=t.doorTemplateOffset!=0;
  need(seen==expected && expected>0,"complete frame denominator");
  // Missing one outer frame word on EITHER BG breaks the whole proof.
  const int cx=0,cy=family && d==1?1:0;
  const int px=flip&1?w-1-cx:cx,py=flip&2?h-1-cy:cy;
  const int owner=split?((d==0?cy==0:d==1?cy==h-1:d==2?cx==0:cx==w-1)?1:0):bg;
  put((owner==bg?0:8192)+2*((20+py)*64+20+px),0xffff);
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  for(const auto &t:*tiles)need(!t.doorTemplateOffset,"overwritten structural part accepted");
  *maps=original;cases++;
 }
 // Repeated frame words can prove two different origins. They are NOT
 // equivalent merely because their overlapping artwork matches.
 templates->fill(0xff);maps->fill(0xfe);c.placementBackgroundIndex=1;
 for(int x=0;x<4;x++)for(int y=0;y<3;y++){
  (*templates)[0x2716+2*(x*3+y)]=1+y;(*templates)[0x2717+2*(x*3+y)]=0;
 }
 for(int x=0;x<5;x++)for(int y=0;y<3;y++){
  const int i=(20+y)*64+20+x;(*maps)[2*i]=1+y;(*maps)[2*i+1]=0;
 }
 S9xRemasterDungeonPlacements(c,profile,*tiles);int ambiguous=0;
 for(const auto &t:*tiles)if(t.doorTemplateOffset){need(t.doorDirection==0xfe,"competing origins accepted");ambiguous++;}
  need(ambiguous>0,"ambiguous candidate denominator");
  // The actual western failure was attachment, not recognition. The previous
  // 288 synthetic frames used full ramps everywhere, so never exercised caps.
  // Prove reusable canonical sections on both axes; flips affect displayed
  // contacts/masks, not this intrinsic source proof. Signed datums are legal.
  for(int d=0;d<4;d++)for(int flip=0;flip<4;flip++)for(int cap=1;cap<7;cap++)for(int datum:{-32,0,300}){
   C::Placement wall;wall.surface=C::Surface::Wall;wall.hasHeight=true;
   wall.wallTemplateOffset=0xb86;wall.highCornerFace=true;wall.hflip=flip&1;wall.vflip=flip&2;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)wall.height[y*8+x]=datum+std::min(cap,7-(d<2?y:x));
   need(S9xRemasterDungeonDoorTangentCap(wall,d),"clipped cap axis/flip/signed proof");
   for(int k=0;k<64;k++){wall.height[k]++;need(!S9xRemasterDungeonDoorTangentCap(wall,d),"partial/overwritten cap proof");wall.height[k]--;}
   wall.wallTemplateOffset=0;need(!S9xRemasterDungeonDoorTangentCap(wall,d),"missing ownership cap proof");wall.wallTemplateOffset=0xb86;
   wall.highCornerFace=false;need(!S9xRemasterDungeonDoorTangentCap(wall,d),"unproved cap handler");wall.highCornerFace=true;
   wall.surface=C::Surface::Floor;need(!S9xRemasterDungeonDoorTangentCap(wall,d),"incompatible cap role");
  }
   // Full support/attachment solve, independent planes, all axes and flips.
   // A wall seam/cap alone is NOT doorway support. Explicit floorward marks
   // on the two innermost frame members seed the shared assembly datum.
  auto attrs=std::make_unique<std::array<uint8_t,8192>>();
  auto result=std::make_unique<RemasterDungeonHeightMap>();
  C solveContext;solveContext.indoors=solveContext.verifiedAlttpRom=true;
  solveContext.placements=tiles->data();solveContext.collisionAttributes=attrs->data();
  for(int axis=0;axis<2;axis++)for(int flip=0;flip<4;flip++)for(int bg=0;bg<2;bg++){
   *tiles={};attrs->fill(1);solveContext.placementBackgroundIndex=bg;
   auto cell=[&](int u,int v){int x=axis?v:u,y=axis?u:v;return (flip&2?63-y:y)*64+(flip&1?63-x:x);};
   solveContext.linkX=(cell(18,17)%64)*8;solveContext.linkY=(cell(18,17)/64)*8;
   for(int u=13;u<24;u++)for(int v=12;v<22;v++){int i=cell(u,v);auto &t=(*tiles)[i];t.surface=C::Surface::Floor;t.hasHeight=true;(*attrs)[i]=0;}
   for(int u=10;u<13;u++)for(int v=12;v<22;v++){
    int i=cell(u,v);auto &t=(*tiles)[i];t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
    const bool door=v>=15 && v<=18 && (u==10 || v==15 || v==18);
    t.surface=door?C::Surface::Door:C::Surface::Wall;
    for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[y*8+x]=door || u!=10?7-(axis?y:x):std::min(5,7-(axis?y:x));
    if(door){t.hasDoorframeMask=true;t.doorframeMask=UINT64_MAX;t.doorTemplateOffset=0x2c6a;
     t.doorDirection=axis?((flip&2)?1:0):((flip&1)?3:2);
     // assembly origin is the displayed minimum coordinate (not canonical).
     t.doorAssembly=std::min({cell(10,15),cell(10,18),cell(12,15),cell(12,18)})+1;
    }else {t.wallTemplateOffset=axis?0x2e8:0x2c8;t.highPerimeterFace=true;}
   }
   // Independent opening has arbitrary relief, no structural frame ownership.
   for(int u=11;u<13;u++)for(int v=16;v<18;v++){auto &t=(*tiles)[cell(u,v)];t={};t.hasHeight=true;t.height.fill(-31);}
    need(S9xRemasterBuildDungeonHeightMap(solveContext,50,*result),"unmarked cap solve");
    need(result->doorframeAttachments.empty(),"unmarked wall/cap supplied doorway support");
    for(int v:{15,18}){
     auto &t=(*tiles)[cell(12,v)];t.doorwaySupport=true;
     t.doorwaySupportDirection=axis?RemasterSupportDirection::South:RemasterSupportDirection::East;
    }
    need(S9xRemasterBuildDungeonHeightMap(solveContext,50,*result),"marked directional doorway solve");
    auto checkAttachments=[&](){
     int count=0;
     for(const auto &a:result->doorframeAttachments){
      need(a.visible,"marked support attachment lost");
      need(a.plane==0,"unsupported solid plane supplied doorway datum");
       const int band=2-(axis?((flip&2)?63-a.cell/64:a.cell/64):((flip&1)?63-a.cell%64:a.cell%64))+10;
       need(a.base==8*band && a.openingBase==0,"frame band rise must compose with independent landing datum");
      count++;
     }
     need(count==8,"full marked frame denominator / separate support planes");
    };
    checkAttachments();
    const auto authored=(*tiles)[cell(10,15)].height;
    need(authored[0]==7,"fixture authored frame ramp changed");
    const auto old=(*tiles)[cell(10,14)].height;(*tiles)[cell(10,14)].height[0]++;
    need(S9xRemasterBuildDungeonHeightMap(solveContext,50,*result),"independent tangent cap solve");
    checkAttachments(); // wall cap geometry cannot override explicit floorward support
    need((*tiles)[cell(10,15)].height==authored,"doorway authored ramp flattened");
    (*tiles)[cell(10,14)].height=old;
    for(int v:{15,18}){
     auto &t=(*tiles)[cell(12,v)];
     t.doorwaySupportDirection=axis?RemasterSupportDirection::North:RemasterSupportDirection::West;
    }
    need(S9xRemasterBuildDungeonHeightMap(solveContext,50,*result),"wrong-direction doorway solve");
    need(result->doorframeAttachments.empty(),"opposite-facing mark borrowed unrelated floor/wall support");
  }
 std::cout<<"PASS "<<cases<<" fixture-free ordinary four axes/stair north-south variants, all flips, split-BG, opening independence, whole-frame overwrite and competing-origin rejection\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
