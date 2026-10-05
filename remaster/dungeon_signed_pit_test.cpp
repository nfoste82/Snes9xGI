// One supported platform above a pit: no wall-foot floor is required.
#include "remaster.h"
#include <cassert>
#include <iostream>

int main(int argc, char **argv)
{
	using Surface = RemasterDungeonFloorContext::Surface;
	auto tiles = std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto attrs = std::make_unique<std::array<uint8_t,8192>>(); attrs->fill(1);
	auto map = std::make_unique<RemasterDungeonHeightMap>();
	RemasterDungeonFloorContext c; c.verifiedAlttpRom=c.indoors=true;
	c.roomIndex=7; c.linkX=10*8; c.linkY=9*8;
	c.placements=tiles->data(); c.collisionAttributes=attrs->data();
	for (int flip=0; flip<4; flip++) {
		*tiles={}; attrs->fill(1);
		for(int y=10;y<13;y++) for(int x=10;x<14;x++) {
			auto &t=(*tiles)[y*64+x]; t.surface=Surface::Floor; t.hasHeight=true; (*attrs)[y*64+x]=0;
		}
		// Reflect the chain and its canonical profile together.
		for(int n=0;n<3;n++) {
			int y=(flip&2)?9-n:13+n;
			for(int x=10;x<14;x++) {
				auto &t=(*tiles)[y*64+x]; t.surface=Surface::Wall; t.hasHeight=true;
				t.hflip=flip&1; t.vflip=flip&2;
				for(int yy=0;yy<8;yy++) for(int xx=0;xx<8;xx++) t.height[yy*8+xx]=7-yy;
			}
		}
		int endY=(flip&2)?7:15;
		(*tiles)[endY*64+14]=(*tiles)[endY*64+13]; // tangent, no direct floor
		(*tiles)[2*64+2]=(*tiles)[endY*64+13]; // truly isolated
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->offsets[11*64+11]==0);
		assert(map->offsets[endY*64+13]==-24);
		assert(map->offsets[endY*64+14]==-24);
		assert(map->offsets[2*64+2]==255);
		assert(map->requiredPlaneCells[1]==0 && map->solvedPlaneCells[1]==0);
		assert(map->structuralConflicts==0);
		assert(S9xRemasterSignedHeightField(-24,7)<0);
		assert(std::abs(S9xRemasterSignedHeightField(-24,7)*255.0f+17.0f)<0.0001f);
		assert(std::abs(S9xRemasterSignedHeightField(512,7)*255.0f-519.0f)<0.0001f);
		RemasterFrame f; f.width=f.height=8; f.originalRgb555.resize(64); f.mainPixels.resize(64); f.subPixels.resize(64);
		uint8_t indices[64]={}; RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
		RemasterFrameTileInstance i; i.source=RemasterSourceType::Background; i.sourceIndex=1; i.tileId=id;
		f.tileInstances.push_back(i);
		RemasterFrameAssetMetadata m; m.tileId=id; m.hasHeight=true; m.height=(*tiles)[endY*64+14].height;
		f.assetMetadata.push_back(m);
		for(int k=0;k<64;k++){ f.mainPixels[k].instanceId=1; f.mainPixels[k].tilePixel=k; }
		c.backgroundScrollX=14*8; c.backgroundScrollY=endY*8;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		assert(f.tileInstances[0].heightOffset==-24);
		assert(S9xRemasterFrameHeightRange(f)==std::make_pair(-24,-17));
		assert(f.assetMetadata[0].height==m.height);
		std::vector<uint8_t> bytes; assert(S9xSerializeRemasterFrame(f,bytes));
		RemasterFrame decoded; assert(S9xDeserializeRemasterFrame(bytes,decoded));
		assert(decoded.schemaVersion==26 && decoded.tileInstances[0].heightOffset==-24);
		// The previous schema's byte offset must still decode without reinterpretation.
		f.tileInstances[0].heightOffset=50; assert(S9xSerializeRemasterFrame(f,bytes));
		bytes.erase(bytes.end()-5,bytes.end()-2); // three extra offset bytes, before yaw/flag
		bytes[8]=25;
		assert(S9xDeserializeRemasterFrame(bytes,decoded));
		assert(decoded.schemaVersion==25 && decoded.tileInstances[0].heightOffset==50);
	}
	if (argc==2) {
		auto loaded=std::make_unique<RemasterDungeonHeightMap>();
		assert(S9xRemasterReadDungeonHeightMap(argv[1],*loaded));
		assert(loaded->hasPlaneOffsets && loaded->offsets[8*64+41]==-8);
		assert(loaded->planeOffsets[1][8*64+41]==-8);
	}
	assert(S9xRemasterHeightPreviewRangeValid(-32,0));
	std::cout<<"signed pit: four flips, descending/tangent/isolated walls, plane scope and frame26 passed\n";
}
