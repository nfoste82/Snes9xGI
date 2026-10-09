// Immutable complete input; composed internal cap must not stop a seven-strip face.
#include "remaster.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);check(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
 check(argc==2,"usage: exact032 capture");const std::string path=argv[1];
 auto input=read(path+".dungeon-input.bin"),tmpl=read(path+".dungeon-templates.bin");
 RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
 check(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"profile");
 std::string before,after;check(S9xRemasterSerializeProfile(profile,before,errors),"serialize");
 RemasterDungeonFloorContext c;c.indoors=c.verifiedAlttpRom=true;c.roomIndex=0x61;
 c.graphicsBase=c.bg1GraphicsBase=16384;c.graphics=input.data()+24584;
 c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.dungeonTemplateData=tmpl.data()+8;
 c.linkX=649;c.linkY=3347;S9xRemasterDungeonStairsFromAttributes(c);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto solve=[&](){S9xRemasterDungeonPlacements(c,profile,*tiles);c.placements=tiles->data();
  auto m=std::make_unique<RemasterDungeonHeightMap>();check(S9xRemasterBuildDungeonHeightMap(c,50,*m),"solve");return m;};
 auto map=solve();check(map->structuralConflicts==0 && map->unknownWallCells==0,"no new room conflicts");
 for(int x:{8,9,10,11,52,53,54,55})for(int row=0;row<7;row++) {
  const int y=(x<32?x+30:93-x)+row;const auto &t=(*tiles)[y*64+x];
  check(t.diagonalHeightPreview,"seven owned rows");
  for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)
   check(t.height[yy*8+xx]==std::max(0,std::min(48,7+8*row-xx-yy)),"continued ramp");
  check(map->offsets[y*64+x]!=255,"signed support base");
 }
 check((*tiles)[5*64+11].height[0]==47,"approved upper preserved");
 for(int x=8;x<=11;x++)for(int r=0;r<7;r++)input[9+2*((x+30+r)*64+x)]^=0x20;
 auto priority=solve();
 for(int x=8;x<=11;x++)for(int r=0;r<7;r++) {
  const int i=(x+30+r)*64+x;check((*tiles)[i].diagonalHeightPreview,"priority-only ownership");
 }
 for(int x=8;x<=11;x++)for(int r=0;r<7;r++)input[9+2*((x+30+r)*64+x)]^=0x20;
 const auto saved=input;
 // A replacement of an internal member cannot inherit the old assembly.
 input[8+2*(41*64+8)]=0xff;input[9+2*(41*64+8)]=3;
 solve();check(!(*tiles)[41*64+8].diagonalHeightPreview,"overwrite rejection");input=saved;
 // Different assembly extents and independently authored signed rises. No
 // room/hash selector: two complete reverse columns are constructed from ROM.
 const auto oldProfile=profile;
 auto graphics=std::vector<uint8_t>(c.graphics,c.graphics+65536);const auto oldTemplates=tmpl;
 c.graphics=graphics.data();
 for(int shift=1;shift<=5;shift++) {
  profile=oldProfile;
  for(int axis=0;axis<2;axis++)for(int band=0;band<3;band++) {
   const int off=(axis?0x2c8:0x2e8)+band*2,w=900+axis*3+band;
   tmpl[8+off]=w&255;tmpl[9+off]=w>>8;
   std::fill_n(graphics.begin()+16384+w*32,32,0);
   graphics[16384+w*32]=1+axis*3+band;
   uint8_t decoded[64];for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
    int v=0;for(int b=0;b<4;b++)v|=((c.graphics[uint16_t(16384+(w&1023)*32+(b/2)*16+y*2+b%2)]>>(7-x))&1)<<b;
    decoded[y*8+x]=v;
   }
   auto &asset=profile.assets[{S9xRemasterHashTile(4,decoded),1,4}];asset.hasHeight=true;
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)asset.height[y*8+x]=shift*8+8*(2-band)+7-(axis?x:y);
  }
  std::fill(input.begin()+8,input.begin()+8200,0);
  // Ensure unplaced independent cardinal proof graphics are decoded too.
  for(int axis=0;axis<2;axis++)for(int band=0;band<3;band++) {
   const int i=5*64+5+axis*3+band,w=900+axis*3+band;
   input[8+i*2]=w&255;input[9+i*2]=w>>8;
  }
  const int extent=4+shift;
  for(int col=0;col<2;col++)for(int row=0;row<extent;row++) {
   const int off=row<4?0x4f2+(row+1)*2:0x47a+(row-shift+1)*2;
   const int i=(20+col+row)*64+20+col;
   input[8+i*2]=tmpl[8+off];input[9+i*2]=tmpl[9+off];
  }
  S9xRemasterDungeonPlacements(c,profile,*tiles);
  const auto &t=(*tiles)[(20+3)*64+20];
  if(shift<=4) {
   check(t.diagonalHeightPreview,"variable extent accepted");
   check(t.diagonalMaximum==shift*8+24 && t.diagonalMinimum==0,"cardinal boundary not fixed four");
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)check(t.height[y*8+x]==std::min(shift*8+24,31-x-y),"internal cap continuity");
  }else check(!t.diagonalHeightPreview,"unsupported dispatch gap rejected");
 }
 input=saved;profile=oldProfile;tmpl=oldTemplates;c.graphics=input.data()+24584;
 // Exact immutable profile arrays survive all effective solves.
 check(S9xRemasterSerializeProfile(profile,after,errors) && before==after,"authored immutability");
 RemasterFrameTileInstance instance;instance.hasPlacementHeight=true;
 RemasterFrameAssetMetadata metadata;metadata.hasHeight=metadata.hasNormals=true;
 for(int k=0;k<64;k++){metadata.height[k]=24;metadata.normalXyz[k*3]=metadata.normalXyz[k*3+1]=128;metadata.normalXyz[k*3+2]=255;}
 for(int y=0;y<8;y++)for(int x=0;x<8;x++)instance.placementHeight[y*8+x]=31-x-y;
 check(S9xRemasterContinuedDiagonalNormalMask(instance,&metadata)!=0,"internal cap normal correction");
 instance.placementHeight.fill(48);check(!S9xRemasterContinuedDiagonalNormalMask(instance,&metadata),"true cap retained");
 instance.placementHeight[0]=47;check(!S9xRemasterContinuedDiagonalNormalMask(instance,&metadata),"non affine rejection");
 std::cout<<"PASS seven strips, mirrors, extents5/6/7/8 and rejected9, exact ramp, signed support, upper preservation, overwrite/priority, normals, immutable profile\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
