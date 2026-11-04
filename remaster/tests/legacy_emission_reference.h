#ifndef REMASTER_LEGACY_EMISSION_REFERENCE_H
#define REMASTER_LEGACY_EMISSION_REFERENCE_H

// Historical point-light approximation, not the production emitter transport.
// Retained only for the old monolithic compatibility test.
#include "../runtime/frame.h"

inline std::vector<RemasterFrameLight> legacyFrameEmissionLights(const RemasterFrame &frame)
{
	RemasterFrameChannelResolver channels(frame);
	struct Accumulator
	{
		float x=0, y=0, red=0, green=0, blue=0, z=0, weight=0;
		bool emissive=false;
	};
	std::vector<Accumulator> accumulators(frame.tileInstances.size());
	for (size_t offset=0; offset<frame.mainPixels.size(); ++offset)
	{
		const auto selected=channels.select(frame,offset,!frame.scenePixels.empty());
		const auto pixel=selected.donors[size_t(RemasterMetadataChannel::Emission)];
		if (!pixel.instanceId || pixel.instanceId>frame.tileInstances.size() || pixel.tilePixel>=64) continue;
		auto &accumulator=accumulators[pixel.instanceId-1];
		const auto &instance=frame.tileInstances[pixel.instanceId-1];
		const auto *metadata=channels.metadata[pixel.instanceId-1];
		if (!metadata || !metadata->hasEmission) continue;
		const auto emission=S9xRemasterEmissionSample(*metadata,pixel.tilePixel,
			instance.hasPaletteArtwork?instance.paletteRgb555[pixel.tilePixel]:frame.originalRgb555[offset]);
		const float intensity=metadata->emissionRgba[pixel.tilePixel*4+3]/25.0f;
		if (intensity<=0) continue;
		accumulator.x+=(float(offset%frame.width)+0.5f)*intensity;
		accumulator.y+=(float(offset/frame.width)+0.5f)*intensity;
		accumulator.red+=std::pow(emission[0]/255.0f,2.2f)*intensity/8.0f;
		accumulator.green+=std::pow(emission[1]/255.0f,2.2f)*intensity/8.0f;
		accumulator.blue+=std::pow(emission[2]/255.0f,2.2f)*intensity/8.0f;
		const auto hp=selected.donors[size_t(RemasterMetadataChannel::Height)];
		const int height=hp.instanceId?frame.tileInstances[hp.instanceId-1].heightOffset+
			channels.metadata[hp.instanceId-1]->height[hp.tilePixel]:0;
		accumulator.z+=height/255.0f*intensity;
		accumulator.weight+=intensity;
		accumulator.emissive=true;
	}
	std::vector<RemasterFrameLight> lights;
	for (const auto &accumulator:accumulators)
	{
		if (!accumulator.emissive || accumulator.weight<=0) continue;
		RemasterFrameLight light;
		light.x=accumulator.x/accumulator.weight;
		light.y=accumulator.y/accumulator.weight;
		light.z=std::max(accumulator.z/accumulator.weight*frame.lightingCoordinateScale,frame.lightingCoordinateScale/255.0f);
		light.radius=96;
		light.red=accumulator.red; light.green=accumulator.green; light.blue=accumulator.blue;
		light.intensity=12;
		lights.push_back(light);
	}
	return lights;
}

#endif
