#include "../runtime/overworld_actor_support.h"
#include "../runtime/placement_offset_editor.h"
#include <iostream>
#include <cstdlib>
static void check(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(int argc,char**argv){
 RemasterFrame f;RemasterDungeonFloorContext c;c.verifiedAlttpRom=true;c.linkX=16;c.linkY=0;
 f.width=f.height=64;f.area.id=320;f.observedBgPixels[1].resize(4096);f.tileInstances.resize(3);
 auto &ground=f.tileInstances[0];ground.source=RemasterSourceType::Background;ground.sourceIndex=1;ground.savedPlacementOffsetStatus=2;ground.heightOffset=40;ground.tileId.hash=1;
 ground.terrainOffset.hasHeight=true;for(unsigned k=0;k<64;k++)ground.terrainOffset.height[k]=float(k%8)*.125f;
 RemasterFrameAssetMetadata m;m.tileId=ground.tileId;m.tileRole=RemasterTileRole::Floor;f.assetMetadata.push_back(m);
 RemasterContextLocation at;at.area=320;at.background=1;at.x=3;at.y=2;f.contextLocations[1]=at;
 for(int j=1;j<3;j++){auto &i=f.tileInstances[j];i.source=RemasterSourceType::Object;i.sourceIndex=55+j;i.vramAddress=j==1?0x8000:0x9000;i.heightOffset=2;}
 auto place=[&](unsigned dx){c.linkX=16+dx;auto &px=f.observedBgPixels[1][20*64+24+dx];px.instanceId=1;px.tilePixel=32+dx;};
 for(unsigned x=0;x<8;x++){place(x);float h;check(S9xRemasterOverworldLinkSupport(f,c,h)&&h==40+x*.125f,"per-pixel support tracks movement");}
 place(3);ground.hFlip=true;f.observedBgPixels[1][20*64+27].tilePixel=36;
 float mirrored;check(S9xRemasterOverworldLinkSupport(f,c,mirrored)&&mirrored==40.375f,"world pixel offsets do not mirror with artwork");ground.hFlip=false;place(3);
 ground.heightOffset=-32;float negative;check(S9xRemasterOverworldLinkSupport(f,c,negative)&&negative==-31.625f,"signed river support");ground.heightOffset=40;
 place(3);check(S9xRemasterApplyOverworldLinkSupport(f,c),"apply support");
 for(unsigned j=1;j<3;j++)check(f.tileInstances[j].heightOffset==42&&f.tileInstances[j].terrainOffset.height[0]==.375f,"shared fractional support preserves existing sprite base");
 c.indoors=true;float h;check(!S9xRemasterOverworldLinkSupport(f,c,h),"indoors isolated");c.indoors=false;
 c.verifiedAlttpRom=false;check(!S9xRemasterOverworldLinkSupport(f,c,h),"unverified ROM isolated");c.verifiedAlttpRom=true;
 f.assetMetadata[0].tileRole=RemasterTileRole::Wall;check(!S9xRemasterOverworldLinkSupport(f,c,h),"wall cannot ground Link");
	// Even completely OBJ-covered floor tiles must publish coordinate evidence.
	f.assetMetadata[0].tileRole=RemasterTileRole::Floor;f.contextLocations.clear();
	std::array<int,4> sx{},sy{};S9xRemasterLocateContextTiles(f,sx,sy);
	check(f.contextLocations.count(1)&&f.contextLocations.at(1).x==3&&f.contextLocations.at(1).y==2,"independent BG observations locate OBJ-hidden floor");
	f.subPixels.resize(4096);f.subPixels[0].instanceId=1;f.subPixels[0].tilePixel=0;
	S9xRemasterLocateContextTiles(f,sx,sy);check(!f.contextLocations.count(1),"conflicting composite/independent coordinates still reject");
	f.subPixels.clear();S9xRemasterLocateContextTiles(f,sx,sy);
 if(argc==3){RemasterFrame cap;RemasterProfile p;std::vector<RemasterProfileDiagnostic>d;check(S9xReadRemasterFrame(argv[1],cap)&&S9xRemasterLoadProfile(argv[2],p,d),"capture/profile load");S9xRemasterApplyProfileToFrame(p,cap);S9xRemasterPreviewPlacementOffsets(p,cap);
  RemasterDungeonFloorContext live;live.verifiedAlttpRom=true;live.linkX=294;live.linkY=1663;live.backgroundScrollX=168;live.backgroundScrollY=1569;
  float base;check(S9xRemasterApplyOverworldLinkSupport(cap,live,&base),"reported capture support reproduced");std::cout<<"Capture support="<<base<<'\n';}
	if(argc==8){RemasterFrame cap;RemasterProfile p;std::vector<RemasterProfileDiagnostic>d;
	 check(S9xReadRemasterFrame(argv[1],cap)&&S9xRemasterLoadProfile(argv[2],p,d),"occluded capture/profile load");
	 RemasterDungeonFloorContext live;live.verifiedAlttpRom=true;live.linkX=std::atoi(argv[3]);live.linkY=std::atoi(argv[4]);live.backgroundScrollX=std::atoi(argv[5]);live.backgroundScrollY=std::atoi(argv[6]);
	 float base=0;check(!S9xRemasterOverworldLinkSupport(cap,live,base),"original capture reproduces support reset");
	 std::array<int,4> xs{},ys{};xs[1]=live.backgroundScrollX;ys[1]=live.backgroundScrollY;
	 // Captured raster observations attest the effective PPU scroll (which can
	 // differ by a scanline from the WRAM copy register). Infer only from an
	 // existing, verified same-world coordinate; never a filename or guessed base.
	 for(size_t n=0;n<cap.observedBgPixels[1].size();++n){const auto &px=cap.observedBgPixels[1][n];auto at=cap.contextLocations.find(px.instanceId);if(at==cap.contextLocations.end() || !px.instanceId || px.tilePixel>=64)continue;const auto &i=cap.tileInstances[px.instanceId-1];if(at->second.background!=1 || !i.savedPlacementOffsetStatus)continue;const auto pixel=S9xRemasterTerrainPixel(px.tilePixel,i.hFlip,i.vFlip);xs[1]=at->second.x*8+pixel%8-int(n%cap.width);ys[1]=at->second.y*8+pixel/8-int(n/cap.width);break;}
	 S9xRemasterLocateContextTiles(cap,xs,ys);
	 // Replay only: re-attest newly located observations from the supplied
	 // sidecar camera. Live EndFrame assigns status directly after location.
	 for(const auto &e:cap.contextLocations)if(e.second.background==1)cap.tileInstances[e.first-1].savedPlacementCoordinateVerified=true;
	 S9xRemasterApplyProfileToFrame(p,cap);S9xRemasterPreviewPlacementOffsets(p,cap);
	 check(S9xRemasterApplyOverworldLinkSupport(cap,live,&base),"occluded floor now supports Link");
	 check(std::abs(base-std::atof(argv[7]))<.001,"capture expected exact floor height");std::cout<<"Occluded capture repaired support="<<base<<'\n';}
 std::cout<<"PASS overworld Link per-pixel support, shared body/equipment base, fractional relief and isolation\n";
}
