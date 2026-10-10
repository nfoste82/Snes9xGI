#ifndef REMASTER_HIDDEN_EMITTER_SAMPLES_H
#define REMASTER_HIDDEN_EMITTER_SAMPLES_H
#include "frame.h"
#include <cmath>

// Matches RemasterDirectSample's four float4 ABI without platform SIMD types.
struct RemasterHiddenDirectSample
{
	std::array<float,4> position, endpoint, normal, radiance;
};
static_assert(sizeof(RemasterHiddenDirectSample)==64,"direct sample ABI");

inline std::vector<RemasterHiddenDirectSample> S9xRemasterHiddenDirectSamples(const RemasterFrame &frame, bool collapsedDepth = false)
{
	std::vector<RemasterHiddenDirectSample> result;
	for (const auto &e : frame.hiddenEmitters) {
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
			s.normal={{e.normal[0],e.normal[1],e.normal[2],collapsedDepth ? e.depth : 0}};
			for(unsigned k=0;k<3;k++)
				s.radiance[k]=std::pow(e.emission[k]/255.0f,2.2f)*(e.emission[3]/25.0f)*100.0f/count;
			result.push_back(s);
		}
	}
	return result;
}
#endif
