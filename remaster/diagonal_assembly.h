#ifndef S9X_REMASTER_DIAGONAL_ASSEMBLY_H
#define S9X_REMASTER_DIAGONAL_ASSEMBLY_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// User-approved semantic boundary: count actual placed SE face members until
// an explicitly classified different surface. Missing classification is NOT a
// top. The foot must retain an authored grounded region or proved foot datum.
// Repeated intrinsic arrays do not supply band phase; room-local order does.
template<class Placements>
inline void S9xRemasterSemanticDiagonalExtents(Placements &tiles)
{
	struct Claim { bool set=false,conflict=false;int intercept=0,minimum=0,maximum=0; };
	std::vector<Claim> claims(4096);
	for(int i=0;i<4096;i++) {
		const auto &foot=tiles[i];
		if(!foot.semanticSoutheastWall || !foot.hasHeight)continue;
		const bool provenFoot=foot.groundedFloorMask || (foot.diagonalHeightPreview && foot.diagonalPreviewRow==3);
		if(!provenFoot)continue;
		const int minimum=foot.groundedFloorMask?0:foot.diagonalPreviewFoot;
		const int up=foot.vflip?1:-1,x=i%64,y=i/64;
		const int lowY=y-up;
		if(lowY<0 || lowY>=64)continue;
		const auto &low=tiles[lowY*64+x];
		if(low.semanticSoutheastWall || !low.semanticBoundaryKnown)continue;
		// Tangent corroboration is a placed assembly relation, not a lone
		// annotated tile. H/V transform the canonical x+y invariant together.
		const int tangent=foot.hflip==foot.vflip?-1:1;
		bool companion=false;
		for(int sign:{-1,1}) {
			const int nx=x+sign,ny=y+sign*tangent;
			if(nx<0 || nx>=64 || ny<0 || ny>=64)continue;
			const auto &n=tiles[ny*64+nx];
			if(n.semanticSoutheastWall && n.hasHeight && n.hflip==foot.hflip && n.vflip==foot.vflip &&
				(n.groundedFloorMask || (n.diagonalHeightPreview && n.diagonalPreviewRow==3)))companion=true;
		}
		if(!companion)continue;
		int extent=0;
		for(int yy=y;yy>=0 && yy<64;yy+=up) {
			const auto &t=tiles[yy*64+x];
			if(!t.semanticSoutheastWall)break;
			if(!t.hasHeight || t.hflip!=foot.hflip || t.vflip!=foot.vflip){extent=0;break;}
			extent++;
		}
		const int topY=y+extent*up;
		if(extent<2 || topY<0 || topY>=64 || !tiles[topY*64+x].semanticBoundaryKnown)continue;
		// The diagonal projection occupies one extra partial band. Only the
		// actual terminator clips the final band; never an intrinsic internal cap.
		const int maximum=minimum+(extent-1)*8;
		if(maximum>254)continue;
		// Semantic extent is a shape rule, not license to repair incompatible
		// cardinal datum. Preserve original approved four-band shell when its
		// authored foot convention agrees; larger spans replace internal caps.
		if(extent<=4)continue; // approved intrinsic short shell is already exact
		for(int row=0;row<extent;row++) {
			const int cell=(y+row*up)*64+x;auto &claim=claims[cell];const int intercept=minimum+7+row*8;
			if(claim.set && (claim.intercept!=intercept || claim.minimum!=minimum || claim.maximum!=maximum))claim.conflict=true;
			claim.set=true;claim.intercept=intercept;claim.minimum=minimum;claim.maximum=maximum;
		}
	}
	// A cardinal dispatch can overwrite the foot of a diagonal column while
	// leaving its upper face intact. Transport band phase across the oblique
	// same-band seam from an independently foot/top-proved companion. Require
	// the surviving column's own explicit top, identical transformed semantic
	// ownership and final identity at EVERY shared band. Never propagate through
	// the overwritten cells or treat their cardinal art as diagonal members.
	for(int pass=0;pass<64;pass++) {
		bool changed=false;auto next=claims;
		for(int i=0;i<4096;i++) {
			const auto &seed=claims[i];const auto &t=tiles[i];
			if(!seed.set || seed.conflict || !t.semanticSoutheastWall)continue;
			const int tangent=t.hflip==t.vflip?-1:1,up=t.vflip?1:-1;
			for(int sign:{-1,1}) {
				const int x=i%64+sign,y=i/64+sign*tangent;
				if(x<0 || x>=64 || y<0 || y>=64)continue;
				const int target=y*64+x;const auto &n=tiles[target];
				if(!n.semanticSoutheastWall || !n.hasHeight || n.hflip!=t.hflip || n.vflip!=t.vflip || n.tileHash!=t.tileHash)continue;
				int top=y;
				while(top>=0 && top<64 && tiles[top*64+x].semanticSoutheastWall) {
					const auto &member=tiles[top*64+x];
					if(!member.hasHeight || member.hflip!=t.hflip || member.vflip!=t.vflip)break;
					top+=up;
				}
				if(top<0 || top>=64 || !tiles[top*64+x].semanticBoundaryKnown || tiles[top*64+x].semanticSoutheastWall)continue;
				// The last partial band must be the same actual clipped top as the
				// source assembly; no guessed height from a different terminator.
				if(seed.intercept+((top-up-y)*up)*8!=seed.maximum+7)continue;
				auto &claim=next[target];
				if(claim.set) {
					if(claim.intercept!=seed.intercept || claim.minimum!=seed.minimum || claim.maximum!=seed.maximum)claim.conflict=true;
				} else {claim=seed;changed=true;}
			}
		}
		claims.swap(next);if(!changed)break;
	}
	for(int i=0;i<4096;i++) {
		const auto &claim=claims[i];if(!claim.set || claim.conflict)continue;
		auto &t=tiles[i];t.surface=decltype(t.surface)::Wall;t.diagonalHeightPreview=true;t.diagonalIntercept=claim.intercept;
		t.diagonalMinimum=claim.minimum;t.diagonalMaximum=claim.maximum;
		for(int y=0;y<8;y++)for(int x=0;x<8;x++)
			t.height[y*8+x]=std::max(claim.minimum,std::min(claim.maximum,claim.intercept-x-y));
	}
}

// Evidence adapter boundary: identities may repeat; position and writer own
// phase. No tile/hash, room, adjacency or authored-normal inference occurs here.
// Contact heights are LOCAL to one independently solved signed support base.
struct RemasterDiagonalAssemblyMember
{
	int cell = -1;
	int intercept = 0;
	uint16_t writer = 0;
	bool finalWordVerified = false;
	bool ownershipVerified = false;
	bool semanticCompatible = false;
	// Multiple distinct writer/phase hypotheses must not select the first one.
	bool ambiguous = false;
};

struct RemasterDiagonalAssemblyContacts
{
	int minimum = 0, maximum = 0;
	bool supportVerified = false, topVerified = false;
	bool sameAssembly = false;
	bool conflict = false;
};

struct RemasterDiagonalAssemblyGeometry
{
	int cell = -1, intercept = 0, minimum = 0, maximum = 0;
	std::array<uint8_t,64> height = {};
};

inline bool S9xRemasterSolveDiagonalAssembly(
	const RemasterDiagonalAssemblyContacts &contacts,
	const std::vector<RemasterDiagonalAssemblyMember> &members,
	std::vector<RemasterDiagonalAssemblyGeometry> &geometry)
{
	geometry.clear();
	if(!contacts.supportVerified || !contacts.topVerified || !contacts.sameAssembly || contacts.conflict ||
		contacts.minimum<0 || contacts.maximum>254 || contacts.maximum<=contacts.minimum)return false;
	// Heap-backed fixed cell index: repeats in asset identity do not matter,
	// duplicate ownership claims do. Entire conflicted assembly rejects.
	std::vector<int> claims(4096,-1);
	for(size_t n=0;n<members.size();n++) {
		const auto &m=members[n];
		if(m.ambiguous)return false;
		if(!m.finalWordVerified || !m.ownershipVerified || !m.semanticCompatible || !m.writer)continue;
		if(m.cell<0 || m.cell>=4096)return false;
		if(claims[m.cell]>=0) {
			const auto &previous=members[claims[m.cell]];
			if(previous.intercept!=m.intercept || previous.writer!=m.writer)return false;
			continue;
		}
		claims[m.cell]=static_cast<int>(n);
		RemasterDiagonalAssemblyGeometry g;
		g.cell=m.cell;g.intercept=m.intercept;g.minimum=contacts.minimum;g.maximum=contacts.maximum;
		for(int y=0;y<8;y++)for(int x=0;x<8;x++)
			g.height[y*8+x]=std::max(g.minimum,std::min(g.maximum,g.intercept-x-y));
		geometry.push_back(g);
	}
	return !geometry.empty();
}
#endif
