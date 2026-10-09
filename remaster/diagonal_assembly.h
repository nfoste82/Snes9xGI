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
