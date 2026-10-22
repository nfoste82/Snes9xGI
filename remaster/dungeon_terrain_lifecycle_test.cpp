// Owned BG blocker versus persistent support; frame-local carried/thrown OAM.
#include "remaster.h"
#include <cassert>
#include <iostream>
int main(){
 using C=RemasterDungeonFloorContext;using S=C::Surface;
 auto c=std::make_unique<C>();c->verifiedAlttpRom=c->indoors=true;c->roomIndex=7;c->linkX=80;c->linkY=80;
 auto t=std::make_unique<std::array<C::Placement,4096>>();auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
 for(int y=8;y<16;y++)for(int x=8;x<16;x++){int i=y*64+x;(*attrs)[i]=0;(*t)[i].surface=S::Floor;(*t)[i].hasHeight=true;}
 for(int y=10;y<12;y++)for(int x=10;x<12;x++){int i=y*64+x;(*attrs)[i]=0x73;(*t)[i].surface=S::Obstacle;(*t)[i].hasHeight=true;(*t)[i].height.fill(5);(*t)[i].removableSupportPlanes=1;}
 c->placements=t->data();c->collisionAttributes=attrs->data();auto map=std::make_unique<RemasterDungeonHeightMap>();
 assert(S9xRemasterBuildDungeonHeightMap(*c,50,*map));assert(map->planeOffsets[0][10*64+10]==0 && (*attrs)[10*64+10]==0x73);
 auto resting=map->planeOffsets[0];
 for(int y=10;y<12;y++)for(int x=10;x<12;x++){int i=y*64+x;(*attrs)[i]=0;(*t)[i].surface=S::Floor;(*t)[i].height.fill(0);(*t)[i].removableSupportPlanes=0;}
 assert(S9xRemasterBuildDungeonHeightMap(*c,50,*map));assert(map->planeOffsets[0]==resting); // pickup/break replaces BG, support remains
 for(int y=10;y<12;y++)for(int x=10;x<12;x++){int i=y*64+x;(*attrs)[i]=0x73;(*t)[i].surface=S::Obstacle;(*t)[i].height.fill(5);(*t)[i].removableSupportPlanes=1;}
 assert(S9xRemasterBuildDungeonHeightMap(*c,50,*map));
 for(auto &v:*t)v.removableSupportPlanes=0;
 auto old=std::make_unique<RemasterDungeonHeightMap>();assert(S9xRemasterBuildDungeonHeightMap(*c,50,*old));assert(map->requiredPlaneCells[0]==old->requiredPlaneCells[0] && map->persistentSupportRequired[0]>0);
 // Both halves/four children get the ONE owner offset, not individual visible
 // bottoms. Link plane differs deliberately from thrown object's own plane.
 c->hasTerrainActors=true;c->groundedEvidence=true;c->linkPlane=0;c->linkX=80;c->linkY=80;c->terrainActors[4]={80,85,10,2,16,0};c->terrainOam[20]={80,69,0x46,0x2c,2};
  map->planeOffsets[0].fill(24);map->planeOffsets[1].fill(-8);map->hasPlaneOffsets=true;
	// The unrelated screen-space actor below stands on a real movement floor.
	// Structural plane offsets alone no longer qualify as walkable support.
	auto screenFloor=std::make_shared<RemasterRoomDiagnostics>(*map->roomDiagnostics);
	const int screenCell=64+1; // lone unrelated pixel at screen(15,15)
	screenFloor->cells[screenCell].floorId=0;screenFloor->cells[screenCell].floorBase=24;
	screenFloor->cells[screenCell].planeObserved=true;screenFloor->cells[screenCell].wall=false;
	screenFloor->cells[screenCell].heightConflict=false;map->roomDiagnostics=screenFloor;
  auto run=[&](int expected,bool overwritten=false){
  auto f=std::make_unique<RemasterFrame>();f->width=16;f->height=16;f->mainPixels.resize(256);
   for(int n=0;n<4;n++){RemasterFrameTileInstance i;i.source=RemasterSourceType::Object;i.sourceIndex=20;i.tileNumber=0x46+(n&1)+(n/2)*16;i.palette=6;i.heightOffset=3;i.hFlip=bool(c->terrainOam[20].flags&0x40);i.vFlip=bool(c->terrainOam[20].flags&0x80);f->tileInstances.push_back(i);}
  // Same artwork in an unrelated slot must not get terrain z/lift.
  auto unrelated=f->tileInstances[0];unrelated.sourceIndex=21;f->tileInstances.push_back(unrelated);
  for(int y=0;y<16;y++)for(int x=0;x<16;x++)f->mainPixels[y*16+x].instanceId=1+(x/8)+(y/8)*2;
   f->mainPixels[255].instanceId=5;
   if(overwritten)for(int n=0;n<4;n++)f->tileInstances[n].hFlip=!f->tileInstances[n].hFlip;
  S9xRemasterApplyDungeonHeightMap(*f,*c,*map);
  for(int n=0;n<4;n++)assert(f->tileInstances[n].heightOffset==3+expected && !f->tileInstances[n].hasPlacementHeight);
  assert(f->tileInstances[4].heightOffset==27); // legacy unrelated actor path, no pot lift
 };
  run(24+16+3);
  // Actual captured C2 producer retains facing bits (062/019 flags=0x6c).
  // All four children, not just the first hash, inherit the one owned base.
  for(int flips=0;flips<4;flips++){c->terrainOam[20].flags=0x2c|(flips<<6);run(24+16+3);}
  c->terrainOam[20].flags=0x2c;
  run(0,true); // final instance transform disagrees with producer output
  c->terrainActors[4].y--;c->terrainOam[20].y--;run(24+16+4); // animation displacement once
  c->terrainActors[4].y++;c->terrainOam[20].y++;
  const int ownerCell=12*64+11;map->planeOffsets[0][ownerCell]=255;run(0);map->planeOffsets[0][ownerCell]=24;
 c->groundedEvidence=false;run(0);c->groundedEvidence=true; // airborne owner unsupported, no floor guess
 c->terrainActors[4]={80,85,9,2,12,1};c->terrainOam[20].y=73;run(-8+12);
 c->terrainActors[4].z=0;c->terrainOam[20].y=85;run(-8);
 c->terrainActors[5]=c->terrainActors[4];run(0); // duplicate identity rejects, no neighbor fallback
 c->terrainActors[5]={};c->terrainOam[22]=c->terrainOam[20];run(0);c->terrainOam[22]={}; // ambiguous output slot also rejects
 c->terrainActors[5]={};c->terrainActors[4].state=0;run(24); // destroyed owner is not retained
 c->terrainActors[4].state=9;c->terrainOam[20].x++;run(24); // mismatched producer cannot apply z
 // Additive evidence is independently versioned; old schema30 and ALTPDI1
 // remain untouched. Pinning copies values, not live WRAM pointers.
 auto &state=S9xRemasterState();state.activeProfile.romSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 if(const char *path=std::getenv("ALTTP_TERRAIN_TEST_EVIDENCE")) {
  std::vector<RemasterProfileDiagnostic> errors;assert(S9xRemasterLoadProfile("Profiles/ALttP/alttp-profile.toml",state.activeProfile,errors));
  c->hasObstacleActors=true;c->obstacleActors[0]={778,2595,0xee,9,0,0,16,2,0,true};
  auto pinned=S9xRemasterPinDungeonCaptureEvidence(c.get(),state.activeProfile,nullptr,0,0,false);
  c->terrainActors[4].z=91;
  c->obstacleActors[0].plane=1;c->obstacleActors[0].x=999;
  S9xRemasterWriteDungeonCaptureEvidence(path,pinned->context,pinned->profile,nullptr,0,0,false);
  C restored;assert(S9xRemasterReadTerrainEvidence(std::string(path)+".terrain-input.bin",restored));
  assert(restored.terrainActors[4].z==0 && restored.hasTerrainActors);
  assert(S9xRemasterReadSpriteObstacleEvidence(std::string(path)+".sprite-input.bin",restored));
  assert(restored.hasObstacleActors && restored.obstacleActors[0].plane==0 && restored.obstacleActors[0].x==778);
  assert(restored.obstacleActors[0].hasMotion && restored.obstacleActors[0].xSubpixel==16 && restored.obstacleActors[0].xVelocity==2);
  std::ifstream evidence(std::string(path)+".sprite-input.bin",std::ios::binary|std::ios::ate);
  assert(evidence.tellg()==204);
 }
 std::cout<<"terrain support / carried / thrown / landed / ambiguous / reset PASS\n";
}
