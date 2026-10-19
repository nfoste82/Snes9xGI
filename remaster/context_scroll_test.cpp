#include "frame.h"
#include "lighting_field_cache.h"
#include <cassert>
#include <iostream>

int main()
{
	// Pending gameplay camera movements must not change displayed tile contexts.
	for (int origin : {0, 7, 8, 511, 512, 1023, 1024, 1920, 4095})
		for (int delta=-4;delta<=4;delta++)
			assert(S9xRemasterUnwrapRenderedScroll(origin+delta,origin&1023)==origin);
	RemasterProfile profile;
	RemasterTileContentId tile;tile.hashVersion=1;tile.bitDepth=4;tile.hash=0xb3881650e2d5d461;
	profile.assets[tile].tileId=tile;profile.assets[tile].hasHeight=true;
	auto &wall=profile.tileContexts[tile]["Wall"];wall.hasHeight=true;wall.height.fill(23);
	profile.tileContexts[tile]["Ground"].hasHeight=true;
	RemasterFrame frame;frame.width=16;frame.height=8;frame.area.id=320;
	frame.mainPixels.resize(128);frame.subPixels.resize(128);frame.tileInstances.resize(2);
	for(auto &instance:frame.tileInstances){instance.tileId=tile;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;}
	for(unsigned y=0;y<8;y++)for(unsigned x=0;x<16;x++){
		auto &pixel=frame.mainPixels[y*16+x];pixel.instanceId=1+x/8;pixel.tilePixel=y*8+x%8;
	}
	std::array<int,4> sx={},sy={};sx[1]=1920;sy[1]=1744;
	S9xRemasterLocateContextTiles(frame,sx,sy);
	profile.contextBindings[frame.contextLocations.at(1)]={tile,"Wall"};
	profile.contextBindings[frame.contextLocations.at(2)]={tile,"Ground"};
	const auto locations=frame.contextLocations;
	RemasterLightingFieldCache cache;
	for(auto delta:{std::pair<int,int>{0,0},{-1,0},{0,-1},{1,0},{0,1},{-3,-3}}){
		sx[1]=S9xRemasterUnwrapRenderedScroll(1920+delta.first,1920&1023);
		sy[1]=S9xRemasterUnwrapRenderedScroll(1744+delta.second,1744&1023);
		S9xRemasterLocateContextTiles(frame,sx,sy);
		assert(frame.contextLocations.size()==locations.size());
		for(const auto &entry:locations){const auto &actual=frame.contextLocations.at(entry.first);assert(!(actual<entry.second) && !(entry.second<actual));}
		S9xRemasterApplyProfileToFrame(profile,frame);cache.update(frame,false);
		assert(cache.height[0]==23/255.0f && cache.height[16]==0);
	}
	std::cout<<"Rendered camera context binding: north/west/east/south and wrap regression passed\n";
}
