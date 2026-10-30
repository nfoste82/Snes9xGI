#ifndef S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H
#define S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H

#include "dungeon_visible_face.h"
#include "dungeon_visible_face_templates.h"
#include "frame.h"
#include "dungeon_wall_band.h"
#include "dungeon_out_of_bounds.h"
#include <unordered_map>

#define REMASTER_VISIBLE_ROOM_CANDIDATES 1
struct RemasterVisibleFaceCandidate {
	int section=-1,axis=-1,down=0,family=0;
	std::array<int,6> footprint={}; bool ambiguous=false;
};
struct RemasterVisibleFaceLocalInput {
	std::array<int16_t,64> height={};
	RemasterDungeonFloorContext::Surface surface={};
	bool hasHeight=false,hflip=false,vflip=false,ambiguous=false;
	bool operator==(const RemasterVisibleFaceLocalInput &b)const {
		return height==b.height && surface==b.surface && hasHeight==b.hasHeight &&
			hflip==b.hflip && vflip==b.vflip && ambiguous==b.ambiguous;
	}
};
// Only values consumed by the visual graph. In particular this is NOT a
// physical epoch (which also changes with actor support/plane). Compared in
// the existing complete placement recognition scan, without hashing memory or
// retaining profile/frame/diagnostic pointers.
struct RemasterVisibleFaceSupportInput {
	std::array<int,2> owner={},base={},floorId={},floorBase={};
	std::array<uint8_t,2> collision={},flags={};
	bool operator==(const RemasterVisibleFaceSupportInput &b)const {
		return owner==b.owner && base==b.base && floorId==b.floorId &&
			floorBase==b.floorBase && collision==b.collision && flags==b.flags;
	}
};
struct RemasterVisibleFaceSolvedCell {
	uint64_t mask=UINT64_MAX;
	int base=0,component=-1;
	RemasterVisualClaimStatus status=RemasterVisualClaimStatus::Unsupported;
	bool frame=false;
};
struct RemasterVisibleFaceSolutionCache {
	// One entry, no per-room growth. All arrays are value owned. Exposure is
	// compared after the already-required current raster/source validation.
	bool valid=false;
	uint16_t room=0xffff;
	uint64_t romIdentity=0;
	std::array<std::array<RemasterDungeonFloorContext::Placement,4096>,2> placements;
	std::array<std::array<RemasterVisibleFaceSupportInput,4096>,2> support;
	std::array<std::array<uint64_t,4096>,2> exposed={},sheets={};
	std::array<std::array<int,4096>,2> exposureBase={};
	std::array<std::array<RemasterVisibleFaceSolvedCell,4096>,2> cells;
};
// Uncached callers already allocate the larger candidate workspace. Its lifetime
// ends before solved-cell projection; reuse that storage, not another room array.
union RemasterVisibleFaceWorkspace {
	std::array<std::array<RemasterVisibleFaceCandidate,4096>,2> candidates;
	std::array<std::array<RemasterVisibleFaceSolvedCell,4096>,2> cells;
	RemasterVisibleFaceWorkspace():candidates{} {}
	~RemasterVisibleFaceWorkspace() {}
};
struct RemasterVisibleFaceTemplateCache {
	bool valid=false;
	std::array<std::array<uint16_t,4096>,2> words={};
	std::array<uint8_t,12876> rom={};
	std::array<S9xRemasterVisibleFace::RoomSections,2> sections;
	// Canonical 8192-placement local recognition only. Exposure, occupied
	// owners, collision, graph contacts and solved bases remain frame-local.
	bool modelValid=false;
	std::array<std::array<RemasterVisibleFaceLocalInput,4096>,2> local;
	std::array<std::array<RemasterVisibleFaceCandidate,4096>,2> candidates;
	size_t modelBuilds=0,modelHits=0;
	std::unique_ptr<RemasterVisibleFaceSolutionCache> solved;
	size_t preparationHits=0,preparationMisses=0,graphBuilds=0,oobBuilds=0,solutionAllocations=0;
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	// Last complete solve, diagnostic only (never reused as input/ownership).
	struct CellResult { int base=INT_MIN,owner=-1,plane=-1; uint8_t status=0; int claimRoot=-1; };
	struct ContactClaim {
		S9xRemasterVisibleFace::Contact edge;
		uint8_t rawSamples=0;
		// 0 admitted, 1 no shared facing, 2 frame floorward (datum-only),
		// 3 different corner family, 4 no common face mask, 5 clipped ceiling.
		uint8_t reason=0;
		bool admitted=false;
	};
	std::vector<ContactClaim> diagnosticClaims;
	std::vector<std::array<int,3>> diagnosticSectionUnions;
	std::array<std::array<CellResult,4096>,2> result;
	std::vector<S9xRemasterVisibleFace::Contact> diagnosticContacts;
	std::vector<std::pair<int,int>> diagnosticCells;
	std::vector<S9xRemasterVisibleFace::FloorContact> diagnosticFloors;
	std::vector<S9xRemasterVisibleFace::DatumContact> diagnosticDatums;
	std::vector<std::array<int,4>> diagnosticOwners;
	std::vector<S9xRemasterVisibleFace::PlateauEvent> diagnosticPlateaus;
	// bg, cell, sheet bg/cell, direction, reason, node, graph status/base,
	// owner guard, owner, plane, physical base/planes, wall/sheet exposure.
	// Original 19 fields followed by numeric edge min/max/count, canonical band
	// mask, downhill bits, and all eight transformed absolute edge samples.
	std::vector<std::array<int64_t,32>> diagnosticTerminals;
	S9xRemasterVisibleFace::EquationTrace diagnosticEquations;
	// bg/cell pairs, direction, admitted; actual observed coverage of both
	// facing edges, transformed flips, ambiguous/source channel presence.
	std::vector<std::array<uint64_t,15>> diagnosticSheetJoins;
#endif
};
// Value-owned invocation-local subset. Annotation grows tileInstances, so even
// placement-height sources must not retain pointers into that vector.
struct RemasterVisibleFaceSource {
	std::array<int16_t,64> height={};
	RemasterTileRole tileRole=RemasterTileRole::Unknown;
	bool hasHeight=false,hasNormals=false;
};
inline std::vector<RemasterVisibleFaceSource> S9xRemasterVisibleFaceSources(const RemasterFrame &frame)
{
	std::map<RemasterTileContentId,const RemasterFrameAssetMetadata *> assets;
	for(const auto &m:frame.assetMetadata)assets[m.tileId]=&m;
	std::vector<RemasterVisibleFaceSource> result;result.reserve(frame.tileInstances.size());
	for(size_t n=0;n<frame.tileInstances.size();n++) {
		const auto &i=frame.tileInstances[n];RemasterVisibleFaceSource s;
		auto found=assets.find(i.tileId);
		if(found!=assets.end()) {
			const auto &m=*found->second;s={m.height,m.tileRole,m.hasHeight,m.hasNormals};
			const RemasterGeometryLayers *g=nullptr;
			if(m.geometryByFlip)g=&m.flipGeometry[S9xRemasterGeometryFlipIndex(i.hFlip,i.vFlip)];
			else if(m.geometryByFacing && i.geometryFacing>=1 && i.geometryFacing<=4)g=&m.facingGeometry[i.geometryFacing-1];
			if(g){if(g->hasHeight){s.height=g->height;s.hasHeight=true;}if(g->hasNormals)s.hasNormals=true;}
		}
		s.tileRole=S9xRemasterResolveInstanceTileRole(s.tileRole,i);
		if(!S9xRemasterInstanceGeometryUnresolved(i)) {
			if(i.hasPlacementHeight){s.height=i.placementHeight;s.hasHeight=true;}
			if(i.hasPlacementNormals)s.hasNormals=true;
		}
		auto context=frame.contextGeometry.find(uint32_t(n+1));
		if(context!=frame.contextGeometry.end()) {
			s.height=context->second.height;s.hasHeight=context->second.hasHeight;s.hasNormals=context->second.hasNormals;
		}
		if(S9xRemasterInstanceGeometryUnresolved(i)) {
			s.hasHeight=false;if(i.source!=RemasterSourceType::Object)s.hasNormals=false;
		}
		result.push_back(s);
	}
	return result;
}
struct RemasterVisibleFaceCloneKey {
	uint32_t instance;int cell,base;bool publish;
	bool operator==(const RemasterVisibleFaceCloneKey &b)const {
		return instance==b.instance && cell==b.cell && base==b.base && publish==b.publish;
	}
};
struct RemasterVisibleFaceCloneHash {
	size_t operator()(const RemasterVisibleFaceCloneKey &k)const {
		uint64_t h=k.instance;
		for(uint32_t v:{uint32_t(k.cell),uint32_t(k.base),uint32_t(k.publish)})h=(h^v)*UINT64_C(1099511628211);
		return size_t(h^(h>>32));
	}
};
// A complete-room visual solve. Physical solves are immutable inputs; raster
// exposure selects publication only, never graph reachability or floor seeds.
inline void S9xRemasterApplyVisibleDungeonFaces(RemasterFrame &frame,
	const RemasterDungeonFloorContext &c,
	const RemasterDungeonFloorContext::Placement *bg1,
	const RemasterDungeonFloorContext::Placement *bg2,
	const RemasterDungeonHeightMap &map1,const RemasterDungeonHeightMap &map2,
	RemasterVisibleFaceTemplateCache *cache=nullptr)
{
	using namespace S9xRemasterVisibleFace;
	using Surface=RemasterDungeonFloorContext::Surface;
	const bool currentStairs=c.module==7 && (c.submodule==8 || c.submodule==16) && c.constructionPhase<=1;
	if(!c.verifiedAlttpRom || !c.indoors || (c.submodule && !currentStairs) || c.collisionMode ||
		!c.constructedMaps || !c.collisionAttributes || !c.dungeonTemplateData ||
		!bg1 || !bg2 || !map1.roomDiagnostics || !map2.roomDiagnostics ||
		map1.roomIndex!=c.roomIndex || map2.roomIndex!=c.roomIndex ||
		frame.scenePixels.size()!=frame.mainPixels.size() || !frame.width)return;
	const RemasterDungeonFloorContext::Placement *tiles[]={bg1,bg2};
	RemasterLightingTrace::Scope recognitionTimer(RemasterLightingTrace::current(),RemasterLightingTrace::Recognition);
	const RemasterDungeonHeightMap *maps[]={&map1,&map2};
	auto wordsStorage=std::make_unique<std::array<std::array<uint16_t,4096>,2>>();auto &words=*wordsStorage;
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		int at=(bg?0:8192)+2*i;words[bg][i]=c.constructedMaps[at]|c.constructedMaps[at+1]<<8;
	}
	// The first scan discovers candidates only. No owner is fabricated from an
	// artwork match. A second scan below uses the independently derived owner.
	const bool templatesChanged=cache && (!cache->valid || cache->words!=words || std::memcmp(cache->rom.data(),c.dungeonTemplateData,12876));
	if(templatesChanged) {
		cache->modelValid=false;
		cache->words=words;std::copy_n(c.dungeonTemplateData,12876,cache->rom.begin());
		cache->sections=collectRoomSections(words[0].data(),words[1].data(),c.dungeonTemplateData,12876,
			[](int,const TemplateSection &,const std::array<int,6>&){return TemplateSection::Equivalence{};});cache->valid=true;
	}
	auto sections=cache?cache->sections:collectRoomSections(words[0].data(),words[1].data(),c.dungeonTemplateData,12876,
		[](int,const TemplateSection &,const std::array<int,6>&){return TemplateSection::Equivalence{};});
	// Exact local dependency comparison; no profile digest and no camera key.
	// Producer hash/word changes already force template rescan above; hash-only
	// source edits with equal local relief do not change these candidate slopes.
	bool reuse=cache && cache->modelValid;
	if(cache && !cache->solved){cache->solved=std::make_unique<RemasterVisibleFaceSolutionCache>();++cache->solutionAllocations;}
	auto *saved=cache?cache->solved.get():nullptr;
	bool solutionReuse=saved && saved->valid && !templatesChanged && saved->room==c.roomIndex && saved->romIdentity==c.romIdentity;
	// This extends, rather than adds, the required local-source scan. Actor
	// plane is deliberately absent: BOTH physical planes are compared by value.
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		const auto &t=tiles[bg][i];
		const RemasterVisibleFaceLocalInput now{t.height,t.surface,t.hasHeight,t.hflip,t.vflip,t.stairSourceAmbiguous};
		if(reuse && !(cache->local[bg][i]==now))reuse=false;
		if(saved) {
			RemasterVisibleFaceSupportInput support;
			for(int p=0;p<2;p++) {
				const auto &d=maps[bg]->roomDiagnostics->cells[p*4096+i];
				support.owner[p]=maps[bg]->occupiedFloorOwners[p][i];support.base[p]=maps[bg]->planeOffsets[p][i];
				support.floorId[p]=d.floorId;support.floorBase[p]=d.floorBase;
				support.flags[p]=d.wall | d.heightConflict<<1 | d.planeObserved<<2;
				support.collision[p]=c.collisionAttributes[p*4096+i];
			}
			const auto &old=saved->placements[bg][i];
			if(solutionReuse && (!old.sameGeometry(t) || old.tileHash!=t.tileHash || old.stairSourceAmbiguous!=t.stairSourceAmbiguous ||
				!(saved->support[bg][i]==support)))solutionReuse=false;
			saved->placements[bg][i]=t;saved->support[bg][i]=support;
		}
	}
	auto candidateStorage=cache?nullptr:std::make_unique<RemasterVisibleFaceWorkspace>();
	auto &candidates=cache?cache->candidates:candidateStorage->candidates;
	if(!reuse) {
	for(auto &bg:candidates)for(auto &v:bg)v={};
	auto slopeStorage=std::make_unique<std::array<std::array<std::array<int8_t,2>,4096>,2>>();auto &slopes=*slopeStorage;
	for(auto &bg:slopes)for(auto &cell:bg)cell.fill(2); // not evaluated this invocation
	for(int bg=0;bg<2;bg++)for(size_t n=0;n<sections[bg].sections.size();n++) {
		const auto &s=sections[bg].sections[n];const int w=s.axis?4:2,h=s.axis?2:4;
		std::array<int,6> cells={};int z=0;
		for(int t=0;t<2;t++)for(int d=s.reverse;d<s.reverse+3;d++) {
			int x=s.axis?d:t,y=s.axis?t:d;
			cells[z++]=(s.y+(s.flip&2?h-1-y:y))*64+s.x+(s.flip&1?w-1-x:x);
		}
		std::sort(cells.begin(),cells.end());
		int down=0;bool valid=true;
		for(int cell:cells) {
			const auto &t=tiles[bg][cell];
			if(t.surface!=Surface::Wall || !t.hasHeight || t.stairSourceAmbiguous) {valid=false;break;}
			auto &slope=slopes[bg][cell][s.axis];
			if(slope==2) {
				int direction=0;bool good=true;
				auto sample=[&](int x,int y){return t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];};
				for(int y=0;y<8;y++)for(int x=0;x<8;x++)if((s.axis?x:y)<7) {
					int d=sample(x+(s.axis?1:0),y+(s.axis?0:1))-sample(x,y);
					if(std::abs(d)>1 || (d && direction && direction!=-d))good=false;else if(d)direction=-d;
				}
				slope=good?direction:3;
			}
			if(slope==3 || (slope && down && down!=slope))valid=false;else if(slope)down=slope;
		}
		if(!valid || !down)continue;
		int family=s.source<0x300?1:2;
		for(int cell:cells) {
			auto &v=candidates[bg][cell];
			if(v.section<0 && !v.ambiguous)v={int(n),s.axis,down,family,cells,false};
			else if(!v.ambiguous && (v.axis!=s.axis || v.down!=down || v.family!=family || v.footprint!=cells))
				{v.section=-1;v.ambiguous=true;}
		}
	}
	if(cache) {
		for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
			const auto &t=tiles[bg][i];cache->local[bg][i]={t.height,t.surface,t.hasHeight,t.hflip,t.vflip,t.stairSourceAmbiguous};
		}
		cache->modelValid=true;++cache->modelBuilds;
	}
	} else ++cache->modelHits;
	if(auto *trace=RemasterLightingTrace::current()) {
		if(reuse)trace->candidateHits++;else trace->candidateBuilds++;
	}
	// Exact selected source and transformed local source at every observed pixel.
	recognitionTimer.stop();
	RemasterFrameHeightResolver resolver(frame);
	auto exposureStorage=std::make_unique<std::array<std::array<uint64_t,4096>,2>>();auto &exposed=*exposureStorage;
	auto exposureBaseStorage=std::make_unique<std::array<std::array<int,4096>,2>>();auto &exposureBase=*exposureBaseStorage;
	// Source equality is placement/occurrence-local, not pixel-local. Recheck
	// once for each actual occurrence of a cell this invocation. Nothing survives
	// the frame: profile, graphics, context layers, exposure and support changes
	// cannot reuse a prior visual datum.
	auto checkedStorage=std::make_unique<std::array<std::array<uint32_t,4096>,2>>();auto &checked=*checkedStorage;
	auto matchesStorage=std::make_unique<std::array<std::array<bool,4096>,2>>();auto &matches=*matchesStorage;
	for(auto &a:exposureBase)a.fill(INT_MIN);
	std::vector<RemasterFramePixel> selected(frame.mainPixels.size());
	for(size_t at=0;at<selected.size();at++) {
		const auto choice=resolver.selectHeight(frame,at,true);
		const auto px=choice.donors[size_t(RemasterMetadataChannel::Height)];
		if(!px.instanceId || px.instanceId>frame.tileInstances.size() || px.tilePixel>=64)continue;
		const auto &inst=frame.tileInstances[px.instanceId-1];
		if(inst.source!=RemasterSourceType::Background || inst.sourceIndex>1)continue;
		const int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511,cell=y/8*64+x/8,bg=inst.sourceIndex;
		const auto &t=tiles[bg][cell]; const auto word=words[bg][cell];
		if(checked[bg][cell]!=px.instanceId) {
			checked[bg][cell]=px.instanceId;const auto &effective=resolver.effective[px.instanceId-1];
			matches[bg][cell]=inst.tileId.hash==t.tileHash && inst.hFlip==t.hflip && inst.vFlip==t.vflip &&
				inst.tileNumber==(word&1023) && inst.palette==((word>>10)&7) && effective.hasHeight && *effective.height==t.height;
		}
		if(!matches[bg][cell] || px.tilePixel!=((inst.vFlip?7-y%8:y%8)*8+(inst.hFlip?7-x%8:x%8)))continue;
		selected[at]=px;exposed[bg][cell]|=UINT64_C(1)<<((y%8)*8+x%8);
		int &base=exposureBase[bg][cell];
		if(base==INT_MIN)base=inst.heightOffset;else if(base!=inst.heightOffset)base=INT_MAX;
	}
	const bool hasPlateaus=std::any_of(bg1,bg1+4096,[](const auto &t){return t.surface==Surface::OutOfBounds;}) ||
		std::any_of(bg2,bg2+4096,[](const auto &t){return t.surface==Surface::OutOfBounds;});
	const auto plateauSources=hasPlateaus?S9xRemasterVisibleFaceSources(frame):std::vector<RemasterVisibleFaceSource>{};
	std::vector<RemasterFramePixel> visible(hasPlateaus?selected.size():0);
	auto sheetExposure=std::make_unique<std::array<std::array<uint64_t,4096>,2>>();
	for(size_t at=0;at<visible.size();at++) {
		const auto px=visible[at]=S9xRemasterSelectVisibleOccurrence(frame,at,true,resolver.alttp);
		if(!px.instanceId || px.instanceId>frame.tileInstances.size() || px.tilePixel>=64)continue;
		const auto &inst=frame.tileInstances[px.instanceId-1];if(inst.source!=RemasterSourceType::Background || inst.sourceIndex>1)continue;
		int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511,cell=y/8*64+x/8,bg=inst.sourceIndex;
		const auto &t=tiles[bg][cell];const auto word=words[bg][cell];
		const auto &m=plateauSources[px.instanceId-1];
		if(t.surface!=Surface::OutOfBounds || m.tileRole!=RemasterTileRole::OutOfBounds || t.stairSourceAmbiguous ||
			inst.tileId.hash!=t.tileHash || inst.hFlip!=t.hflip || inst.vFlip!=t.vflip || inst.tileNumber!=(word&1023) || inst.palette!=((word>>10)&7) ||
			m.hasHeight!=t.hasHeight || (m.hasHeight && m.height!=t.height) || frame.contextGeometry.count(px.instanceId) ||
			px.tilePixel!=((inst.vFlip?7-y%8:y%8)*8+(inst.hFlip?7-x%8:x%8)))continue;
		(*sheetExposure)[bg][cell]|=UINT64_C(1)<<((y%8)*8+x%8);
	}
	solutionReuse=solutionReuse && saved->exposed==exposed && saved->exposureBase==exposureBase && saved->sheets==*sheetExposure;
	auto &solvedCells=saved?saved->cells:candidateStorage->cells;
	if(solutionReuse) {++cache->preparationHits;} else {
	if(cache){++cache->preparationMisses;++cache->graphBuilds;++cache->oobBuilds;}
	auto neighbor=[](int cell,int dx,int dy){int x=cell%64+dx,y=cell/64+dy;return x<0||x>=64||y<0||y>=64?-1:y*64+x;};
	std::vector<Region> regions;std::vector<Contact> contacts;std::vector<FloorContact> floorContacts;
	std::vector<WallBand> bands;std::vector<DatumContact> datums;
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	if(cache){cache->diagnosticClaims.clear();cache->diagnosticSectionUnions.clear();}
#endif
	auto claim=[&](Contact edge,uint8_t raw,uint8_t reason) {
		bool admitted=edge.samples && (reason==0 || reason==5);
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
		if(cache)cache->diagnosticClaims.push_back({edge,raw,reason,admitted});
#endif
		if(admitted)contacts.push_back(edge);
	};
	auto nodeStorage=std::make_unique<std::array<std::array<int,4096>,2>>();auto &nodes=*nodeStorage;for(auto &a:nodes)a.fill(-1);
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		const auto &t=tiles[bg][i];const auto &a=candidates[bg][i];
		if(a.ambiguous)continue;
		auto band=wallBand(t,i,a.axis,a.down,a.section>=0);
		if(!band.downhill)continue;
		nodes[bg][i]=int(regions.size());regions.push_back({t.height,t.hflip,t.vflip,true});bands.push_back(band);
	}
	struct Anchor {int node,plane,owner,cell;};std::vector<Anchor> anchors;
	// Physical owner IDs are BG-local namespaces. Only identical complete
	// occupied footprints on the SAME plane, with identical physical bases,
	// attest that two namespaces describe the same support. Intrinsic art is
	// checked independently at each zero-local contact, not across hidden art.
	std::map<std::array<int,3>,bool> ownerAliases;
	auto sameOccupiedFloor=[&](int p,int a,int b) {
		const std::array<int,3> key={p,a,b};auto found=ownerAliases.find(key);
		if(found!=ownerAliases.end())return found->second;
		bool same=a>=0 && b>=0;
		for(int k=0;same && k<4096;k++) {
			bool x=map1.occupiedFloorOwners[p][k]==a,y=map2.occupiedFloorOwners[p][k]==b;
			if(x!=y){same=false;break;}if(!x)continue;
			if(map1.planeOffsets[p][k]!=map2.planeOffsets[p][k]){same=false;break;}
		}
		ownerAliases[key]=same;return same;
	};
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++)if(nodes[bg][i]>=0) {
		const int node=nodes[bg][i];const auto &a=bands[node];
		static const int ddx[]={0,0,-1,1},ddy[]={-1,1,0,0};
		for(int dir=0;dir<4;dir++)if(a.downhill&(1<<dir)) {
		const int dx=ddx[dir],dy=ddy[dir],j=neighbor(i,dx,dy);
		if(j<0)continue;
		for(int other=0;other<2;other++) {
			if(nodes[other][j]>=0) {
				const auto &b=bands[nodes[other][j]];
				auto mask=wallEdgeSamples(a,regions[node],b,regions[nodes[other][j]],dx,dy);
				if((a.downhill&b.downhill&(1<<dir)) && (!a.frame && !b.frame)) {
					claim({node,nodes[other][j],dx,dy,-1,false,mask},mask,mask?0:4);
				} else claim({node,nodes[other][j],dx,dy,-1,false,mask},mask,(a.frame || b.frame)?2:1);
				continue;
			}
			// A floor usage under a continuing wall in the peer map is not an
			// exposed wall foot. Keep that occupied underpass independent.
			if(nodes[1-other][j]>=0)continue;
			const auto &t=tiles[other][j];if(!t.hasHeight || t.surface==Surface::OutOfBounds || t.surface==Surface::Wall || t.surface==Surface::Stair ||
				t.surface==Surface::Door || t.surface==Surface::ClosedDoor || t.surface==Surface::Obstacle ||
				t.obstacleAssembly || t.authoredObstacle || t.removableSupportPlanes || t.doorAssembly)continue;
			// Independently occupied complete-room support is sufficient even
			// offscreen. If observed, a different contextual base is contradictory
			// evidence, not permission to replace that occurrence's geometry.
			for(int p=0;p<2;p++) {
				const auto &d=maps[other]->roomDiagnostics->cells[p*4096+j];
				const int owner=maps[other]->occupiedFloorOwners[p][j],base=maps[other]->planeOffsets[p][j];
				if(owner<0 || base==255 || (exposed[other][j] && exposureBase[other][j]!=base) || c.collisionAttributes[p*4096+j]!=0 || d.wall || d.heightConflict || !d.planeObserved ||
					d.floorId!=owner || d.floorBase!=base)continue;
				int floor=INT_MIN;bool flat=true;
				for(int k=0;k<8;k++) {
					int x=dx?(dx>0?0:7):k,y=dy?(dy>0?0:7):k;
					int value=base+t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];
					if(floor!=INT_MIN && floor!=value)flat=false;floor=value;
				}
				if(!flat || floor!=base)continue;
				int canonicalOwner=other?16384+owner:owner;
				if(other==1 && sameOccupiedFloor(p,map1.occupiedFloorOwners[p][j],owner))canonicalOwner=map1.occupiedFloorOwners[p][j];
				anchors.push_back({nodes[bg][i],p,canonicalOwner,j});
				for(int k=0;k<8;k++) {
					int x=dx?(dx>0?7:0):k,y=dy?(dy>0?7:0):k;
					const auto &r=regions[node];int local=(r.vflip?7-y:y)*8+(r.hflip?7-x:x);
					if(a.mask&(UINT64_C(1)<<local))floorContacts.push_back({node,x,y,floor});
				}
			}
		}
		// Same-band tangent contacts use signed edge differences, not equal
		// offsets. This carries a floor datum sideways with no support below
		// each intermediate member, including different local relief origins.
		for(int sign:{-1,1}) {
			int tx=dy?sign:0,ty=dx?sign:0,k=neighbor(i,tx,ty);
			if(k<0 || nodes[bg][k]<0)continue;
			const auto &b=bands[nodes[bg][k]];
			auto raw=wallEdgeSamples(a,regions[node],b,regions[nodes[bg][k]],tx,ty);
			if((a.downhill&b.downhill&(1<<dir)) && !(a.corner && b.corner && a.downhill!=b.downhill &&
				tiles[bg][i].wallTemplateOffset!=tiles[bg][k].wallTemplateOffset)) {
				auto mask=wallEdgeSamples(a,regions[node],b,regions[nodes[bg][k]],tx,ty);
				// A clipped corner's ceiling plateau is not a tangent face.
				// Exact same corner assembly family plus two authored facings
				// identifies the face samples; never change its intrinsic cap.
				if(a.corner && b.corner && a.downhill!=b.downhill) {
					mask=cornerFaceSamples(regions[node],regions[nodes[bg][k]],tx,ty,mask);
				}
				if((a.corner && b.frame) || (a.frame && b.corner)) {
					auto af=a,bf=b;
					if(a.corner)af.mask&=highCornerFaceMask(tiles[bg],i,dir);
					if(b.corner)bf.mask&=highCornerFaceMask(tiles[bg],k,dir);
					mask&=wallEdgeSamples(af,regions[node],bf,regions[nodes[bg][k]],tx,ty);
				}
				claim({node,nodes[bg][k],tx,ty,0,false,mask},raw,!mask?4:mask!=raw?5:0);
			} else claim({node,nodes[bg][k],tx,ty,0,false,raw},raw,(a.downhill&b.downhill&(1<<dir))?3:1);
		}
		}
	}
	// A complete exact door assembly carries one signed band datum across
	// disconnected jamb/cap masks, including closed leaves. No interior floor.
	std::map<std::pair<int,int>,int> frameRoots;
	for(size_t n=0;n<bands.size();n++)if(bands[n].frame) {
		const auto key=std::make_pair(bands[n].assembly,bands[n].direction);
		auto it=frameRoots.emplace(key,int(n));
		if(!it.second)datums.push_back({it.first->second,int(n),bands[n].phase-bands[it.first->second].phase});
	}
	// Distinct occupied owners (including planes) remain ambiguous even if they
	// happen to have equal bases. Component membership uses only admitted seams.
	std::vector<int> parent(regions.size());for(size_t i=0;i<parent.size();i++)parent[i]=int(i);
	auto root=[&](int n){while(parent[n]!=n)n=parent[n];return n;};
	for(const auto &e:contacts){int a=root(e.a),b=root(e.b);if(a!=b)parent[b]=a;}
	for(const auto &e:datums){int a=root(e.a),b=root(e.b);if(a!=b)parent[b]=a;}
	// The two tangent rows of a complete surviving ROM section are one
	// ownership claim, not two independently publishable one-row surfaces.
	for(int bg=0;bg<2;bg++)for(const auto &s:sections[bg].sections) {
		int first=-1,w=s.axis?4:2,h=s.axis?2:4;
		for(int t=0;t<2;t++)for(int d=s.reverse;d<s.reverse+3;d++) {
			int x=s.axis?d:t,y=s.axis?t:d,cell=(s.y+(s.flip&2?h-1-y:y))*64+s.x+(s.flip&1?w-1-x:x);
			int n=nodes[bg][cell];if(n<0)continue;
			if(first<0)first=n;else {
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
				if(cache)cache->diagnosticSectionUnions.push_back({first,n,int(s.source)});
#endif
				int a=root(first),b=root(n);if(a!=b)parent[b]=a;
			}
		}
	}
	std::vector<int> owner(regions.size(),-1),plane(regions.size(),-1);std::vector<bool> conflict(regions.size()),crestPlaneConflict(regions.size());
	for(const auto &a:anchors){int r=root(a.node);
		if(plane[r]>=0 && plane[r]!=a.plane)crestPlaneConflict[r]=true;
		if(owner[r]>=0 && (owner[r]!=a.owner || plane[r]!=a.plane))conflict[r]=true;else {owner[r]=a.owner;plane[r]=a.plane;}}
	// Preserve the visual wall publication/ownership contract above. An OOB
	// terminal has a different role: each floor contact was independently proved
	// occupied, plane-observed and zero-local before entering the signed graph.
	// Distinct floor IDs alone cannot invalidate that graph's consistent datum.
	// Never use physical/render fallback or minimize inconsistent wall equations.
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	if(cache)equationTrace=&cache->diagnosticEquations;
#endif
	const auto solution=solve(regions,contacts,floorContacts,datums);
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	equationTrace=nullptr;
#endif
	auto edge=[](int direction){uint64_t mask=0;for(int k=0;k<8;k++) {
		int x=direction==2?0:direction==3?7:k,y=direction==0?0:direction==1?7:k;mask|=UINT64_C(1)<<(y*8+x);
	}return mask;};
	#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	if(cache){cache->diagnosticPlateaus.clear();cache->diagnosticTerminals.clear();cache->diagnosticSheetJoins.clear();plateauEvents=&cache->diagnosticPlateaus;}
	#endif
	const auto plateaus=outOfBoundsPlateaus(tiles,[&](int bg,int cell,int direction,int &height,int &o,int &p,int sheetBg,int sheetCell,int &maximum,bool exact,int &datum) {
		auto resolve=[&]() {
		int n=nodes[bg][cell];if(n<0)return RemasterVisualClaimStatus::NotEvaluated;
		const auto &b=bands[n];
		// Only the uphill terminal edge of a proved wall/frame is a crest.
		if(!(b.downhill&(1<<(direction^1))))return RemasterVisualClaimStatus::NotEvaluated;
		// Hidden peer-BG walls are not contacts. Partial/unobserved edges cannot
		// certify an OOB crest in this bounded visible-surface feature.
		if((exposed[bg][cell]&edge(direction))!=edge(direction))return RemasterVisualClaimStatus::NotEvaluated;
		if(((*sheetExposure)[sheetBg][sheetCell]&edge(direction^1))!=edge(direction^1))return RemasterVisualClaimStatus::Unsupported;
		int rt=root(n);if(crestPlaneConflict[rt] || solution.status[n]==Status::Conflict)return RemasterVisualClaimStatus::Conflict;
		if(solution.status[n]!=Status::Solved || owner[rt]<0)return RemasterVisualClaimStatus::Unanchored;
		int value=INT_MAX,high=INT_MIN;bool any=false,hasDatum=false;
		for(int k=0;k<8;k++) {
			int x=direction==2?0:direction==3?7:k,y=direction==0?0:direction==1?7:k;
			const auto &r=regions[n];int local=(r.vflip?7-y:y)*8+(r.hflip?7-x:x);
			// A masked fragment cannot certify the complete shared crest edge.
			if(!(b.mask&(UINT64_C(1)<<local)))return RemasterVisualClaimStatus::Unsupported;
			int h=solution.base[n]+r.height[local];
			if(exact) {
				const auto &sheet=tiles[sheetBg][sheetCell];
				int sx=direction>=2?7-x:x,sy=direction<2?7-y:y;
				int sh=sheet.hasHeight?sheet.height[(sheet.vflip?7-sy:sy)*8+(sheet.hflip?7-sx:sx)]:0;
				int vote=h-sh;
				if(hasDatum && datum!=vote)return RemasterVisualClaimStatus::Conflict;
				datum=vote;hasDatum=true;
			}
			// Only independently validated uphill band samples reach this point.
			// Art reuse may legitimately vary within an edge as well as between
			// walls. Reduce absolute values, never alter the signed wall solve.
			any=true;value=std::min(value,h);high=std::max(high,h);
		}
		if(!any)return RemasterVisualClaimStatus::Unsupported;
		height=value;maximum=high;o=owner[rt];p=plane[rt];return RemasterVisualClaimStatus::Solved;
		};
		auto status=resolve();
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
		if(cache && nodes[bg][cell]>=0) {
		 int n=nodes[bg][cell],rt=root(n);const auto &b=bands[n];
		 // Numeric candidate is evidence even when a separate owner guard vetoes
		 // admission. It is never promoted into the callback's output values.
		 int candidate=INT_MIN,edgeMin=INT_MAX,edgeMax=INT_MIN,edgeCount=0;
		 std::array<int64_t,8> edgeValues;edgeValues.fill(INT_MIN);
		 if(solution.status[n]==Status::Solved)for(int k=0;k<8;k++) {
		  int x=direction==2?0:direction==3?7:k,y=direction==0?0:direction==1?7:k;
		  const auto &r=regions[n];int local=(r.vflip?7-y:y)*8+(r.hflip?7-x:x);
		  if(b.mask&(UINT64_C(1)<<local)){int h=solution.base[n]+r.height[local];edgeValues[k]=h;
		   edgeMin=std::min(edgeMin,h);edgeMax=std::max(edgeMax,h);++edgeCount;
		   if(candidate==INT_MIN)candidate=h;else if(candidate!=h)candidate=INT_MAX;}
		 }
		 int reason=!(b.downhill&(1<<(direction^1)))?1:
		  (exposed[bg][cell]&edge(direction))!=edge(direction)?2:
		  ((*sheetExposure)[sheetBg][sheetCell]&edge(direction^1))!=edge(direction^1)?3:
		  crestPlaneConflict[rt]?4:solution.status[n]==Status::Conflict?5:solution.status[n]!=Status::Solved || owner[rt]<0?6:
		  status==RemasterVisualClaimStatus::Conflict?7:status==RemasterVisualClaimStatus::Unsupported?8:0;
		 std::array<int64_t,32> terminal={bg,cell,sheetBg,sheetCell,direction,reason,n,int(solution.status[n]),solution.base[n],conflict[rt],owner[rt],plane[rt],maps[bg]->offsets[cell],maps[bg]->structuralPlaneOffsets[0][cell],maps[bg]->structuralPlaneOffsets[1][cell],int64_t(exposed[bg][cell]),int64_t((*sheetExposure)[sheetBg][sheetCell]),candidate,int(status),edgeMin,edgeMax,edgeCount,int64_t(b.mask),b.downhill};
		 std::copy(edgeValues.begin(),edgeValues.end(),terminal.begin()+24);
		 cache->diagnosticTerminals.push_back(terminal);
		}
#endif
		return status;
	},[&](int a,int i,int b,int j,int d) {
		// An exact pair of visible opaque edges proves continuation, even across
		// BGs. No hash/coordinate-only flood and no hidden overlapping bridge.
		const bool admitted=((*sheetExposure)[a][i]&edge(d))==edge(d) && ((*sheetExposure)[b][j]&edge(d^1))==edge(d^1);
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
		if(cache)cache->diagnosticSheetJoins.push_back({uint64_t(a),uint64_t(i),uint64_t(b),uint64_t(j),uint64_t(d),uint64_t(admitted),(*sheetExposure)[a][i]&edge(d),(*sheetExposure)[b][j]&edge(d^1),edge(d),edge(d^1),uint64_t(tiles[a][i].hflip|tiles[a][i].vflip<<1),uint64_t(tiles[b][j].hflip|tiles[b][j].vflip<<1),uint64_t(tiles[a][i].stairSourceAmbiguous|tiles[b][j].stairSourceAmbiguous<<1),uint64_t(tiles[a][i].hasHeight),uint64_t(tiles[b][j].hasHeight)});
#endif
		return admitted;
	});
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	plateauEvents=nullptr;
#endif
	// Role-only sheets have no height donor until solved; use actual visible
	// composition, never the lower layer's fallback height, to select publication.
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
	if(cache) {
		cache->diagnosticOwners.clear();for(const auto &a:anchors)cache->diagnosticOwners.push_back({a.node,a.plane,a.owner,a.cell});
		cache->diagnosticContacts=contacts;cache->diagnosticFloors=floorContacts;cache->diagnosticDatums=datums;cache->diagnosticCells.assign(regions.size(),{-1,-1});
		for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++)if(nodes[bg][i]>=0)cache->diagnosticCells[nodes[bg][i]]={bg,i};
	}
	if(cache)for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		auto &r=cache->result[bg][i];r={};int n=nodes[bg][i];if(n<0)continue;
		int rt=root(n);r.claimRoot=rt;r.owner=owner[rt];r.plane=plane[rt];
		r.status=conflict[rt] || solution.status[n]==Status::Conflict?3:
			solution.status[n]==Status::Solved && owner[rt]>=0?1:2;
		if(r.status==1)r.base=solution.base[n];
	}
#endif
	if(candidateStorage)new (&candidateStorage->cells) std::array<std::array<RemasterVisibleFaceSolvedCell,4096>,2>{};
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		auto &out=solvedCells[bg][i];out={};int n=nodes[bg][i];
		if(tiles[bg][i].surface==Surface::OutOfBounds) {
			const auto &p=plateaus[bg*4096+i];out.status=p.status;out.base=p.base;out.component=p.component;
		} else if(n>=0) {
			out.mask=bands[n].mask;out.frame=bands[n].frame;out.component=root(n);out.base=solution.base[n];
			out.status=conflict[root(n)] || solution.status[n]==Status::Conflict?RemasterVisualClaimStatus::Conflict:
				solution.status[n]==Status::Solved && owner[root(n)]>=0?RemasterVisualClaimStatus::Solved:RemasterVisualClaimStatus::Unanchored;
		}
	}
	if(saved){saved->exposed=exposed;saved->exposureBase=exposureBase;saved->sheets=*sheetExposure;saved->room=c.roomIndex;saved->romIdentity=c.romIdentity;saved->valid=true;}
	} // graph preparation/solve/traversal miss; hits project current occurrences below
	// Change only the actual scene occurrence selected by the shared policy.
	// Clone per placement/base; never mutate physical map arrays or stock pixels.
	// Hashing changes lookup only: insertion/clone IDs still follow the exact
	// scene/main/sub/observed raster first-encounter order below. Never iterate it.
	std::unordered_map<RemasterVisibleFaceCloneKey,uint32_t,RemasterVisibleFaceCloneHash> copies;
	// Exposure's scratch is dead here. Reuse it for an invocation-local,
	// direct-mapped identity memo keyed by BOTH BG/cell and actual occurrence.
	// Different occurrences evict safely; no source pointers survive clone growth.
	for(auto &a:checked)a.fill(0);
	auto annotate=[&](std::vector<RemasterFramePixel> &raster,bool scene) {
	for(size_t at=0;at<raster.size();at++) {
		const auto px=raster[at];if(!px.instanceId || px.instanceId>frame.tileInstances.size() || px.tilePixel>=64)continue;
		const auto &inst=frame.tileInstances[px.instanceId-1];int bg=inst.sourceIndex;
		if(inst.source!=RemasterSourceType::Background || bg>1)continue;
		int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511,cell=y/8*64+x/8;
		const auto &solved=solvedCells[bg][cell];
		const auto &tile=tiles[bg][cell];const auto word=words[bg][cell];
		const bool plateau=tile.surface==Surface::OutOfBounds;
		if(solved.component<0 && tile.surface!=Surface::Wall && !plateau)continue;
		if(checked[bg][cell]!=px.instanceId) {
			checked[bg][cell]=px.instanceId;
#ifdef REMASTER_VISIBLE_FACE_IDENTITY_CHECK
			REMASTER_VISIBLE_FACE_IDENTITY_CHECK();
#endif
			matches[bg][cell]=inst.tileId.hash==tile.tileHash && inst.hFlip==tile.hflip && inst.vFlip==tile.vflip &&
				inst.tileNumber==(word&1023) && inst.palette==((word>>10)&7);
		}
		if(!matches[bg][cell] || px.tilePixel!=((inst.vFlip?7-y%8:y%8)*8+(inst.hFlip?7-x%8:x%8)))continue;
		bool unsupportedSource=false;
		if(plateau) {
			const auto &m=plateauSources[px.instanceId-1];
			if(m.tileRole!=RemasterTileRole::OutOfBounds)continue;
			unsupportedSource=m.hasHeight!=tile.hasHeight || (m.hasHeight && m.height!=tile.height) || frame.contextGeometry.count(px.instanceId);
		}
		const uint64_t mask=solved.mask;
		if(!(mask&(UINT64_C(1)<<px.tilePixel)))continue;
		const auto status=unsupportedSource?RemasterVisualClaimStatus::Unsupported:solved.status;
		// A contextual/actor/transition contribution is not this map's placement
		// base. Do not erase it when selecting the separate visual representative.
		int physical=maps[bg]->offsets[cell]==255?0:maps[bg]->offsets[cell];bool physicalConflict=false;
		if(plateau)physical=0; // explicit role is not the gameplay support beneath this sheet
		if(solved.frame) {
			physical=INT_MIN;
			for(const auto &a:maps[bg]->doorframeAttachments)if(a.cell==cell && a.visible && (a.mask&(UINT64_C(1)<<px.tilePixel))) {
				if(physical!=INT_MIN && physical!=a.base)physicalConflict=true;physical=a.base;
			}
		}
		const auto &choice=plateau?visible[at]:selected[at];
		const bool selectedScene=scene && choice.instanceId==px.instanceId && choice.tilePixel==px.tilePixel;
		const bool publish=selectedScene && status==RemasterVisualClaimStatus::Solved && !physicalConflict && inst.heightOffset==physical &&
			!inst.hasPlacementHeight && !frame.contextGeometry.count(px.instanceId);
		const int base=publish?solved.base:inst.heightOffset;
		const RemasterVisibleFaceCloneKey key{px.instanceId,cell,base,publish};auto it=copies.find(key);
		if(it==copies.end()){
			auto copy=inst;copy.heightOffset=base;copy.visualClaim={status,mask,cell,solved.component,publish};
			if(plateau && publish) {
				const auto &m=plateauSources[px.instanceId-1];
				copy.geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;copy.placementTileRole=RemasterTileRole::OutOfBounds;
				if(!m.hasHeight){copy.hasPlacementHeight=true;copy.placementHeight.fill(0);}
				if(!m.hasNormals){copy.hasPlacementNormals=true;for(int k=0;k<64;k++){copy.placementNormalXyz[k*3]=128;copy.placementNormalXyz[k*3+1]=128;copy.placementNormalXyz[k*3+2]=255;}}
			}
			// A clone retains any contextual layers; numeric publication above never replaces them.
			auto context=frame.contextGeometry.find(px.instanceId);
			if(context!=frame.contextGeometry.end())frame.contextGeometry.emplace(uint32_t(frame.tileInstances.size()+1),context->second);
			auto location=frame.contextLocations.find(px.instanceId);
			if(location!=frame.contextLocations.end())frame.contextLocations.emplace(uint32_t(frame.tileInstances.size()+1),location->second);
			frame.tileInstances.push_back(copy);it=copies.emplace(key,uint32_t(frame.tileInstances.size())).first;
		}
		raster[at].instanceId=it->second;
	}
	};
	// Scene publication first, then evidence-only stock/observed occurrences.
	// Channel presence and stock art/owner are never changed by annotation.
	annotate(frame.scenePixels,true);annotate(frame.mainPixels,false);annotate(frame.subPixels,false);
	for(auto &raster:frame.observedBgPixels)annotate(raster,false);
}
#endif
