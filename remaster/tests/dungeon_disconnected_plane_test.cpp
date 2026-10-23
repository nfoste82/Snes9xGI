#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>
int main() {
 using Surface=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto attr=std::make_unique<std::array<uint8_t,8192>>();attr->fill(1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=0;c.linkX=80;c.linkY=80;
 c.placements=tiles->data();c.collisionAttributes=attr->data();
 for(int p=0;p<2;p++)for(int y=10;y<14;y++)for(int x=10;x<14;x++) {
  int i=y*64+x;(*attr)[p*4096+i]=0;auto &t=(*tiles)[i];t.hasHeight=true;t.surface=Surface::Floor;
  t.floorPlaneMask=3;t.distinctLowerUsage=true;
 }
 auto m=std::make_unique<RemasterDungeonHeightMap>();assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->disconnectedPlaneCandidates && m->planeRelationshipUnresolved);
 assert(m->candidateFloorCells[0]==16 && m->candidateFloorCells[1]==16);
 assert(m->unobservedFloorCells[1]==16 && m->requiredPlaneCells[1]==0);
 assert(m->planeOffsets[1][11*64+11]==255);
 c.linkPlane=1;assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));
 assert(m->unobservedFloorCells[0]==16 && m->requiredPlaneCells[0]==0);
 assert(m->planeOffsets[0][11*64+11]==255);
 for(auto &t:*tiles)t.floorPlaneMask=1;
 c.linkPlane=0;assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(!m->disconnectedPlaneCandidates);
 for(auto &t:*tiles){t.floorPlaneMask=3;t.distinctLowerUsage=false;}
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->candidateFloorCells[1]==0);
 (*attr)[11*64+11]=1;
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->candidateFloorCells[1]>0);
 std::array<uint8_t,16384> maps={};std::array<uint8_t,65536> graphics={};
 maps[8192]=1;c.constructedMaps=maps.data();c.graphics=graphics.data();
 auto key=S9xRemasterDungeonHeightInputKey(c,50);graphics[32]=1;
 assert(key!=S9xRemasterDungeonHeightInputKey(c,50));
	// End-to-end placed BG classifier: same index but distinct character
	// bases must classify independently, without mistaking a BG1 wall for floor.
	RemasterProfile profile;uint8_t flat[64]={},wall[64];std::fill_n(wall,64,1);
	RemasterTileContentId floorId={S9xRemasterHashTile(4,flat),1,4},wallId={S9xRemasterHashTile(4,wall),1,4};
	profile.assetGroups["dungeon_floor"].tileIds.push_back(floorId);profile.assets[floorId].hasHeight=true;
	profile.assetGroups["wall_faces"].tileIds.push_back(wallId);profile.assets[wallId].hasHeight=true;
	graphics.fill(0);maps.fill(0);c.bg1GraphicsBase=32768;
	for(int y=0;y<8;y++)graphics[32768+y*2]=255;
	S9xRemasterDungeonPlacements(c,profile,*tiles);
	assert((*tiles)[0].floorPlaneMask==1); // BG1 wall excluded
	key=S9xRemasterDungeonHeightInputKey(c,50);graphics[32768]=0;
	assert(key!=S9xRemasterDungeonHeightInputKey(c,50));
	graphics.fill(0);S9xRemasterDungeonPlacements(c,profile,*tiles);assert((*tiles)[0].floorPlaneMask==3);
 // Disjoint candidate floor bodies do not need overlap or a staircase.
 tiles->fill({});attr->fill(1);c.constructedMaps=nullptr;c.graphics=nullptr;c.linkPlane=0;
 for(int p=0;p<2;p++)for(int y=10;y<14;y++)for(int x=10+p*10;x<14+p*10;x++) {
  int i=y*64+x;auto &t=(*tiles)[i];t.surface=Surface::Floor;t.hasHeight=true;t.floorPlaneMask=1<<p;
  (*attr)[p*4096+i]=0;
 }
 assert(S9xRemasterBuildDungeonHeightMap(c,50,*m));assert(m->disconnectedPlaneCandidates);
 assert(m->candidateFloorCells[0]==16 && m->candidateFloorCells[1]==16 && m->unobservedFloorCells[1]==16);
 std::cout<<"PASS disconnected zero-stair candidate planes, overlap, either entry, wall exclusion, unknown separation, BG1 cache\n";
}
