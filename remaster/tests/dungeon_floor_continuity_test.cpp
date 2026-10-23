#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

int main()
{
	using Surface=RemasterDungeonFloorContext::Surface;
	auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto map=std::make_unique<RemasterDungeonHeightMap>();
	std::vector<uint8_t> attributes(8192,1);
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
	c.placements=tiles->data();c.collisionAttributes=attributes.data();
	c.linkX=20*8;c.linkY=19*8;
	for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++) {
		*tiles={};attributes.assign(8192,1);c.placementBackgroundIndex=bg;
		for(int y:{20,26})for(int yy=y;yy<y+3;yy++)for(int x=20;x<24;x++) {
			auto &t=tiles->at(yy*64+x);t.surface=Surface::Floor;t.hasHeight=true;
			attributes[yy*64+x]=0;
		}
		for(int y=23;y<26;y++)for(int x=20;x<24;x++) {
			auto &t=tiles->at(y*64+x);t.surface=Surface::Wall;t.hasHeight=true;
			for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
		}
		// One-tile-wide floor spur cannot contain Link's complete collision body.
		// Classification defers authored nonflat floors from the plateau graph.
		for(int x=24;x<29;x++) {
			auto &t=tiles->at(20*64+x);t.hasHeight=t.diagnosticAuthoredFloor=true;
			t.hflip=flip&1;t.vflip=flip&2;t.height[3*8+4]=1;
			attributes[20*64+x]=0;
		}
		const auto original=tiles->at(20*64+28).height;
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		const auto required=map->requiredPlaneCells,solved=map->solvedPlaneCells;
		for(int x=24;x<29;x++) {
			const int i=20*64+x;
			assert(map->planeOffsets[0][i]==24 && map->planeOffsets[1][i]==255);
			assert(map->offsets[i]==24 && !map->fallbackDirections[i]);
			assert(map->roomDiagnostics->cells[i].floorBase==24);
		}
		assert(tiles->at(20*64+28).height==original);
		// Unknown art, a wall, solid collision and explicit plane exclusion each
		// break continuity. Never jump the gap or borrow another BG/plane.
		for(int rejection=0;rejection<4;rejection++) {
			auto &gap=tiles->at(20*64+26);const auto saved=gap;
			if(rejection==0)gap.diagnosticAuthoredFloor=false;
			if(rejection==1){gap.diagnosticAuthoredFloor=false;gap.surface=Surface::Wall;}
			if(rejection==2)attributes[20*64+26]=1;
			if(rejection==3)gap.structuralFloorPlaneMask=2;
			assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
			assert(map->planeOffsets[0][20*64+28]==255);
			gap=saved;attributes[20*64+26]=0;
		}
		// Corner-only contact supplies no base.
		auto &diagonal=tiles->at(19*64+29);diagonal=tiles->at(20*64+28);
		attributes[19*64+29]=0;
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->planeOffsets[0][19*64+29]==255);
		assert(map->requiredPlaneCells==required && map->solvedPlaneCells==solved);
		// Actual packet composition retains relief and flips; only the base moves.
		RemasterFrame frame;frame.width=frame.height=8;frame.mainPixels.resize(64);frame.subPixels.resize(64);
		RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=bg;
		instance.hFlip=flip&1;instance.vFlip=flip&2;frame.tileInstances.push_back(instance);
		for(int i=0;i<64;i++){frame.mainPixels[i].instanceId=1;frame.mainPixels[i].tilePixel=i;}
		c.backgroundScrollX=28*8;c.backgroundScrollY=20*8;
		S9xRemasterApplyDungeonHeightMap(frame,c,*map,nullptr,nullptr,bg);
		assert(frame.tileInstances[0].heightOffset==24 && !frame.tileInstances[0].hasPlacementHeight);
		// Extend the spur down to the independently solved lower landing. A
		// conflicting floor region must not erase the proved 24-unit rise.
		for(int y=21;y<=26;y++) {
			auto &t=tiles->at(y*64+28);t=tiles->at(20*64+28);attributes[y*64+28]=0;
		}
		for(int x=24;x<28;x++){tiles->at(26*64+x)=tiles->at(20*64+28);attributes[26*64+x]=0;}
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->planeOffsets[0][20*64+28]==255);
		assert(map->planeOffsets[0][21*64+21]==24 && map->planeOffsets[0][27*64+21]==0);
	}
	std::cout<<"PASS cardinal floor relief continuity: both BGs, four flips, preserved relief, no reachability inflation, barriers/planes/diagonals rejected\n";
}
