#ifndef S9X_REMASTER_DUNGEON_VISIBLE_FACE_H
#define S9X_REMASTER_DUNGEON_VISIBLE_FACE_H

// Signed equations for independently identified exposed face regions. This
// solver does not identify exposure, select a gameplay plane, inspect ROMs or
// alter physical support graphs. Callers must establish those facts first.
// In particular a coincident BG cell or a matching tile hash is not ownership.
#include <array>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace S9xRemasterVisibleFace {

struct Region {
	std::array<int16_t,64> height = {};
	bool hflip = false, vflip = false;
	// False includes missing source and ambiguous/overwritten ownership.
	bool established = false;
};

// One demonstrated contact between two regions on the same exposed surface.
// dx/dy points from a to b, step is the signed displayed pixel step across the
// edge. A tangent seam uses 0; an ordinary full unit face uses +/-1.
struct Contact { int a, b, dx, dy, step; };

// Floor height is supplied by the independently solved occupied support and
// local artwork at the actual contact, not by Link's plane or a nearby floor.
struct FloorContact { int region, x, y, height; };

enum class Status { Unanchored, Solved, Conflict };
struct Result { std::vector<int> base; std::vector<Status> status; };

inline int sample(const Region &r, int x, int y) {
	return r.height[(r.vflip ? 7-y : y)*8 + (r.hflip ? 7-x : x)];
}

// The caller's contact list must cover the whole region being claimed. A bad
// edge, owner, or competing floor anchor invalidates its entire component; no
// priority/max/nearest datum or automatic normalization is used. INT_MIN is
// unresolved and 255 remains reserved by the legacy placement representation.
inline Result solve(const std::vector<Region> &regions,
	const std::vector<Contact> &contacts, const std::vector<FloorContact> &floors) {
	const int size = int(regions.size());
	std::vector<std::vector<std::pair<int,int>>> graph(size);
	std::vector<bool> invalid(size, false);
	for(int i=0;i<size;i++) invalid[i]=!regions[i].established;
	for(const auto &c:contacts) {
		if(c.a<0 || c.a>=size || c.b<0 || c.b>=size) continue;
		bool good=c.a!=c.b && std::abs(c.dx)+std::abs(c.dy)==1 && std::abs(c.step)<=1;
		int delta=INT_MIN;
		if(good) for(int k=0;k<8;k++) {
			int ax=c.dx?(c.dx>0?7:0):k, ay=c.dy?(c.dy>0?7:0):k;
			int bx=c.dx?(c.dx>0?0:7):k, by=c.dy?(c.dy>0?0:7):k;
			int ah=sample(regions[c.a],ax,ay), bh=sample(regions[c.b],bx,by);
			if(c.step && (ah-sample(regions[c.a],ax-c.dx,ay-c.dy)!=c.step ||
				sample(regions[c.b],bx+c.dx,by+c.dy)-bh!=c.step)) good=false;
			int d=ah+c.step-bh;
			if(delta!=INT_MIN && delta!=d)good=false;
			delta=d;
		}
		// Keep even a rejected edge in the component: failing one member must
		// not let the remainder be independently published as if intact.
		if(!good) {invalid[c.a]=true;invalid[c.b]=true;delta=0;}
		graph[c.a].push_back({c.b,delta});graph[c.b].push_back({c.a,-delta});
	}
	std::vector<std::vector<int>> anchor(size);
	for(const auto &f:floors) {
		if(f.region<0 || f.region>=size)continue;
		if(f.x<0 || f.x>7 || f.y<0 || f.y>7){invalid[f.region]=true;continue;}
		anchor[f.region].push_back(f.height-sample(regions[f.region],f.x,f.y));
	}
	Result out{std::vector<int>(size,INT_MIN),std::vector<Status>(size,Status::Unanchored)};
	std::vector<int> relative(size,INT_MIN);
	for(int start=0;start<size;start++) if(relative[start]==INT_MIN) {
		std::vector<int> queue(1,start);relative[start]=0;bool bad=false;int datum=INT_MIN;
		for(size_t at=0;at<queue.size();at++) {
			int n=queue[at];bad|=invalid[n];
			for(int base:anchor[n]) {int d=base-relative[n];if(datum!=INT_MIN && datum!=d)bad=true;datum=d;}
			for(auto edge:graph[n]) {
				int v=relative[n]+edge.second;
				if(relative[edge.first]==INT_MIN){relative[edge.first]=v;queue.push_back(edge.first);}
				else if(relative[edge.first]!=v)bad=true;
			}
		}
		if(datum!=INT_MIN)for(int n:queue) {
			int v=datum+relative[n];if(v < -32767 || v > 32767 || v==255)bad=true;
		}
		for(int n:queue) {
			out.status[n]=bad?Status::Conflict:datum==INT_MIN?Status::Unanchored:Status::Solved;
			if(out.status[n]==Status::Solved)out.base[n]=datum+relative[n];
		}
	}
	return out;
}
}
#endif
