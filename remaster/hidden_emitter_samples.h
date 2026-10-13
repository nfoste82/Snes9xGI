#ifndef REMASTER_HIDDEN_EMITTER_SAMPLES_H
#define REMASTER_HIDDEN_EMITTER_SAMPLES_H
#include "frame.h"
#include "emitter_patches.h"
#include <cmath>

// Matches RemasterDirectSample's four float4 ABI without platform SIMD types.
struct RemasterHiddenDirectSample
{
	std::array<float,4> position, endpoint, normal, radiance;
};
static_assert(sizeof(RemasterHiddenDirectSample)==64,"direct sample ABI");

inline std::vector<RemasterHiddenDirectSample> S9xRemasterHiddenDirectSamples(const RemasterFrame &frame,
	bool collapsedDepth = false, bool clustered = false)
{
	std::vector<RemasterHiddenDirectSample> result;
	auto append = [&](const RemasterFrameHiddenEmitter &e, const std::array<float,3> &radiance, unsigned area) {
		const unsigned count=e.depth>0 && !collapsedDepth ? 4 : 1;
		for(unsigned stratum=0;stratum<count;stratum++) {
			RemasterHiddenDirectSample s={};
			const float offset=collapsedDepth ? 0 : e.depth*(stratum+0.5f)/count;
			s.position={{e.x+e.normal[0]*offset,e.y+e.normal[1]*offset,e.z+e.normal[2]*offset,
				e.depth>0 ? 1.0f : e.omnidirectional ? 2.0f : 0.0f}};
			s.endpoint=s.position;
			// HUD pixels have no world shell; retain the source point with only
			// the same epsilon used for known zero-thickness visible surfaces.
			if (e.depth>0 || !e.omnidirectional) s.endpoint[2]+=0.0001f;
			s.endpoint[3]=0;
			s.normal={{e.normal[0],e.normal[1],e.normal[2],collapsedDepth ? e.depth : clustered ? -e.depth : 0}};
			for(unsigned k=0;k<3;k++)
				s.radiance[k]=radiance[k]/count;
			s.radiance[3]=clustered ? float(area)/count : 0;
			result.push_back(s);
		}
	};
	auto power = [](const RemasterFrameHiddenEmitter &e) {
		std::array<float,3> value;
		for(unsigned k=0;k<3;k++) value[k]=std::pow(e.emission[k]/255.0f,2.2f)*(e.emission[3]/25.0f)*100.0f;
		return value;
	};
	if (!clustered || (frame.emitterPatchSize <= 1 && !frame.representativeEmitters)) {
		for (const auto &e : frame.hiddenEmitters) append(e,power(e),1);
		return result;
	}
	// Source-only clustering uses the same bounded rectangle/representative rules
	// as visible pixels, on tiny CPU-only 8x8 support grids. Keep source geometry
	// and directional modes separate; these grids never enter scene/receiver data.
	using Key=std::tuple<int,int,float,float,std::array<float,3>,bool>;
	std::map<Key,std::vector<const RemasterFrameHiddenEmitter *>> groups;
	for (const auto &e : frame.hiddenEmitters) {
		// Actual-member representatives can span varying height/normal/depth,
		// exactly as visible representative patches do. Centroids require common
		// flat support. Separate planar/depth layouts to keep strata unambiguous.
		groups[{int(std::floor(e.x/8)),int(std::floor(e.y/8)),frame.representativeEmitters ? 0 : e.z,
			frame.representativeEmitters ? float(e.depth>0) : e.depth,
			frame.representativeEmitters ? std::array<float,3>{} : e.normal,e.omnidirectional}].push_back(&e);
	}
	for (const auto &entry : groups) {
		const auto &sources=entry.second;
		RemasterFrame support;
		support.width=support.height=8;
		support.emitterPatchSize=frame.emitterPatchSize;
		support.representativeEmitters=frame.representativeEmitters;
		support.mainPixels.resize(64);support.subPixels.resize(64);
		std::array<uint8_t,256> emission={};std::array<uint8_t,128> participation={};
		std::array<float,256> surface={};std::vector<RemasterSurfaceMesh::Cell> mesh(64);
		std::array<const RemasterFrameHiddenEmitter *,64> pixels={};
		bool duplicate=false;
		for (const auto *e : sources) {
			const int xx=int(std::floor(e->x)),yy=int(std::floor(e->y));
			// Signed offscreen coordinates need floor-division buckets and positive
			// remainders; truncation previously addressed outside the8x8 grid.
			unsigned p=((yy%8+8)%8)*8+(xx%8+8)%8;
			if (pixels[p]) { duplicate=true;break; }
			pixels[p]=e;
			support.mainPixels[p]={0x02000000u,1,uint8_t(p)};
			std::copy(e->emission.begin(),e->emission.end(),emission.begin()+p*4);
			participation[p*2]=participation[p*2+1]=255;
			surface[p*4]=e->z;
			std::copy(e->normal.begin(),e->normal.end(),surface.begin()+p*4+1);
			mesh[p].emissionDepth=e->depth;mesh[p].emissionOmnidirectional=e->omnidirectional;
			std::fill_n(mesh[p].corners,4,e->z);std::copy(e->normal.begin(),e->normal.end(),mesh[p].normal);
		}
		// Two room planes may emit at the same XY: preserve both rather than
		// overwriting the support grid or silently losing their power.
		if (duplicate) { for(const auto *e:sources)append(*e,power(*e),1);continue; }
		for (const auto &patch : S9xRemasterEmitterPatches(support,emission.data(),participation.data(),surface.data(),mesh)) {
			std::array<float,3> radiance={};float weight=0,x=0,y=0;
			for(unsigned j=0;j<patch.count;j++) {
				const auto &e=*pixels[patch.members[j]];const auto value=power(e);
				float w=*std::max_element(value.begin(),value.end());weight+=w;x+=e.x*w;y+=e.y*w;
				for(unsigned k=0;k<3;k++)radiance[k]+=value[k];
			}
			if (weight<=0)continue;
			x/=weight;y/=weight;
			auto representative=*pixels[patch.members[0]];
			if (patch.mode&1u) {
				float nearest=INFINITY;
				for(unsigned j=0;j<patch.count;j++) {
					const auto &e=*pixels[patch.members[j]];float d=(e.x-x)*(e.x-x)+(e.y-y)*(e.y-y);
					if(d<nearest){nearest=d;representative=e;}
				}
			} else { representative.x=x;representative.y=y; }
			append(representative,radiance,patch.count);
		}
	}
	return result;
}
#endif
