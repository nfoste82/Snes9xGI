#include "../runtime/sprite_obstacle_replay.h"
#include <cassert>
#include <iostream>

int main(int argc,char **argv)
{
	for(int base:{-16,0,50})for(int start:{48,68,92,112})for(int effects:{0,2}) {
		RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
		c.roomIndex=7;c.hasTerrainActors=true;
		const int head=start+effects,body=head+1;
		c.terrainOam[head]={0,0,0x20,0x28,2};c.terrainOam[body]={0,8,0x22,0x28,2};
		auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=7;
		map->hasPlaneOffsets=true;map->offsets.fill(99);for(auto &p:map->planeOffsets)p.fill(99);
		auto diagnostics=std::make_shared<RemasterRoomDiagnostics>();
		// Blocked bottom row has structural height99 but is not standable.
		// Only the row immediately above it within the body footprint is floor.
		for(int x=0;x<2;x++) {
			auto &d=diagnostics->cells[64+x];d.floorId=3;d.floorBase=base;d.planeObserved=true;
			map->planeOffsets[0][64+x]=base;
			// Producer body extends through y23 even when ink ends at18.
			diagnostics->cells[128+x]=d;map->planeOffsets[0][128+x]=base;
		}
		map->roomDiagnostics=diagnostics;
		RemasterFrame f;f.width=16;f.height=19;f.mainPixels.resize(304);
		for(int y=0;y<19;y++) {
			RemasterFrameTileInstance i;i.source=RemasterSourceType::Object;
			i.sourceIndex=y<8?head:body;i.tileNumber=y<8?0x20:0x22;
			i.vramAddress=0x8000+i.tileNumber*32;i.palette=4;
			i.heightOffset=2;i.hasPlacementHeight=true;i.placementHeight.fill(7);
			f.tileInstances.push_back(i);
			for(int x=0;x<16;x++)f.mainPixels[y*16+x].instanceId=y+1;
		}
		auto original=f;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances){assert(i.heightOffset==base+2);assert(i.placementHeight[0]==7);}
		// Torso overlaps another floor/relief while transparent or clipped lower
		// ink ends early. Only the attested producer contact row may vote.
		for(int x=0;x<2;x++)map->planeOffsets[0][64+x]=base+30;
		f=original;S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==base+2);
		for(int x=0;x<2;x++)map->planeOffsets[0][64+x]=base;
		// Plane1 must consume its own floor, not the plane0 artwork/base.
		for(int x=0;x<2;x++) {
			diagnostics->cells[4096+64+x]=diagnostics->cells[64+x];
			map->planeOffsets[1][64+x]=base+8;
			diagnostics->cells[4096+128+x]=diagnostics->cells[128+x];map->planeOffsets[1][128+x]=base+8;
		}
		c.linkPlane=1;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==base+2); // delayed follower remains plane0
		c.terrainOam[head].flags=c.terrainOam[body].flags=0x18;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==base+10);
		c.terrainOam[head].flags=c.terrainOam[body].flags=0x28;
		c.linkPlane=0;
		// Incorrect producer observation must not join independent body parts.
		// Give the head-only footprint a different floor to make that observable.
		for(int x=0;x<2;x++) {
			auto &d=diagnostics->cells[x];d.floorId=4;d.floorBase=base+20;d.planeObserved=true;
			map->planeOffsets[0][x]=base+20;
		}
		for(int rejection=0;rejection<4;rejection++) {
			f=original;auto &i=f.tileInstances.back();
			if(rejection==0)i.tileNumber=0x24;
			if(rejection==1)i.vramAddress+=32;
			if(rejection==2)i.palette=3;
			if(rejection==3)i.vFlip=!i.vFlip;
			S9xRemasterApplyDungeonHeightMap(f,c,*map);
			assert(f.tileInstances[0].heightOffset==base+22);
		}
		c.terrainOam[head].name=0x24;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		assert(f.tileInstances[0].heightOffset==base+22);
		assert(f.tileInstances.back().heightOffset==base+2);
		c.terrainOam[head].name=0x20;
		// Special-transition floor has a solved semantic datum but deliberately
		// no ordinary movement component. Never manufacture a floorId to use it.
		for(int x=0;x<2;x++)for(int row:{64,128}) {
			auto &d=diagnostics->cells[row+x];d.floorId=-1;d.semanticFloorSupport=true;
		}
		// Animated pose may flip only the lower half (Tagalong DMA flags04).
		c.terrainOam[body].flags|=0x40;
		for(auto &i:original.tileInstances)if(i.sourceIndex==body)i.hFlip=true;
		f=original;S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==base+2 && i.placementHeight[0]==7);
		// A semantic floor with unknown/conflicting support must veto, not be
		// ignored in favour of another solved cell in the same footprint.
		map->planeOffsets[0][129]=255;f=original;S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==2);
		map->planeOffsets[0][129]=base;diagnostics->cells[129].heightConflict=true;
		f=original;S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==2);
		diagnostics->cells[129].heightConflict=false;
		// A second floor at another base is a conflict, not a max-height vote.
		map->planeOffsets[0][129]=base+10;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==2);
		// No walkable support cannot promote structural relief into actor support.
		diagnostics->cells[64].floorId=diagnostics->cells[65].floorId=-1;f=original;
		diagnostics->cells[128].floorId=diagnostics->cells[129].floorId=-1;
		for(int x=0;x<2;x++)for(int row:{64,128})diagnostics->cells[row+x].semanticFloorSupport=false;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==2);
	}
	if(argc>1) {
		RemasterFrame f;assert(S9xReadRemasterFrame(argv[1],f));
		RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;
		assert(S9xRemasterLoadProfile(std::string(argv[1])+".profile.toml",p,errors));
		assert(S9xRemasterRestoreSpriteObstacleSupport(argv[1],f,p));
		auto support=f.spriteObstacleSupport;
		auto map=std::make_unique<RemasterDungeonHeightMap>();
		map->roomIndex=support->context.roomIndex;map->hasPlaneOffsets=true;
		auto d=std::make_shared<RemasterRoomDiagnostics>();
		for(int n=0;n<8192;n++) {
			const auto &s=support->cells[n];auto &v=d->cells[n];
			v.floorId=s.component;v.floorBase=s.base;v.planeObserved=s.observed;v.wall=s.wall;v.heightConflict=s.conflict;v.semanticFloorSupport=s.semanticFloorSupport;
			map->planeOffsets[n/4096][n%4096]=s.planeBase;
		}
		map->roomDiagnostics=d;
		int oldHead=-999,oldBody=-999;
		for(auto &i:f.tileInstances)if(i.source==RemasterSourceType::Object){
			if(i.sourceIndex==68)oldHead=i.heightOffset;
			if(i.sourceIndex==69)oldBody=i.heightOffset;
			i.heightOffset=0;
		}
		assert(oldHead==50 && oldBody==0);
		S9xRemasterApplyDungeonHeightMap(f,support->context,*map);
		size_t members=0;
		for(const auto &i:f.tileInstances)if(i.source==RemasterSourceType::Object && (i.sourceIndex==68 || i.sourceIndex==69)){
			// Historical packet records50; current own-profile authored-stair
			// solver reproduces24. Compare to independently solved support,
			// never substitute the historical configured-rise fallback.
			assert(i.heightOffset==map->planeOffsets[0][10*64+28]);members++;
		}
		assert(members>0);std::cout<<"Pinned Zelda capture: "<<members<<" records share reproduced own-profile support (recorded head50/body0)\n";
	}
	std::cout<<"NPC walkable floor support PASS\n";
}
