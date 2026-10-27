#ifndef S9X_REMASTER_DUNGEON_VISIBLE_FACE_TEMPLATES_H
#define S9X_REMASTER_DUNGEON_VISIBLE_FACE_TEMPLATES_H

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace S9xRemasterVisibleFace {
// A recognizable section is geometry evidence, not proof of a live dispatch,
// PPU exposure, support owner, or a completed visible face. The fourth trim
// band is outside this contract: a later door may legitimately overwrite it.
struct TemplateSection {
	uint16_t source;
	int x, y, axis, reverse, flip;
	// Optional independent semantic/ownership attestation, in sorted displayed
	// cell order. A ROM word match alone does not identify a physical owner.
	// Zero/unknown owners are deliberately ineligible for alias merging.
	struct Equivalence {
		bool established=false;
		uint32_t family=0, owner=0;
		int plane=-1;
		int dx=0, dy=0; // displayed downhill direction
		std::array<std::array<int16_t,64>,6> relief={}; // displayed pixels
		std::array<uint64_t,6> contact={}; // displayed support contact masks
	} equivalence;
};
struct SectionClaim { int section=-1; bool ambiguous=false; };

// Exact final placement words for each complete 3x2 face footprint. Priority
// bit changes do not alter art; tile, palette and both flips do. Every compatible
// section is gathered before determining ambiguity. Caller supplies verified
// ROM templates and 4096 final words for one BG in the matching state.
inline void collectSections(const uint16_t *words, const uint8_t *rom, size_t romSize,
	const std::vector<TemplateSection> &specs, std::array<SectionClaim,4096> &claims) {
	claims.fill({});
	if(!words || !rom)return;
	std::vector<std::array<int,6>> footprints(specs.size());
	std::vector<bool> matched(specs.size(),false);
	for(size_t s=0;s<specs.size();s++) {
		const auto &v=specs[s];
		if(v.axis<0 || v.axis>1 || v.reverse<0 || v.reverse>1 || v.flip<0 || v.flip>3)continue;
		if(size_t(v.source)+16>romSize)continue;
		const int w=v.axis?4:2,h=v.axis?2:4,start=v.reverse;
		if(v.x<0 || v.y<0 || v.x+w>64 || v.y+h>64)continue;
		std::array<int,6> cells={};bool exact=true;int n=0;
		for(int tangent=0;tangent<2;tangent++)for(int depth=start;depth<start+3;depth++) {
			int sx=v.axis?depth:tangent,sy=v.axis?tangent:depth;
			int tx=(v.flip&1)?w-1-sx:sx,ty=(v.flip&2)?h-1-sy:sy;
			int cell=(v.y+ty)*64+v.x+tx;
			int source=v.source+2*(tangent*4+depth);
			int expected=(rom[source]|rom[source+1]<<8)^((v.flip&1?0x4000:0)|(v.flip&2?0x8000:0));
			if((words[cell]&0xdfff)!=(expected&0xdfff))exact=false;
			cells[n++]=cell;
		}
		if(!exact)continue;
		std::sort(cells.begin(),cells.end());footprints[s]=cells;matched[s]=true;
		auto equivalent=[&](size_t a,size_t b) {
			const auto &x=specs[a].equivalence,&y=specs[b].equivalence;
			return matched[a] && matched[b] && footprints[a]==footprints[b] &&
				x.established && y.established && x.owner && x.family && x.plane>=0 && x.plane<2 &&
				x.family==y.family && x.owner==y.owner && x.plane==y.plane &&
				x.dx==y.dx && x.dy==y.dy && x.relief==y.relief && x.contact==y.contact;
		};
		for(int cell:cells) {
			auto &claim=claims[cell];
			if(claim.section<0 && !claim.ambiguous)claim.section=int(s);
			else if(!claim.ambiguous && !equivalent(size_t(claim.section),s)){claim.section=-1;claim.ambiguous=true;}
		}
	}
}

struct RoomSections {
	std::vector<TemplateSection> sections;
	std::array<SectionClaim,4096> claims={};
};

// The complete final-map producer enumerates the verified straight perimeter
// ROM families in each BG independently. Witness is responsible for proving
// the same owner, plane, displayed local relief and contact semantics; this
// function must never infer physical ownership from artwork or proximity.
// The caller owns the ROM pin, collision/exposure and witness lifetime.
template<class Witness>
inline std::array<RoomSections,2> collectRoomSections(const uint16_t *bg1,
	const uint16_t *bg2,const uint8_t *rom,size_t romSize,Witness witness) {
	std::array<RoomSections,2> out;
	for(auto &bg:out)bg.claims.fill({});
	if(!bg1 || !bg2 || !rom)return out;
	struct Family {uint16_t source;int axis,reverse;};
	constexpr Family families[]={{0x2e8,0,0},{0x2f8,0,1},{0x328,0,0},{0x338,0,1},
		{0x2c8,1,0},{0x2d8,1,1},{0x308,1,0},{0x318,1,1}};
	const uint16_t *maps[]={bg1,bg2};
	for(const auto &f:families) {
		if(size_t(f.source)+16>romSize)continue;
		bool varied=false;
		for(int i=1;i<8;i++)if(rom[f.source+i*2]!=rom[f.source] || rom[f.source+i*2+1]!=rom[f.source+1])varied=true;
		if(!varied)continue; // default/empty table bytes are not an assembly
		int w=f.axis?4:2,h=f.axis?2:4;
		for(int flip=0;flip<4;flip++)for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++) {
			std::array<int,6> cells={};std::array<uint16_t,6> expected={};int n=0;
			for(int tangent=0;tangent<2;tangent++)for(int depth=f.reverse;depth<f.reverse+3;depth++) {
				int sx=f.axis?depth:tangent,sy=f.axis?tangent:depth;
				int tx=flip&1?w-1-sx:sx,ty=flip&2?h-1-sy:sy;
				cells[n]=(y+ty)*64+x+tx;
				int at=f.source+2*(tangent*4+depth);
				expected[n++]=(rom[at]|rom[at+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
			}
			for(int bg=0;bg<2;bg++) {
				bool exact=true;for(int i=0;i<6;i++)if((maps[bg][cells[i]]&0xdfff)!=(expected[i]&0xdfff)){exact=false;break;}
				if(!exact)continue;
				TemplateSection s{f.source,x,y,f.axis,f.reverse,flip,{}};
				auto sorted=cells;std::sort(sorted.begin(),sorted.end());
				s.equivalence=witness(bg,s,sorted);
				out[bg].sections.push_back(std::move(s));
			}
		}
	}
	for(int bg=0;bg<2;bg++)collectSections(maps[bg],rom,romSize,out[bg].sections,out[bg].claims);
	return out;
}
}
#endif
