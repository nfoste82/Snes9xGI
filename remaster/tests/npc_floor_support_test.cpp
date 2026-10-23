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
		// Plane1 must consume its own floor, not the plane0 artwork/base.
		for(int x=0;x<2;x++) {
			diagnostics->cells[4096+64+x]=diagnostics->cells[64+x];
			map->planeOffsets[1][64+x]=base+8;
		}
		c.linkPlane=1;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==base+10);
		c.linkPlane=0;
		// Incorrect producer observation must not join independent body parts.
		// Give the head-only footprint a different floor to make that observable.
		for(int x=0;x<2;x++) {
			auto &d=diagnostics->cells[x];d.floorId=4;d.floorBase=base+20;d.planeObserved=true;
			map->planeOffsets[0][x]=base+20;
		}
		c.terrainOam[head].name=0x24;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		assert(f.tileInstances[0].heightOffset==base+22);
		assert(f.tileInstances.back().heightOffset==base+2);
		c.terrainOam[head].name=0x20;
		// A second floor at another base is a conflict, not a max-height vote.
		map->planeOffsets[0][65]=base+10;f=original;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		for(const auto &i:f.tileInstances)assert(i.heightOffset==2);
		// No walkable support cannot promote structural relief into actor support.
		diagnostics->cells[64].floorId=diagnostics->cells[65].floorId=-1;f=original;
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
			v.floorId=s.component;v.floorBase=s.base;v.planeObserved=s.observed;v.wall=s.wall;v.heightConflict=s.conflict;
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
			assert(i.heightOffset==50);members++;
		}
		assert(members>0);std::cout<<"Pinned Zelda capture: "<<members<<" records share floor50 (was head50/body0)\n";
	}
	std::cout<<"NPC walkable floor support PASS\n";
}
