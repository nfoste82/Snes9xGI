#include "frame.h"
#include "lighting_field_cache.h"
#include "profile_merge.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterProfile p;p.schemaVersion=28;p.gameTitle="Binding";p.romSha256=std::string(64,'a');
	RemasterTileContentId tile;tile.hashVersion=1;tile.bitDepth=4;tile.hash=123;
	p.assets[tile].tileId=tile;p.assets[tile].hasHeight=true;
	auto &wall=p.tileContexts[tile]["Wall"];wall.hasHeight=wall.hasNormals=true;wall.height.fill(16);
	for(int i=0;i<64;i++){wall.normalXyz[i*3]=128;wall.normalXyz[i*3+1]=255;wall.normalXyz[i*3+2]=128;}
	auto &floor=p.tileContexts[tile]["Floor"];floor.hasHeight=true;
	RemasterFrame f;f.width=16;f.height=8;f.area.id=320;f.originalRgb555.resize(128);
	f.mainPixels.resize(128);f.subPixels.resize(128);f.tileInstances.resize(2);
	for(auto &i:f.tileInstances){i.tileId=tile;i.source=RemasterSourceType::Background;i.sourceIndex=1;}
	for(unsigned y=0;y<8;y++)for(unsigned x=0;x<16;x++) {
		auto &pixel=f.mainPixels[y*16+x];pixel.instanceId=1+x/8;pixel.tilePixel=y*8+x%8;
	}
	std::array<int,4> sx={},sy={};sx[1]=2048;sy[1]=512;
	S9xRemasterLocateContextTiles(f,sx,sy);
	assert(f.contextLocations.at(1).x==256 && f.contextLocations.at(2).x==257);
	p.contextBindings[f.contextLocations.at(1)]={tile,"Wall"};
	p.contextBindings[f.contextLocations.at(2)]={tile,"Floor"};
	assert(S9xRemasterContextSelection(p,f,{tile},"Wall")==std::set<uint32_t>{1});
	assert(S9xRemasterContextSelection(p,f,{tile},"Floor")==std::set<uint32_t>{2});
	assert(S9xRemasterContextSelection(p,f,{tile},"").empty());
	assert(S9xRemasterContextSelection(p,f,{tile},"Unused").empty());
	assert(S9xRemasterContextSelection(p,f,{},"Wall").empty());
	{
		auto unassigned=p;unassigned.contextBindings.erase(f.contextLocations.at(2));
		assert(S9xRemasterContextSelection(unassigned,f,{tile},"")==std::set<uint32_t>{2});
		unassigned.contextBindings[f.contextLocations.at(1)].tile.hash++;
		assert(S9xRemasterContextSelection(unassigned,f,{tile},"Wall").empty());
		assert(S9xRemasterContextSelection(unassigned,f,{tile},"").size()==2);
		auto legacy=f;legacy.contextLocations.clear();
		assert(S9xRemasterContextSelection(p,legacy,{tile},"Wall").empty());
		auto hidden=f;hidden.mainPixels.clear();hidden.subPixels.clear();
		assert(S9xRemasterContextSelection(p,hidden,{tile},"Wall").empty());
		hidden.scenePixels=f.mainPixels;
		assert(S9xRemasterContextSelection(p,hidden,{tile},"Wall")==std::set<uint32_t>{1});
	}
	S9xRemasterApplyProfileToFrame(p,f);
	assert(S9xRemasterResolveFrameGeometry(f.assetMetadata[0],f,0).height[0]==16);
	assert(S9xRemasterResolveFrameGeometry(f.assetMetadata[0],f,1).height[0]==0);
	RemasterLightingFieldCache cache;cache.update(f,false);
	assert(cache.height[0]>cache.height[16]);
	p.contextBindings[f.contextLocations.at(1)].name="Floor";S9xRemasterApplyProfileToFrame(p,f);cache.update(f,false);
	assert(cache.height[0]==cache.height[16]);
	std::string text;std::vector<RemasterProfileDiagnostic>d;
	assert(S9xRemasterSerializeProfile(p,text,d));RemasterProfile q;assert(S9xRemasterParseProfile(text,q,d));
	assert(q.schemaVersion==29 && q.contextBindings.size()==2);
	auto disk=q,local=q;local.contextBindings[f.contextLocations.at(1)].name="Wall";
	disk.contextBindings[f.contextLocations.at(2)].name="Wall";
	RemasterProfileMergeResult merged;
	assert(S9xRemasterMergeProfiles(q,local,disk,{},false,merged,d));
	assert(merged.saved.contextBindings.size()==2 && merged.saved.contextBindings.at(f.contextLocations.at(1)).name=="Wall" &&
		merged.saved.contextBindings.at(f.contextLocations.at(2)).name=="Wall");
	std::vector<uint8_t> bytes;assert(S9xSerializeRemasterFrame(f,bytes));RemasterFrame replay;
	assert(S9xDeserializeRemasterFrame(bytes,replay));assert(replay.contextLocations.size()==2);
	S9xRemasterApplyProfileToFrame(q,replay);assert(replay.contextGeometry.size()==2);
	// Room and BG scope cannot alias outdoor bindings.
	replay.area.id=0x55;S9xRemasterLocateContextTiles(replay,sx,sy);S9xRemasterApplyProfileToFrame(q,replay);
	assert(replay.contextGeometry.empty());assert(replay.contextLocations.at(1).x==0);
	p.contextBindings.clear();p.tileContexts[tile].erase("Floor");S9xRemasterApplyProfileToFrame(p,f);
	assert(f.contextGeometry.size()==2); // one context needs no location records
	assert(S9xRemasterContextSelection(p,f,{tile},"Wall").size()==2);
	assert(S9xRemasterContextSelection(p,f,{tile},"").empty());
	assert(S9xRemasterSerializeProfile(p,text,d) && text.find("context_bindings")==std::string::npos);
	p.contextBindings[f.contextLocations.at(1)]={tile,"Wall"};
	assert(!S9xRemasterSerializeProfile(p,text,d)); // refuse location storage for one context
	std::cout<<"Context bindings: occurrence isolation, coordinates, cache updates, profile/frame roundtrip, sparse single-context behavior passed\n";
}
