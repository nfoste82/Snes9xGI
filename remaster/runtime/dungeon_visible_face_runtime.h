#ifndef S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H
#define S9X_REMASTER_DUNGEON_VISIBLE_FACE_RUNTIME_H

#include "dungeon_visible_face.h"
#include "dungeon_visible_face_templates.h"
#include "frame.h"

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
};
// A frame-local visual solve. It consumes completed physical solves, but never
// adds an edge or datum to either physical graph. Exposure is the actual shared
// channel selector, not every tile which happens to exist in a BG map.
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
	auto visibleStorage=std::make_unique<std::array<std::array<uint64_t,4096>,2>>();auto &visible=*visibleStorage;
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
		if(choice.visible.instanceId==px.instanceId && choice.visible.tilePixel==px.tilePixel)
			visible[bg][cell]|=UINT64_C(1)<<((y%8)*8+x%8);
		int &base=exposureBase[bg][cell];
		if(base==INT_MIN)base=inst.heightOffset;else if(base!=inst.heightOffset)base=INT_MAX;
	}
	auto edgeMask=[](int dx,int dy) {uint64_t m=0;for(int k=0;k<8;k++)m|=UINT64_C(1)<<((dy?(dy>0?7:0):k)*8+(dx?(dx>0?7:0):k));return m;};
	auto neighbor=[](int cell,int dx,int dy){int x=cell%64+dx,y=cell/64+dy;return x<0||x>=64||y<0||y>=64?-1:y*64+x;};
	std::vector<Region> regions;std::vector<Contact> contacts;std::vector<FloorContact> floorContacts;
	auto nodeStorage=std::make_unique<std::array<std::array<int,4096>,2>>();auto &nodes=*nodeStorage;for(auto &a:nodes)a.fill(-1);
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++)if(candidates[bg][i].section>=0 && exposed[bg][i]) {
		const auto &t=tiles[bg][i];nodes[bg][i]=int(regions.size());regions.push_back({t.height,t.hflip,t.vflip,true});
	}
	struct Anchor {int node,plane,owner,cell;};std::vector<Anchor> anchors;
	for(int bg=0;bg<2;bg++)for(int i=0;i<4096;i++)if(nodes[bg][i]>=0) {
		const auto &a=candidates[bg][i];const int dx=a.axis?a.down:0,dy=a.axis?0:a.down,j=neighbor(i,dx,dy);
		// A lone clipped edge is not a demonstrated exposed face direction.
		// Retain the inward sample row as well as the shared edge. In particular
		// do not publish a partially visible end from an offscreen inferred ramp.
		const uint64_t edge=edgeMask(dx,dy),inner=dx>0?edge>>1:dx<0?edge<<1:dy>0?edge>>8:edge<<8;
		if(j<0 || (exposed[bg][i]&(edge|inner))!=(edge|inner))continue;
		for(int other=0;other<2;other++) {
			if((exposed[other][j]&edgeMask(-dx,-dy))!=edgeMask(-dx,-dy))continue;
			if(nodes[other][j]>=0) {
				const auto &b=candidates[other][j];
				if(a.axis==b.axis && a.down==b.down && (bg==other ? a.family==b.family : a.family!=b.family))
					contacts.push_back({nodes[bg][i],nodes[other][j],dx,dy,-1});
				continue;
			}
			const auto &t=tiles[other][j];if(!t.hasHeight || t.surface==Surface::Wall || t.surface==Surface::Stair ||
				t.surface==Surface::Door || t.surface==Surface::ClosedDoor || t.surface==Surface::Obstacle ||
				t.obstacleAssembly || t.authoredObstacle || t.removableSupportPlanes)continue;
			// A channel fallback is a donor, not evidence of a visible floor
			// touching this face. Nor is a raised local sheet a physical datum.
			if((visible[other][j]&edgeMask(-dx,-dy))!=edgeMask(-dx,-dy))continue;
			for(int p=0;p<2;p++) {
				const auto &d=maps[other]->roomDiagnostics->cells[p*4096+j];
				const int owner=maps[other]->occupiedFloorOwners[p][j],base=maps[other]->planeOffsets[p][j];
				if(owner<0 || base==255 || exposureBase[other][j]!=base || c.collisionAttributes[p*4096+j]!=0 || d.wall || d.heightConflict || !d.planeObserved ||
					d.floorId!=owner || d.floorBase!=base)continue;
				int floor=INT_MIN;bool flat=true;
				for(int k=0;k<8;k++) {
					int x=dx?(dx>0?0:7):k,y=dy?(dy>0?0:7):k;
					int value=base+t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];
					if(floor!=INT_MIN && floor!=value)flat=false;floor=value;
				}
				if(!flat || floor!=base)continue;
				anchors.push_back({nodes[bg][i],p,owner,j});
				for(int k=0;k<8;k++)floorContacts.push_back({nodes[bg][i],dx?(dx>0?7:0):k,dy?(dy>0?7:0):k,floor});
			}
		}
	}
	// Distinct occupied owners (including planes) remain ambiguous even if they
	// happen to have equal bases. Component membership uses only admitted seams.
	std::vector<int> parent(regions.size());for(size_t i=0;i<parent.size();i++)parent[i]=int(i);
	auto root=[&](int n){while(parent[n]!=n)n=parent[n];return n;};
	for(const auto &e:contacts){int a=root(e.a),b=root(e.b);if(a!=b)parent[b]=a;}
	// The two tangent rows of a complete surviving ROM section are one
	// ownership claim, not two independently publishable one-row surfaces.
	for(int bg=0;bg<2;bg++)for(const auto &s:sections[bg].sections) {
		int first=-1,w=s.axis?4:2,h=s.axis?2:4;
		for(int t=0;t<2;t++)for(int d=s.reverse;d<s.reverse+3;d++) {
			int x=s.axis?d:t,y=s.axis?t:d,cell=(s.y+(s.flip&2?h-1-y:y))*64+s.x+(s.flip&1?w-1-x:x);
			int n=nodes[bg][cell];if(n<0)continue;
			if(first<0)first=n;else {int a=root(first),b=root(n);if(a!=b)parent[b]=a;}
		}
	}
	std::vector<int> owner(regions.size(),-1),plane(regions.size(),-1);std::vector<bool> conflict(regions.size());
	for(const auto &a:anchors){int r=root(a.node);if(owner[r]>=0 && (owner[r]!=a.owner || plane[r]!=a.plane))conflict[r]=true;else {owner[r]=a.owner;plane[r]=a.plane;}}
	const auto solution=solve(regions,contacts,floorContacts);
	for(int bg=0;bg<2;bg++)for(auto &s:sections[bg].sections) {
		int own=-1,p=-1;bool bad=false;std::array<int,6> cells={};int z=0,w=s.axis?4:2,h=s.axis?2:4;
		for(int t=0;t<2;t++)for(int d=s.reverse;d<s.reverse+3;d++){int x=s.axis?d:t,y=s.axis?t:d;cells[z++]=(s.y+(s.flip&2?h-1-y:y))*64+s.x+(s.flip&1?w-1-x:x);}
		std::sort(cells.begin(),cells.end());
		for(int cell:cells){int n=nodes[bg][cell];if(n<0)continue;int r=root(n);if(conflict[r])bad=true;if(owner[r]>=0){if(own>=0 && (own!=owner[r] || p!=plane[r]))bad=true;own=owner[r];p=plane[r];}}
		if(bad || own<0)continue;
		auto &e=s.equivalence;e.established=true;e.family=s.source<0x300?1:2;e.owner=uint32_t(own)+1;e.plane=p;
		for(int k=0;k<6;k++){const auto &t=tiles[bg][cells[k]];for(int y=0;y<8;y++)for(int x=0;x<8;x++)e.relief[k][y*8+x]=t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];e.contact[k]=exposed[bg][cells[k]];}
		const auto &v=candidates[bg][cells[0]];e.dx=v.axis?v.down:0;e.dy=v.axis?0:v.down;
	}
	for(int bg=0;bg<2;bg++)collectSections(words[bg].data(),c.dungeonTemplateData,12876,sections[bg].sections,sections[bg].claims);
	// Change only the actual scene occurrence selected by the shared policy.
	// Clone per placement/base; never mutate physical map arrays or stock pixels.
	std::map<std::pair<uint32_t,int>,uint32_t> copies;
	for(size_t at=0;at<selected.size();at++) {
		const auto px=selected[at];if(!px.instanceId || frame.scenePixels[at].instanceId!=px.instanceId || frame.scenePixels[at].tilePixel!=px.tilePixel)continue;
		const auto &inst=frame.tileInstances[px.instanceId-1];int bg=inst.sourceIndex;
		int x=(c.backgroundScrollX+at%frame.width)&511,y=(c.backgroundScrollY+at/frame.width)&511,cell=y/8*64+x/8,n=nodes[bg][cell];
		if(n<0 || sections[bg].claims[cell].section<0 || sections[bg].claims[cell].ambiguous ||
			solution.status[n]!=Status::Solved || conflict[root(n)] || owner[root(n)]<0)continue;
		// A contextual/actor/transition contribution is not this map's placement
		// base. Do not erase it when selecting the separate visual representative.
		const int physical=maps[bg]->offsets[cell]==255?0:maps[bg]->offsets[cell];
		if(inst.heightOffset!=physical || inst.hasPlacementHeight || frame.contextGeometry.count(px.instanceId))continue;
		const int base=solution.base[n];const auto key=std::make_pair(px.instanceId,base);auto it=copies.find(key);
		if(it==copies.end()){auto copy=inst;copy.heightOffset=base;frame.tileInstances.push_back(copy);it=copies.emplace(key,uint32_t(frame.tileInstances.size())).first;}
		frame.scenePixels[at].instanceId=it->second;
	}
}
#endif
