// ROM-template placement recognition and seam solve without changing profiles.
#include "remaster.h"
#include <cassert>
#include <iostream>
int main(){
 using Surface=RemasterDungeonFloorContext::Surface;
 std::array<uint8_t,16384> maps={};std::array<uint8_t,65536> graphics={};
 std::array<uint8_t,8192> attr;attr.fill(1);
 std::array<uint8_t,12876> templates={};
 RemasterProfile profile;std::array<RemasterTileContentId,8> ids;
 for(int n=0;n<8;n++){
  uint8_t pixels[64];for(int k=0;k<64;k++)pixels[k]=(k+n)%15+1;
  for(int y=0;y<8;y++)for(int x=0;x<8;x++)for(int b=0;b<4;b++)
   graphics[n*32+(b/2)*16+y*2+b%2]|=((pixels[y*8+x]>>b)&1)<<(7-x);
  ids[n]={S9xRemasterHashTile(4,pixels),1,4};
  auto &a=profile.assets[ids[n]];a.hasHeight=true;a.hasNormals=true;
  a.normalXyz.fill(128);for(int k=0;k<64;k++)a.height[k]=n==1 || n==5?39-k/8:n==0 || n==4?47-k/8:7-k/8;
  // Flat authored normal arrays must not be replaced by geometric facings.
 }
 profile.assetGroups["wall_faces"].tileIds={ids[0],ids[2],ids[4],ids[6]};
 for(int n=0;n<8;n++){templates[0x2e8+n*2]=n;templates[0x2e8+n*2+1]=0;}
 for(int x=0;x<2;x++)for(int y=0;y<3;y++)maps[((10+y)*64+10+x)*2]=x*4+y;
 // Unsupported room art around the fixture is explicitly unknown.
 for(int i=0;i<4096;i++)if(i/64<10 || i/64>12 || i%64<10 || i%64>11)maps[i*2]=7;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
 c.constructedMaps=maps.data();c.graphics=graphics.data();c.collisionAttributes=attr.data();
 std::array<RemasterDungeonFloorContext::Placement,4096> tiles;
 S9xRemasterDungeonPlacements(c,profile,tiles);assert(tiles[11*64+10].surface==Surface::Unknown);
 c.dungeonTemplateData=templates.data();S9xRemasterDungeonPlacements(c,profile,tiles);
 assert(tiles[11*64+10].surface==Surface::Wall && tiles[11*64+10].wallTemplateOffset==0x2e8);
 assert(tiles[11*64+10].height==profile.assets[ids[1]].height);
 // An overwritten face row cannot be rescued by equal relief or normals.
 maps[(12*64+10)*2]=7;S9xRemasterDungeonPlacements(c,profile,tiles);
 assert(tiles[11*64+10].surface==Surface::Unknown);maps[(12*64+10)*2]=2;
 // An explicit obstacle role vetoes the whole claimed face assembly.
 profile.assetGroups["obstacles"].tileIds={ids[1]};S9xRemasterDungeonPlacements(c,profile,tiles);
 assert(tiles[11*64+10].surface==Surface::Obstacle);assert(tiles[11*64+11].surface==Surface::Wall); // independent intact column
 profile.assetGroups.erase("obstacles");S9xRemasterDungeonPlacements(c,profile,tiles);
 for(int y=13;y<16;y++)for(int x=10;x<14;x++){
  attr[y*64+x]=0;tiles[y*64+x]={};tiles[y*64+x].hasHeight=true;tiles[y*64+x].surface=Surface::Floor;
 }
 c.placements=tiles.data();c.linkX=80;c.linkY=13*8;
 RemasterDungeonHeightMap map;assert(S9xRemasterBuildDungeonHeightMap(c,50,map));
 // Foot base0; middle absolute32..39 needs base-24; top40..47 same
 // base-24. That joins actual samples7 ->8 ->15 ->16, not flat offsets.
 assert(map.offsets[12*64+10]==0);assert(map.offsets[11*64+10]==-24);assert(map.offsets[10*64+10]==-24);
 assert(map.offsets[11*64+10]+tiles[11*64+10].height[63]==8);
 // Handler2f rail wall: full start/middle/end plus repeated trim, not a
 // single nearby relief tile. Explicit floor below is the only support.
 const int railWords[]={7,0,2,4,6,3};
 for(int n=0;n<6;n++){templates[0x688+n*2]=railWords[n];templates[0x689+n*2]=0;}
 profile.assetGroups["wall_faces"].tileIds={ids[0],ids[2],ids[4],ids[6],ids[3]};
 for(int k=0;k<14;k++){
  int n=k<2?k+1:k>=12?k-8:3;maps[(20*64+20+k)*2]=railWords[n];maps[(21*64+20+k)*2]=7;
 }
 S9xRemasterDungeonPlacements(c,profile,tiles);
 assert(tiles[21*64+25].supportingTrim && tiles[21*64+25].surface==Surface::Obstacle);
  maps[(20*64+32)*2]=1;S9xRemasterDungeonPlacements(c,profile,tiles);
  assert(!tiles[21*64+25].supportingTrim); // overwritten end vetoes full rail
  for(int n=0;n<6;n++){templates[0x694+n*2]=railWords[n];templates[0x695+n*2]=0;}
  for(int k=0;k<14;k++){
   int n=k<2?k+1:k>=12?k-8:3;maps[(41*64+20+k)*2]=railWords[n];maps[(40*64+20+k)*2]=7;
  }
  S9xRemasterDungeonPlacements(c,profile,tiles);
  assert(tiles[40*64+25].supportingTrim && tiles[40*64+25].wallTemplateOffset==0x694);
   maps[(41*64+32)*2]=1;S9xRemasterDungeonPlacements(c,profile,tiles);
   assert(!tiles[40*64+25].supportingTrim);
   // Early rejection must neither accept a prefix at another candidate width
   // nor skip a later valid width. Exercise both dispatch orientations and
   // every supported length; reject first/middle/last face and trim overwrites.
   const auto savedMaps=maps;
   for(int kind=0;kind<2;kind++)for(int width=14;width<=29;width++) {
    maps=savedMaps;
    for(int y=20;y<=21;y++)for(int x=20;x<64;x++)maps[(y*64+x)*2]=7;
    const int faceY=20+kind,trimY=21-kind,off=kind?0x694:0x688;
    for(int k=0;k<width;k++) {
     const int n=k<2?k+1:k>=width-2?k-width+6:3;
     maps[(faceY*64+20+k)*2]=railWords[n];
    }
    const auto intact=maps;
    S9xRemasterDungeonPlacements(c,profile,tiles);
    for(int k=0;k<width;k++)assert(tiles[trimY*64+20+k].supportingTrim &&
     tiles[trimY*64+20+k].wallTemplateOffset==off);
    for(int row:{faceY,trimY})for(int k:{0,width/2,width-1}) {
     maps=intact;maps[(row*64+20+k)*2]=1;
     S9xRemasterDungeonPlacements(c,profile,tiles);
     for(int j=0;j<width;j++)assert(!tiles[trimY*64+20+j].supportingTrim);
    }
   }
   maps=savedMaps;
  // Outer-corner bounded strip: exact three ordered words and two wall seeds.
  for(int n=0;n<16;n++){templates[0xbc6+n*2]=n%8;templates[0xbc7+n*2]=0;}
  for(int k=0;k<3;k++)maps[((30+k)*64+30)*2]=k;
  S9xRemasterDungeonPlacements(c,profile,tiles);
  assert(tiles[31*64+30].surface==Surface::Wall && tiles[31*64+30].wallTemplateOffset==0xbc6);
  assert(tiles[31*64+30].height==profile.assets[ids[1]].height);
  maps[(32*64+30)*2]=7;S9xRemasterDungeonPlacements(c,profile,tiles);
  assert(tiles[31*64+30].surface==Surface::Unknown);
  maps[(32*64+30)*2]=2;profile.assets[ids[1]].tileRole=RemasterTileRole::Obstacle;
  S9xRemasterDungeonPlacements(c,profile,tiles);
  assert(tiles[31*64+30].surface==Surface::Obstacle);
	// Observer-only intact assembly without ANY solver wall seed or authored
	// arrays. Classification must not promote roles or manufacture local heights.
	profile.assetGroups.clear();profile.assets.clear();
	for(int xx=0;xx<2;xx++)for(int yy=0;yy<4;yy++)maps[((50+yy)*64+10+xx)*2]=xx*4+yy;
	S9xRemasterDungeonPlacements(c,profile,tiles);
	for(int xx=0;xx<2;xx++)for(int yy=0;yy<4;yy++) {
		const auto &t=tiles[(50+yy)*64+10+xx];
		assert(t.surface==Surface::Unknown && !t.hasHeight && !t.wallTemplateOffset);
		assert(t.diagnosticWallTemplateOffset==0x2e8);
	}
	// Profile normal is opposite the height axis, never itself a wall role.
	auto &normalOnly=profile.assets[ids[1]];normalOnly.hasNormals=true;
	for(int k=0;k<64;k++){normalOnly.normalXyz[k*3]=220;normalOnly.normalXyz[k*3+1]=128;normalOnly.normalXyz[k*3+2]=128;}
	S9xRemasterDungeonPlacements(c,profile,tiles);
	assert(tiles[51*64+10].diagnosticNormalUphill==4);
	assert(tiles[51*64+10].surface==Surface::Unknown && !tiles[51*64+10].hasHeight);
	normalOnly.normalXyz[0]=36;S9xRemasterDungeonPlacements(c,profile,tiles);
	assert(!tiles[51*64+10].diagnosticNormalUphill); // roughness is not a unique axis
	maps[(51*64+10)*2]=7;S9xRemasterDungeonPlacements(c,profile,tiles);
	assert(!tiles[51*64+10].diagnosticWallTemplateOffset);
	// Authored floor trim can have slight relief. Keep its diagnostic support
	// identity even though the solver intentionally defers the nonflat role.
	profile.assets[ids[1]].tileRole=RemasterTileRole::Floor;
	profile.assets[ids[1]].hasHeight=true;profile.assets[ids[1]].height[0]=1;
	maps[(51*64+10)*2]=1;S9xRemasterDungeonPlacements(c,profile,tiles);
	assert(tiles[51*64+10].surface==Surface::Unknown && tiles[51*64+10].diagnosticAuthoredFloor);
	assert(!tiles[51*64+10].diagnosticWallTemplateOffset);
  std::cout<<"PASS exact straight/corner template middle, overwritten/obstacle rejection, authored arrays, signed variable seams\n";
}
