#include "remaster.h"
#include <cassert>

uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;

int main()
{
	using Surface=RemasterDungeonFloorContext::Surface;
	auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto attrs=std::make_unique<std::array<uint8_t,8192>>();attrs->fill(1);
	auto map=std::make_unique<RemasterDungeonHeightMap>();
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
	c.placements=tiles->data();c.collisionAttributes=attrs->data();
	auto floor=[&](int x,int y,int plane) {
		for(int yy=y;yy<y+3;yy++)for(int xx=x;xx<x+4;xx++) {
			const int i=yy*64+xx;(*attrs)[plane*4096+i]=0;
			auto &t=(*tiles)[i];t.surface=Surface::Floor;t.hasHeight=true;t.tileHash=12;
		}
	};
	floor(8,19,0);floor(30,19,0);
	for(int y=15;y<19;y++)for(int x=8;x<12;x++) {
		auto &t=(*tiles)[y*64+x];t.surface=Surface::Wall;t.hasHeight=true;t.tileHash=13;
		for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=2*(7-yy);
	}
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	const auto &d=*map->roomDiagnostics;
	assert(d.cells[20*64+8].floorBase==d.cells[20*64+30].floorBase);
	assert(d.cells[20*64+8].floorId!=d.cells[20*64+30].floorId);
	for(int y=15;y<19;y++) {
		assert(d.cells[y*64+8].uphillDirections==1); // north increasing height
		assert(d.cells[y*64+8].lowerFloorDistance==19-y); // not base/8
		assert(d.cells[y*64+8].lowerFloorId==d.cells[20*64+8].floorId);
	}
	const auto bases=map->offsets;
	for(int y=15;y<19;y++)for(int x=8;x<12;x++) {
		auto &t=(*tiles)[y*64+x];t.vflip=true;
		for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=2*yy;
	}
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->offsets==bases);
	assert(map->roomDiagnostics->cells[15*64+8].uphillDirections==1);
	// True reversal has the foot on the opposite side, not nearest old floor.
	for(int y=15;y<19;y++)for(int x=8;x<12;x++)(*tiles)[y*64+x].vflip=false;
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	assert(map->roomDiagnostics->cells[15*64+8].uphillDirections==2);
	assert(map->roomDiagnostics->cells[15*64+8].lowerFloorDistance==-1);
	// Unsolved walls still expose authored direction; flat/nonmonotone unknown slopes do not.
	(*tiles)[2*64+2]=(*tiles)[15*64+8];
	(*tiles)[2*64+3]=(*tiles)[2*64+2];(*tiles)[2*64+3].height.fill(0);
	(*tiles)[2*64+4]=(*tiles)[2*64+2];(*tiles)[2*64+4].height[0]=99;
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	assert(map->roomDiagnostics->cells[2*64+2].uphillDirections==2);
	assert(map->roomDiagnostics->cells[2*64+2].lowerFloorDistance==-1);
	assert(map->roomDiagnostics->cells[2*64+3].uphillDirections==0);
	assert(map->roomDiagnostics->cells[2*64+4].uphillDirections==0);
	// Affine diagonal HEIGHT axis, independently mirrored by canonical H/V flips.
	auto &diagonal=(*tiles)[3*64+3];diagonal.surface=Surface::Wall;diagonal.hasHeight=true;
	for(int y=0;y<8;y++)for(int x=0;x<8;x++)diagonal.height[y*8+x]=x+y;
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	assert(map->roomDiagnostics->cells[3*64+3].uphillDirections==(1<<7));
	diagonal.hflip=true;diagonal.vflip=true;
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	assert(map->roomDiagnostics->cells[3*64+3].uphillDirections==(1<<4));
	// Same BG and same XY, independent gameplay-plane identities.
	floor(30,19,1);c.stairCount=1;c.stairs[0].changesPlane=true;
	assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
	assert(map->roomDiagnostics->cells[20*64+30].floorId>=0);
	assert(map->roomDiagnostics->cells[4096+20*64+30].floorId>=0);
	assert(map->roomDiagnostics->cells[20*64+30].floorId!=map->roomDiagnostics->cells[4096+20*64+30].floorId);
	// Both corner branches required, even equal elevations on disconnected floors.
	auto corner=std::make_unique<RemasterRoomDiagnostics>();
	auto required=std::make_unique<std::array<uint8_t,8192>>();
	corner->cells[0].wall=true;(*required)[0]=2|8;
	corner->cells[1].floorId=11;corner->cells[1].floorBase=0;
	corner->cells[2].floorId=12;corner->cells[2].floorBase=0;
	S9xRemasterResolveRoomDiagnosticDistances(*corner,{{0,1,2}},*required);
	assert(corner->cells[0].lowerFloorDistance==-1);
	S9xRemasterResolveRoomDiagnosticDistances(*corner,{{0,1,2},{0,2,8}},*required);
	assert(corner->cells[0].lowerFloorDistance==-2);
	// Identified lower support with a contradictory HEIGHT remains countable.
	auto conflicted=std::make_unique<RemasterRoomDiagnostics>();
	conflicted->cells[0].wall=true;conflicted->cells[0].heightConflict=true;
	conflicted->cells[1].floorId=77;conflicted->cells[1].heightConflict=true;
	required->fill(0);(*required)[0]=2;
	S9xRemasterResolveRoomDiagnosticDistances(*conflicted,{{0,1,2}},*required);
	assert(conflicted->cells[0].lowerFloorDistance==1);
	assert(conflicted->cells[0].lowerFloorBase==255 && conflicted->cells[0].heightConflict);
	// ROM-owned, deferred diagonal members: end-to-end TILE distance, not
	// accepted seam equalities. No change to absolute solver placement arrays.
	auto dt=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto da=std::make_unique<std::array<uint8_t,8192>>();da->fill(1);
	auto dm=std::make_unique<RemasterDungeonHeightMap>();
	auto dc=c;dc.placements=dt->data();dc.collisionAttributes=da->data();dc.stairCount=0;
	for(int y=13;y<16;y++)for(int x=13;x<17;x++) {
		auto &t=(*dt)[y*64+x];t.surface=Surface::Floor;t.hasHeight=true;(*da)[y*64+x]=0;
	}
	for(int y=10;y<=13;y++)for(int x=10;x<=13;x++)if(x!=13 || y!=13) {
		auto &t=(*dt)[y*64+x];t.hasHeight=true;
		for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=std::min(10,14-xx-yy);
	}
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	const auto originalOffsets=dm->offsets;
	const auto originalPlanes=dm->planeOffsets,originalStructural=dm->structuralPlaneOffsets;
	for(auto &t:*dt)if(t.hasHeight && t.surface==Surface::Unknown)t.diagnosticWallTemplateOffset=0x420;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->offsets==originalOffsets && dm->planeOffsets==originalPlanes && dm->structuralPlaneOffsets==originalStructural);
	for(int k=10;k<13;k++) {
		const auto &d=dm->roomDiagnostics->cells[k*64+k];
		assert(d.wall && d.uphillDirections==(1<<4));
		assert(d.lowerFloorDistance==13-k);
		assert(d.wallSource==RemasterRoomDiagnostic::WallSource::RomAssembly);
	}
	// Unknown special/pit collision is not floor zero; occluded plane is not
	// permission to use the other plane's lower-floor path.
	(*da)[13*64+13]=0x20;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[12*64+12].lowerFloorDistance==-1);
	assert(dm->roomDiagnostics->cells[4096+12*64+12].lowerFloorDistance==-1);
	auto &normalWall=(*dt)[2*64+2];normalWall.diagnosticWallTemplateOffset=0x420;
	normalWall.diagnosticNormalUphill=1<<7;normalWall.hflip=normalWall.vflip=true;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[2*64+2].uphillDirections==(1<<4));
	normalWall.diagnosticNormalUphill=0;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[2*64+2].wall && dm->roomDiagnostics->cells[2*64+2].axisUnknown);
	// Captured061 quantized half-slope is monotone, not integer affine.
	// Proven diagonal ownership and agreeing normals distinguish it from a
	// two-face corner; V flip transforms NW into SW, never W+S corner arrows.
	normalWall.hasHeight=true;normalWall.vflip=true;normalWall.hflip=false;
	normalWall.diagnosticNormalUphill=16;
	for(int y=0;y<8;y++)for(int x=0;x<8;x++)normalWall.height[y*8+x]=(14-x-y)/2;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[2*64+2].uphillDirections==64);
	// Reproduced support plus OWN authored floor trim survives a solver-deferred
	// nonflat role. It must not become a wall or require an invented zero array.
	(*da)[13*64+13]=0;
	for(int y=13;y<16;y++)for(int x=13;x<17;x++) {
		auto &t=(*dt)[y*64+x];t.surface=Surface::Unknown;t.diagnosticAuthoredFloor=true;t.height[0]=1;
	}
	dc.placementBackgroundIndex=0;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[14*64+14].floorId>=0);
	for(int y=13;y<16;y++)for(int x=13;x<17;x++)(*dt)[y*64+x].diagnosticAuthoredFloor=false;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[14*64+14].floorId==-1);
	for(int y=13;y<16;y++)for(int x=13;x<17;x++)(*dt)[y*64+x].diagnosticRomFloorTrim=true;
	assert(S9xRemasterBuildDungeonHeightMap(dc,50,*dm));
	assert(dm->roomDiagnostics->cells[14*64+14].floorId>=0);
	assert((*dt)[14*64+14].surface==Surface::Unknown); // no solver role promotion
	// Projection must retain both planes, reject wrong producers and never serialize diagnostics.
	RemasterFrame f;f.width=f.height=1;f.mainPixels.resize(1);f.mainPixels[0].instanceId=1;
	RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;
	instance.sourceIndex=1;instance.tileId={12,1,4};f.tileInstances.push_back(instance);
	c.backgroundScrollX=30*8;c.backgroundScrollY=20*8;
	S9xRemasterProjectRoomDiagnostics(f,c,map.get(),nullptr);
	assert(f.mainRoomDiagnostics[0].planes[0].floorId!=f.mainRoomDiagnostics[0].planes[1].floorId);
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff); // equal-base ambiguity
	f.tileInstances[0].hFlip=true;S9xRemasterProjectRoomDiagnostics(f,c,map.get(),nullptr);
	assert(f.mainRoomDiagnostics[0].planes[0].floorId==-1);
	// Lower BG1 presentation requires an exposed own floor and every branch of
	// its exact downhill path. Equal/unknown heights alone cannot select a plane.
	auto lowerMap=std::make_unique<RemasterDungeonHeightMap>();
	auto supportMap=std::make_unique<RemasterDungeonHeightMap>();
	auto lowerDs=std::make_shared<RemasterRoomDiagnostics>();
	auto supportDs=std::make_shared<RemasterRoomDiagnostics>();
	lowerMap->roomDiagnostics=lowerDs;supportMap->roomDiagnostics=supportDs;
	lowerMap->roomIndex=supportMap->roomIndex=7;
	for(auto &plane:lowerMap->structuralPlaneOffsets)plane.fill(255);
	auto words=std::make_unique<std::array<uint8_t,16384>>();
	auto graphics=std::make_unique<std::array<uint8_t,65536>>();
	c.constructedMaps=words->data();c.graphics=graphics->data();c.graphicsBase=0;
	c.backgroundScrollX=8*8;c.backgroundScrollY=18*8;
	f.tileInstances[0].sourceIndex=0;f.tileInstances[0].hFlip=false;
	const int wall=18*64+8,foot=19*64+8;
	lowerMap->producerHashes[wall]=12;
	for(int p=0;p<2;p++) {
		auto &v=lowerDs->cells[p*4096+wall];v.room=7;v.cell=wall;v.background=0;
		v.wall=true;v.planeObserved=true;v.uphillDirections=1;
	}
	auto &w=lowerDs->cells[4096+wall];w.lowerFloorDistance=1;w.lowerFloorId=4100;
	auto &s=lowerDs->cells[4096+foot];s.planeObserved=true;s.floorId=s.lowerFloorId=4100;s.lowerFloorDistance=0;
	supportDs->cells[4096+foot].floorId=4100+8192;
	(*attrs)[4096+wall]=1;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==1);
	lowerMap->structuralPlaneOffsets[0][wall]=20;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==0); // never override solved visual ownership
	lowerMap->structuralPlaneOffsets[0][wall]=255;
	lowerDs->cells[foot].floorId=100;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff); // overlapping support not resolved by a hole
	lowerDs->cells[foot].floorId=-1;
	// Nontransparent foreign BG2 word vetoes ownership even with numeric path.
	(*words)[wall*2]=1;(*graphics)[32]=1;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff);
	(*words)[wall*2]=0;
	// A corner requires BOTH facings; do not select from one convenient branch.
	w.uphillDirections=1|4;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff);
	w.uphillDirections=1;(*graphics)[0]=1;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),lowerMap.get());
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff); // foot no longer exposed
	// BG2 proven blocking wall versus its nonblocking underpass: a numeric
	// diagnostic path selects presentation only when neither base is solved.
	f.tileInstances[0].sourceIndex=1;supportMap->producerHashes[wall]=12;
	for(auto &p:supportMap->structuralPlaneOffsets)p.fill(255);
	for(int p=0;p<2;p++) {
		auto &d=supportDs->cells[p*4096+wall];d.wall=true;d.planeObserved=true;
		d.wallTemplateOffset=0x2c8;d.room=7;d.cell=wall;d.background=1;
	}
	supportDs->cells[wall].lowerFloorDistance=3;(*attrs)[wall]=1;(*attrs)[4096+wall]=0;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),nullptr);
	assert(f.mainRoomDiagnostics[0].visiblePlane==0);
	supportDs->cells[4096+wall].lowerFloorDistance=2;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),nullptr);
	assert(f.mainRoomDiagnostics[0].visiblePlane==0xff); // competing path
	supportDs->cells[4096+wall].lowerFloorDistance=-1;
	supportMap->structuralPlaneOffsets[1][wall]=9;
	S9xRemasterProjectRoomDiagnostics(f,c,supportMap.get(),nullptr);
	assert(f.mainRoomDiagnostics[0].visiblePlane==1); // solved ownership wins
	RemasterFrame minimal;minimal.width=minimal.height=1;minimal.originalRgb555.resize(1);
	minimal.mainPixels.resize(1);minimal.subPixels.resize(1);
	std::vector<uint8_t> before,after;assert(S9xSerializeRemasterFrame(minimal,before));
	minimal.mainRoomDiagnostics.resize(1);minimal.mainRoomDiagnostics[0].planes[0].floorId=42;
	assert(S9xSerializeRemasterFrame(minimal,after));assert(before==after);
}
