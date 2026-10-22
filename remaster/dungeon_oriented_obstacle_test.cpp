#include "remaster.h"
#include <cassert>
#include <iostream>
int main()
{
	using C=RemasterDungeonFloorContext;
	auto words=std::make_unique<std::array<uint8_t,16384>>();
	auto gfx=std::make_unique<std::array<uint8_t,65536>>();
	auto attr=std::make_unique<std::array<uint8_t,8192>>();
	auto tiles=std::make_unique<std::array<C::Placement,4096>>();
	auto map=std::make_unique<RemasterDungeonHeightMap>();RemasterProfile profile;
	RemasterObstacleGroup group;group.name="horizontal";
	for(int n=0;n<5;n++) {
		uint8_t pixels[64];std::fill_n(pixels,64,n);
		for(int y=0;y<8;y++)for(int b=0;b<4;b++)(*gfx)[n*32+b/2*16+y*2+b%2]=(n&(1<<b))?255:0;
		RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};auto &a=profile.assets[id];a.tileId=id;a.hasHeight=true;
		if(!n)a.tileRole=RemasterTileRole::Floor;
		else {a.height.fill(7);for(int y=0;y<2;y++)group.members.push_back({id,n-1,y,0,false,bool(y),n==4});}
	}
	profile.obstacleGroups={group};C c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkX=80;c.linkY=64;
	c.constructedMaps=words->data();c.graphics=gfx->data();c.collisionAttributes=attr->data();c.placements=tiles->data();
	for(int flip=0;flip<4;flip++) {
		words->fill(0);attr->fill(1);
		for(int y=6;y<20;y++)for(int x=6;x<30;x++)(*attr)[y*64+x]=0;
		for(const auto &m:group.members) {
			const int x=16+((flip&1)?-m.x:m.x),y=12+((flip&2)?-m.y:m.y),i=y*64+x;
			const int w=(m.x+1)|((flip&1)?0x4000:0)|((m.vflip!=bool(flip&2))?0x8000:0);
			(*words)[i*2]=w;(*words)[i*2+1]=w>>8;(*attr)[i]=1;
		}
		S9xRemasterDungeonPlacements(c,profile,*tiles);
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->authoredObstacleCells==8 && map->solvedAuthoredObstacleCells==8 && map->unmatchedAuthoredObstacleCells==0);
		for(const auto &t:*tiles)if(t.authoredObstacle)assert(t.authoredObstacleDirection==((flip&1)?2:3));
		// Symmetric V artwork has ONE semantic owner, not opposite south/north
		// support guesses. Removing the actual outward floor leaves it unknown.
		const int edge=16+((flip&1)?-4:4);
		for(int y=10;y<=14;y++)(*attr)[y*64+edge]=1;
		assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
		assert(map->solvedAuthoredObstacleCells==0);
	}
	std::cout<<"PASS horizontal authored contacts: west/east mirrored supports, V-symmetric ownership, real outward floor required\n";
}
