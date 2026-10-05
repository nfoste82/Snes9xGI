#include "remaster.h"
#include <cassert>
#include <iostream>

int main()
{
	using Surface=RemasterDungeonFloorContext::Surface;
	auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto map=std::make_unique<RemasterDungeonHeightMap>();
	std::vector<uint8_t> attributes(8192,1);
	RemasterDungeonFloorContext c; c.verifiedAlttpRom=c.indoors=true; c.roomIndex=7;
	c.placements=tiles->data(); c.collisionAttributes=attributes.data();
	c.linkX=20*8; c.linkY=19*8;
	for (int flip=0; flip<4; flip++)
	{
		*tiles={}; attributes.assign(8192,1);
		// Raised floor (24) connected by three authored ramps to lower floor (0).
		for (int y : {20,26}) for (int yy=y; yy<y+3; yy++) for (int x=20;x<24;x++)
		{
			auto &t=tiles->at(yy*64+x); t.surface=Surface::Floor; t.hasHeight=true;
			attributes[yy*64+x]=0;
		}
		for (int y=23;y<26;y++) for(int x=20;x<24;x++)
		{
			auto &t=tiles->at(y*64+x); t.surface=Surface::Wall; t.hasHeight=true;
			for(int yy=0;yy<8;yy++) for(int xx=0;xx<8;xx++) t.height[yy*8+xx]=7-yy;
		}
		const int wx=(flip&1)?24:19, wy=(flip&2)?23:19, index=wy*64+wx;
		auto &mixed=tiles->at(index); mixed.surface=Surface::Wall; mixed.hasHeight=true;
		mixed.hflip=flip&1; mixed.vflip=flip&2;
		for(int y=0;y<8;y++) for(int x=0;x<8;x++)
		{
			mixed.height[y*8+x]=std::max(0,10-x-y);
			if(x+y>=10) mixed.groundedFloorMask|=UINT64_C(1)<<(y*8+x);
		}
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->planeOffsets[0][21*64+21]==24);
		// A straight mixed foot with an exact full seam exercises the datum
		// solver itself (the clipped diagonal application is tested below).
		const int testCell=21*64+19;
		auto &straight=tiles->at(testCell); straight.surface=Surface::Wall; straight.hasHeight=true;
		for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
			straight.height[y*8+x]=std::max(0,3-x);
			if(x>=3) straight.groundedFloorMask|=UINT64_C(1)<<(y*8+x);
		}
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->mixedFloorBases[0][testCell]==24 && map->mixedFloorBases[1][testCell]==255);
		// Missing classified support cannot be replaced by the nearest floor.
		const auto saved=tiles->at(21*64+20);
		tiles->at(21*64+20).surface=Surface::Unknown;
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->mixedFloorBases[0][testCell]==255);
		tiles->at(21*64+20)=saved;
		straight.groundedFloorMask=0; // zero profile alone must never ground a wall
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->mixedFloorBases[0][testCell]==255);
		straight={};
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		// Pixel foot contacts are intentionally independent of the accepted
		// full-face seam solve. Pin an already solved wall base for application.
		// Solver must retain the grounded datum only for a solved structure.
		// Attach through a same-plane zero foot to the actual raised floor.
		map->offsets[index]=20; map->structuralPlaneOffsets[0][index]=20;
		map->mixedFloorBases[0][index]=24;
		map->structuralPlaneOffsets[1][index]=255; map->mixedFloorBases[1][index]=0;
		assert(S9xRemasterMixedFloorBase(*map,index)==24);
		RemasterFrame f; f.width=f.height=8; f.mainPixels.resize(64); f.subPixels.resize(64);
		RemasterFrameTileInstance i; i.source=RemasterSourceType::Background; i.sourceIndex=1;
		i.hFlip=mixed.hflip; i.vFlip=mixed.vflip;
		f.tileInstances.push_back(i);
		RemasterFrameAssetMetadata m; m.tileId=i.tileId; m.hasHeight=m.hasNormals=true; m.height=mixed.height;
		m.normalXyz.fill(123); f.assetMetadata.push_back(m);
		for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
			auto &px=f.mainPixels[y*8+x]; px.instanceId=1;
			px.tilePixel=(i.vFlip?7-y:y)*8+(i.hFlip?7-x:x);
		}
		c.backgroundScrollX=wx*8; c.backgroundScrollY=wy*8;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		const auto &applied=f.tileInstances[0]; assert(applied.heightOffset==20 && applied.hasPlacementHeight);
		for(auto &px:f.mainPixels) assert(applied.heightOffset+applied.placementHeight[px.tilePixel]==std::max(24,20+int(m.height[px.tilePixel])));
		assert(f.assetMetadata[0].height==m.height && f.assetMetadata[0].normalXyz==m.normalXyz);
		map->structuralPlaneOffsets[1][index]=20; // ambiguous visible-plane identity
		assert(S9xRemasterMixedFloorBase(*map,index)==255);
		map->structuralPlaneOffsets[1][index]=255; map->mixedFloorBases[0][index]=255;
		assert(S9xRemasterMixedFloorBase(*map,index)==255);
		assert(S9xRemasterMixedFloorHeight(m.height,20,255)==m.height);
		assert(S9xRemasterMixedFloorHeight(m.height,30,24)==m.height); // never lower slope
	}
	std::cout<<"PASS mixed floor: H/V flips, raised24, preserved slopes/normals, unknown/ambiguous planes\n";
}
