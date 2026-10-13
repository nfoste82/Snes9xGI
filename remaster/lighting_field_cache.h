#ifndef REMASTER_LIGHTING_FIELD_CACHE_H
#define REMASTER_LIGHTING_FIELD_CACHE_H

#include "frame.h"
#include "held_lamp.h"
#include "surface_mesh_material.h"
#include <map>

// Exact frame-local input comparison. No assumptions about static game artwork;
// emission is independent of geometry and transient instance IDs remain domains.
struct RemasterLightingFieldCache
{
	std::vector<uint8_t> occlusion, emission, participation, opposite, authoredNormals;
	std::vector<float> height, surface, reflectance, depths, omnidirectional;
	std::vector<RemasterSurfaceMesh::Sample> samples;
	size_t updatedPixels = 0;

	static bool geometryEqual(const RemasterFrameAssetMetadata &a, const RemasterFrameAssetMetadata &b)
	{
		return a.hasMaterialSelectors == b.hasMaterialSelectors && a.materialSelectors == b.materialSelectors &&
			a.hasOcclusion == b.hasOcclusion && a.occlusion == b.occlusion &&
			a.hasHeight == b.hasHeight && a.height == b.height && a.hasNormals == b.hasNormals &&
			a.normalXyz == b.normalXyz && a.directLightingOppositeFacing == b.directLightingOppositeFacing &&
			a.normalIgnoreTileTransform == b.normalIgnoreTileTransform;
	}
	static bool instanceEqual(const RemasterFrameTileInstance &a, const RemasterFrameTileInstance &b)
	{
		return a.tileId == b.tileId && a.source == b.source && a.sourceIndex == b.sourceIndex &&
			a.heightOffset == b.heightOffset && a.hasPlacementHeight == b.hasPlacementHeight &&
			a.placementHeight == b.placementHeight && a.hFlip == b.hFlip && a.vFlip == b.vFlip &&
			a.normalYaw == b.normalYaw && a.assetGroup == b.assetGroup && a.material == b.material;
	}
	static bool materialEqual(const RemasterFrameMaterial &a, const RemasterFrameMaterial &b)
	{
		return a.name == b.name && a.surfaceClass == b.surfaceClass && a.receivesGi == b.receivesGi &&
			a.hasDiffuseReflectance == b.hasDiffuseReflectance && a.diffuseReflectance == b.diffuseReflectance;
	}
	void update(const RemasterFrame &frame, bool useScene, bool forceFull = false)
	{
		const auto &pixels = useScene ? frame.scenePixels : frame.mainPixels;
		const size_t count = frame.mainPixels.size();
		bool full = forceFull || width_ != frame.width || height_ != frame.height || scale_ != frame.lightingCoordinateScale || keys_.size() != count;
		if (materials_.size() != frame.materials.size()) full = true;
		else for (size_t i = 0; i < materials_.size(); i++) if (!materialEqual(materials_[i], frame.materials[i])) { full = true; break; }
		if (keys_.size() != count)
		{
			keys_.resize(count); occlusion.resize(count*2); emission.resize(count*4);
			participation.resize(count*2); opposite.resize(count); authoredNormals.resize(count);
			height.resize(count*2); surface.resize(count*4); reflectance.resize(count*4);
			depths.resize(count); omnidirectional.resize(count); samples.resize(count);
		}
		std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
		for (const auto &m : frame.assetMetadata) metadata[m.tileId] = &m;
		std::map<std::string, const RemasterFrameMaterial *> materials;
		for (const auto &m : frame.materials) materials[m.name] = &m;
		std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(frame.tileInstances.size());
		std::vector<bool> dirty(frame.tileInstances.size(), full);
		std::vector<uint64_t> normalMasks(frame.tileInstances.size());
		std::map<RemasterTileContentId, bool> metadataDirty;
		for (const auto &entry : metadata)
		{
			const auto old = metadata_.find(entry.first);
			metadataDirty[entry.first] = old == metadata_.end() || !geometryEqual(old->second, *entry.second);
		}
		for (size_t i = 0; i < frame.tileInstances.size(); i++)
		{
			const auto &instance = frame.tileInstances[i];
			const auto found = metadata.find(instance.tileId);
			const auto *m = found == metadata.end() ? nullptr : found->second;
			instanceMetadata[i] = m;
			normalMasks[i] = S9xRemasterContinuedDiagonalNormalMask(instance, m);
			dirty[i] = full || i >= instances_.size() || !instanceEqual(instance, instances_[i]) ||
				(m ? metadataDirty[instance.tileId] : metadata_.count(instance.tileId) != 0) ||
				i >= normalMasks_.size() || normalMasks[i] != normalMasks_[i];
		}
		updatedPixels = 0;
		for (size_t i = 0; i < count; i++)
		{
			const bool hud = S9xRemasterHeldLampHudPixel(frame, i);
			const auto &main = pixels[i];
			const bool sub = !useScene && !main.instanceId && static_cast<uint8_t>(main.owner >> 24) == static_cast<uint8_t>(RemasterSourceType::Backdrop);
			const auto &pixel = sub ? frame.subPixels[i] : main;
			const bool valid = !hud && pixel.instanceId && pixel.instanceId <= frame.tileInstances.size() && pixel.tilePixel < 64;
			const uint64_t key = valid ? (uint64_t(pixel.instanceId) << 8) | pixel.tilePixel : hud ? UINT64_MAX : 0;
			const auto *m = valid ? instanceMetadata[pixel.instanceId-1] : nullptr;
			// Sources animate independently of cached surface fields.
			std::fill_n(emission.begin()+i*4, 4, 0);
			if (m && m->hasEmission) std::copy_n(m->emissionRgba.begin()+pixel.tilePixel*4, 4, emission.begin()+i*4);
			depths[i] = m ? m->emissionDepth : 0;
			omnidirectional[i] = m && m->emissionOmnidirectional ? 1.0f : 0.0f;
			if (!full && key == keys_[i] && (!valid || !dirty[pixel.instanceId-1])) continue;
			keys_[i] = key; updatedPixels++;
			occlusion[i*2] = occlusion[i*2+1] = 0;
			height[i*2] = height[i*2+1] = 0;
			participation[i*2] = participation[i*2+1] = hud ? 0 : 255;
			opposite[i] = authoredNormals[i] = 0;
			std::fill_n(surface.begin()+i*4, 4, 0.0f);
			std::fill_n(reflectance.begin()+i*4, 4, 0.0f);
			samples[i] = {};
			if (!valid) continue;
			const auto &instance = frame.tileInstances[pixel.instanceId-1];
			auto material = instance.material.empty() ? materials.end() : materials.find(instance.material);
			if (m && m->hasMaterialSelectors && !m->materialSelectors[pixel.tilePixel].empty()) material = materials.find(m->materialSelectors[pixel.tilePixel]);
			const auto *mat = material == materials.end() ? nullptr : material->second;
			RemasterSurfaceMesh::classify(samples[i], instance.source, instance.sourceIndex,
				mat ? mat->surfaceClass : RemasterSurfaceClass::Unclassified, instance.assetGroup, pixel.instanceId);
			samples[i].wallBase = instance.heightOffset / 255.0f * frame.lightingCoordinateScale;
			if (mat && mat->surfaceClass == RemasterSurfaceClass::UserInterface) participation[i*2] = participation[i*2+1] = 0;
			else if (mat && !mat->receivesGi) participation[i*2+1] = 0;
			if (mat && mat->hasDiffuseReflectance)
			{
				std::copy(mat->diffuseReflectance.begin(), mat->diffuseReflectance.end(), reflectance.begin()+i*4);
				reflectance[i*4+3] = 1;
			}
			occlusion[i*2] = m && m->hasOcclusion ? m->occlusion[pixel.tilePixel] : 255;
			occlusion[i*2+1] = 255;
			opposite[i] = m && m->directLightingOppositeFacing ? 255 : 0;
			if ((m && m->hasHeight) || instance.hasPlacementHeight || instance.heightOffset)
			{
				const int local = instance.hasPlacementHeight ? instance.placementHeight[pixel.tilePixel] : m && m->hasHeight ? m->height[pixel.tilePixel] : 0;
				height[i*2] = S9xRemasterSignedHeightField(instance.heightOffset, static_cast<uint8_t>(local)); height[i*2+1] = 1;
			}
			surface[i*4] = height[i*2] * frame.lightingCoordinateScale;
			if (m && m->hasNormals)
			{
				authoredNormals[i] = 1;
				float nx = m->normalXyz[pixel.tilePixel*3]/127.5f-1, ny = m->normalXyz[pixel.tilePixel*3+1]/127.5f-1, nz = m->normalXyz[pixel.tilePixel*3+2]/127.5f-1;
				if (normalMasks[pixel.instanceId-1] & (UINT64_C(1) << pixel.tilePixel)) nx = ny = nz = 1;
				S9xRemasterTransformNormalForTileInstance(instance, nx, ny, nz, m->normalIgnoreTileTransform);
				const float length = std::sqrt(nx*nx+ny*ny+nz*nz);
				surface[i*4+1] = length > 0.0001f ? nx/length : 0;
				surface[i*4+2] = length > 0.0001f ? ny/length : 0;
				surface[i*4+3] = length > 0.0001f ? nz/length : 1;
			}
			samples[i].height = surface[i*4]; samples[i].known = height[i*2+1] != 0 && participation[i*2] != 0;
			samples[i].coverage = occlusion[i*2]/255.0f;
		}
		instances_ = frame.tileInstances; normalMasks_ = std::move(normalMasks);
		metadata_.clear(); for (const auto &m : frame.assetMetadata) metadata_.emplace(m.tileId, m);
		materials_ = frame.materials; width_ = frame.width; height_ = frame.height; scale_ = frame.lightingCoordinateScale;
	}
private:
	uint32_t width_ = 0, height_ = 0;
	float scale_ = 0;
	std::vector<uint64_t> keys_, normalMasks_;
	std::vector<RemasterFrameTileInstance> instances_;
	std::map<RemasterTileContentId, RemasterFrameAssetMetadata> metadata_;
	std::vector<RemasterFrameMaterial> materials_;
};
#endif
