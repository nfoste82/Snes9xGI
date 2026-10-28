#ifndef S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H
#define S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H

#include "dungeon_visible_face.h"
#include "dungeon_visible_face_templates.h"
#include "frame.h"
#include "dungeon_wall_band.h"

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
#endif
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
	if(!c.verifiedAlttpRom || !c.indoors || c.submodule || c.collisionMode ||
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
	if(cache && (!cache->valid || cache->words!=words || std::memcmp(cache->rom.data(),c.dungeonTemplateData,12876))) {
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
	if(reuse)for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++) {
		const auto &t=tiles[bg][i];
		const RemasterVisibleFaceLocalInput now{t.height,t.surface,t.hasHeight,t.hflip,t.vflip,t.stairSourceAmbiguous};
		if(!(cache->local[bg][i]==now)){reuse=false;break;}
	}
	auto candidateStorage=cache?nullptr:std::make_unique<std::array<std::array<RemasterVisibleFaceCandidate,4096>,2>>();
	auto &candidates=cache?cache->candidates:*candidateStorage;
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
			const auto &t=tiles[other][j];if(!t.hasHeight || t.surface==Surface::Wall || t.surface==Surface::Stair ||
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
	std::vector<int> owner(regions.size(),-1),plane(regions.size(),-1);std::vector<bool> conflict(regions.size());
	for(const auto &a:anchors){int r=root(a.node);if(owner[r]>=0 && (owner[r]!=a.owner || plane[r]!=a.plane))conflict[r]=true;else {owner[r]=a.owner;plane[r]=a.plane;}}
	const auto solution=solve(regions,contacts,floorContacts,datums);
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
	// Change only the actual scene occurrence selected by the shared policy.
	// Clone per placement/base; never mutate physical map arrays or stock pixels.
	std::map<std::tuple<uint32_t,int,int,bool>,uint32_t> copies;
	auto annotate=[&](std::vector<RemasterFramePixel> &raster,bool scene) {
	for(size_t at=0;at<raster.size();at++) {
		const auto px=raster[at];if(!px.instanceId || px.instanceId>frame.tileInstances.size() || px.tilePixel>=64)continue;
		const auto &inst=frame.tileInstances[px.instanceId-1];int bg=inst.sourceIndex;
		if(inst.source!=RemasterSourceType::Background || bg>1)continue;
		int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511,cell=y/8*64+x/8,n=nodes[bg][cell];
		const auto &tile=tiles[bg][cell];const auto word=words[bg][cell];
		if(inst.tileId.hash!=tile.tileHash || inst.hFlip!=tile.hflip || inst.vFlip!=tile.vflip ||
			inst.tileNumber!=(word&1023) || inst.palette!=((word>>10)&7) ||
			px.tilePixel!=((inst.vFlip?7-y%8:y%8)*8+(inst.hFlip?7-x%8:x%8)))continue;
		if(n<0 && tiles[bg][cell].surface!=Surface::Wall)continue;
		const uint64_t mask=n<0?UINT64_MAX:bands[n].mask;
		if(!(mask&(UINT64_C(1)<<px.tilePixel)))continue;
		const auto status=n<0?RemasterVisualClaimStatus::Unsupported:
			conflict[root(n)] || solution.status[n]==Status::Conflict?RemasterVisualClaimStatus::Conflict:
			solution.status[n]==Status::Solved && owner[root(n)]>=0?RemasterVisualClaimStatus::Solved:RemasterVisualClaimStatus::Unanchored;
		// A contextual/actor/transition contribution is not this map's placement
		// base. Do not erase it when selecting the separate visual representative.
		int physical=maps[bg]->offsets[cell]==255?0:maps[bg]->offsets[cell];bool physicalConflict=false;
		if(n>=0 && bands[n].frame) {
			physical=INT_MIN;
			for(const auto &a:maps[bg]->doorframeAttachments)if(a.cell==cell && a.visible && (a.mask&(UINT64_C(1)<<px.tilePixel))) {
				if(physical!=INT_MIN && physical!=a.base)physicalConflict=true;physical=a.base;
			}
		}
		const bool selectedScene=scene && at<selected.size() && selected[at].instanceId==px.instanceId && selected[at].tilePixel==px.tilePixel;
		const bool publish=selectedScene && status==RemasterVisualClaimStatus::Solved && !physicalConflict && inst.heightOffset==physical &&
			!inst.hasPlacementHeight && !frame.contextGeometry.count(px.instanceId);
		const int base=publish?solution.base[n]:inst.heightOffset;
		const auto key=std::make_tuple(px.instanceId,cell,base,publish);auto it=copies.find(key);
		if(it==copies.end()){
			auto copy=inst;copy.heightOffset=base;copy.visualClaim={status,mask,cell,n<0?-1:root(n),publish};
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
