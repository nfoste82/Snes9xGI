#ifndef REMASTER_LIGHTING_FIELD_CACHE_H
#define REMASTER_LIGHTING_FIELD_CACHE_H

#include "frame.h"
#include "held_lamp.h"
#include "surface_mesh_material.h"
#include <map>

#define REMASTER_FIELD_MATERIAL_WORK 1

// Exact frame-local input comparison. No assumptions about static game artwork;
// emission is independent of geometry and transient instance IDs remain domains.
struct RemasterLightingFieldCache
{
	std::vector<uint8_t> occlusion, emission, participation, opposite, authoredNormals;
	// authoredNormals is the historical renderer override bit (explicit normal,
	// including semantic geometry). Separate provenance prevents inventory tools
	// from counting a proved geometric +Z as human-authored metadata.
	std::vector<uint8_t> semanticNormals;
	std::vector<float> height, surface, reflectance, depths, omnidirectional;
	std::vector<RemasterSurfaceMesh::Sample> samples;
	// Runtime diagnostics: exact observed per-channel donors, including absence.
	std::vector<RemasterVisibleChannelSelection> channelSources;
	// Same donor domains as channelSources; no claim is invented for absent channels.
	std::vector<std::array<RemasterVisualClaim,size_t(RemasterMetadataChannel::Count)>> visualClaims;
	size_t updatedPixels = 0;
	size_t selectedPixels = 0;
	size_t resolvedInstances = 0;
#ifdef REMASTER_FIELD_COUNT_WORK
	size_t materialLookups = 0;
#endif
	// Multi-flag exact dependency observations; diagnostic only, never cache policy.
	enum Reason : uint64_t { Forced=1, Dimensions=2, SceneMetadata=4, Material=8,
		ContentGeometry=16, EmissionMetadata=32, InstanceIdentity=64, Context=128,
		Palette=256, Raster=512, CompositionDepth=1024, NormalMask=2048, Cold=4096 };
	uint64_t reasons = 0;

	static bool geometryEqual(const RemasterFrameAssetMetadata &a, const RemasterFrameAssetMetadata &b)
	{
		if (a.geometryByFacing != b.geometryByFacing || a.geometryByFlip != b.geometryByFlip) return false;
		for (unsigned f = 0; f < 8; f++)
		{
			const auto &x = f < 4 ? a.facingGeometry[f] : a.flipGeometry[f - 4];
			const auto &y = f < 4 ? b.facingGeometry[f] : b.flipGeometry[f - 4];
			if (x.hasHeight != y.hasHeight || x.height != y.height || x.hasNormals != y.hasNormals ||
				x.normalXyz != y.normalXyz || x.heightSampling != y.heightSampling) return false;
		}
		return a.tileRole == b.tileRole && a.heightSampling == b.heightSampling &&
			a.hasDoorframeMask==b.hasDoorframeMask && a.doorframeMask==b.doorframeMask &&
			a.hasMaterialSelectors == b.hasMaterialSelectors && a.materialSelectors == b.materialSelectors &&
			a.hasOcclusion == b.hasOcclusion && a.occlusion == b.occlusion &&
			a.hasHeight == b.hasHeight && a.height == b.height && a.hasNormals == b.hasNormals &&
			a.normalXyz == b.normalXyz && a.directLightingOppositeFacing == b.directLightingOppositeFacing &&
			a.normalIgnoreTileTransform == b.normalIgnoreTileTransform;
	}
	static bool instanceEqual(const RemasterFrameTileInstance &a, const RemasterFrameTileInstance &b)
	{
		return a.visualClaim == b.visualClaim && a.tileId == b.tileId && a.source == b.source && a.sourceIndex == b.sourceIndex &&
			a.heightOffset == b.heightOffset && a.hasPlacementHeight == b.hasPlacementHeight &&
			a.placementHeight == b.placementHeight && a.hFlip == b.hFlip && a.vFlip == b.vFlip &&
			a.geometryStatus == b.geometryStatus && a.placementTileRole == b.placementTileRole &&
			a.hasPlacementNormals == b.hasPlacementNormals && a.placementNormalXyz == b.placementNormalXyz &&
			a.hasPlacementOcclusion == b.hasPlacementOcclusion && a.placementOcclusion == b.placementOcclusion &&
			a.placementNormalIgnoreTileTransform == b.placementNormalIgnoreTileTransform &&
			a.normalYaw == b.normalYaw && a.geometryFacing == b.geometryFacing && a.assetGroup == b.assetGroup && a.material == b.material;
	}
	static bool materialEqual(const RemasterFrameMaterial &a, const RemasterFrameMaterial &b)
	{
		return a.name == b.name && a.surfaceClass == b.surfaceClass && a.receivesGi == b.receivesGi &&
			a.hasDiffuseReflectance == b.hasDiffuseReflectance && a.diffuseReflectance == b.diffuseReflectance;
	}
	void update(const RemasterFrame &frame, bool useScene, bool forceFull = false)
	{
		RemasterLightingTrace::Scope selectionTimer(RemasterLightingTrace::current(),RemasterLightingTrace::Selection);
		const size_t count = frame.mainPixels.size();
		auto *trace=RemasterLightingTrace::current();
		const bool diagnose=trace && trace->id;
		reasons=0;
		if(diagnose) {
			if(forceFull)reasons|=Forced;
			if(width_!=frame.width || height_!=frame.height || scale_!=frame.lightingCoordinateScale || keys_.size()!=count)reasons|=Dimensions;
			if(rom_!=frame.profileRomSha256 || area_!=frame.area.id || scene_!=useScene)reasons|=SceneMetadata;
			if(!rasterValid_)reasons|=Cold;
			if(materials_.size()!=frame.materials.size())reasons|=Material;
			else for(size_t i=0;i<materials_.size();++i)if(!materialEqual(materials_[i],frame.materials[i])){reasons|=Material;break;}
			if(originalColors_!=frame.originalRgb555)reasons|=Palette;
			if(instances_.size()!=frame.tileInstances.size())reasons|=InstanceIdentity;
		}
		bool full = forceFull || width_ != frame.width || height_ != frame.height || scale_ != frame.lightingCoordinateScale || keys_.size() != count;
		full = full || rom_ != frame.profileRomSha256 || area_ != frame.area.id || scene_ != useScene;
		bool selectionFull = full || !rasterValid_;
		bool emissionDirty = forceFull || originalColors_ != frame.originalRgb555;
		if (materials_.size() != frame.materials.size()) full = true;
		else for (size_t i = 0; i < materials_.size(); i++) if (!materialEqual(materials_[i], frame.materials[i])) { full = true; break; }
		selectionFull = selectionFull || full || instances_.size()!=frame.tileInstances.size();
		if (keys_.size() != count)
		{
			keys_.resize(count); occlusion.resize(count*2); emission.resize(count*4);
			participation.resize(count*2); opposite.resize(count); authoredNormals.resize(count);
			height.resize(count*2); surface.resize(count*4); reflectance.resize(count*4);
			semanticNormals.resize(count);
			depths.resize(count); omnidirectional.resize(count); samples.resize(count); channelSources.resize(count);
			visualClaims.resize(count);
		}
		std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
		for (const auto &m : frame.assetMetadata) metadata[m.tileId] = &m;
		// HUD classification intentionally uses FIRST original metadata, whereas
		// field donors use LAST metadata. Do not merge those producer precedences.
		// A changed first-match selector is uncommon and conservatively global.
		std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> hudMetadata;
		for (const auto &m : frame.assetMetadata) hudMetadata.emplace(m.tileId, &m);
		bool hudDirty = false;
		for(const auto &entry:hudMetadata) {
			const auto old=hudMetadata_.find(entry.first);
			if(entry.second->hasMaterialSelectors ? old==hudMetadata_.end() || old->second!=entry.second->materialSelectors :
				old!=hudMetadata_.end()) hudDirty=true;
		}
		for(const auto &entry:hudMetadata_)if(!hudMetadata.count(entry.first))hudDirty=true;
		selectionFull = selectionFull || hudDirty;
		std::map<std::string, const RemasterFrameMaterial *> materials;
		for (const auto &m : frame.materials) materials[m.name] = &m;
		std::vector<const RemasterFrameAssetMetadata *> instanceMetadata(frame.tileInstances.size());
		effective_.resize(frame.tileInstances.size());
		resolvedInstances = 0;
		std::vector<bool> dirty(frame.tileInstances.size(), full);
		std::vector<uint64_t> normalMasks(frame.tileInstances.size());
		std::map<RemasterTileContentId, bool> metadataDirty;
		for (const auto &entry : metadata)
		{
			const auto old = metadata_.find(entry.first);
			metadataDirty[entry.first] = old == metadata_.end() || !geometryEqual(old->second, *entry.second) ||
				old->second.hasEmission != entry.second->hasEmission;
			if(diagnose && metadataDirty[entry.first])reasons|=ContentGeometry;
			if(diagnose && (old==metadata_.end() || old->second.hasEmission!=entry.second->hasEmission ||
				old->second.emissionRgba!=entry.second->emissionRgba || old->second.emissionColorMask!=entry.second->emissionColorMask ||
				old->second.emissionDepth!=entry.second->emissionDepth || old->second.emissionOmnidirectional!=entry.second->emissionOmnidirectional))reasons|=EmissionMetadata;
			// Presence is channel policy: absent and authored zero are distinct.
			if(old==metadata_.end()) emissionDirty=true;
			else {
				const auto &a=old->second,&b=*entry.second;
				emissionDirty = emissionDirty || a.hasEmission!=b.hasEmission || a.emissionRgba!=b.emissionRgba ||
					a.emissionColorMask!=b.emissionColorMask || a.emissionDepth!=b.emissionDepth || a.emissionOmnidirectional!=b.emissionOmnidirectional;
			}
		}
		for(const auto &entry:metadata_)if(!metadata.count(entry.first))emissionDirty=true;
		if(diagnose && metadata_.size()!=metadata.size())reasons|=ContentGeometry|EmissionMetadata;
		for (size_t i = 0; i < frame.tileInstances.size(); i++)
		{
			const auto &instance = frame.tileInstances[i];
			if(diagnose) {
				if(i>=instances_.size() || !instanceEqual(instance,instances_[i]))reasons|=InstanceIdentity;
				if(i>=instances_.size() || instance.hasPaletteArtwork!=instances_[i].hasPaletteArtwork || instance.paletteRgb555!=instances_[i].paletteRgb555)reasons|=Palette;
			}
			emissionDirty = emissionDirty || i>=instances_.size() ||
				instance.hasPaletteArtwork!=instances_[i].hasPaletteArtwork || instance.paletteRgb555!=instances_[i].paletteRgb555;
			const auto found = metadata.find(instance.tileId);
			const auto *original = found == metadata.end() ? nullptr : found->second;
			const bool contextual = instance.geometryStatus != RemasterOccurrenceGeometryStatus::Inherit ||
				instance.hasPlacementHeight || instance.hasPlacementNormals || instance.hasPlacementOcclusion || frame.contextGeometry.count(i+1);
			const auto context=frame.contextGeometry.find(i+1);
			const auto oldContext=contexts_.find(i+1);
			const bool contextDirty=(context==frame.contextGeometry.end()) != (oldContext==contexts_.end()) ||
				(context!=frame.contextGeometry.end() && oldContext!=contexts_.end() && !contextEqual(context->second,oldContext->second));
			const bool sourceDirty=i>=instances_.size() || !instanceEqual(instance,instances_[i]) ||
				(original ? metadataDirty[instance.tileId] : metadata_.count(instance.tileId)!=0) || contextDirty;
			if(diagnose && contextDirty)reasons|=Context;
			const auto *m = original;
			if (contextual || (original && (original->geometryByFacing || original->geometryByFlip)))
			{
				// Resolved geometry is occurrence-local. Keep static BG resolution
				// separate from animation emission, which is refreshed independently.
				if(sourceDirty || !effective_[i] || forceFull) {
					if(!effective_[i]) effective_[i]=std::make_unique<RemasterFrameAssetMetadata>();
					*effective_[i]=S9xRemasterResolveFrameGeometry(original ? *original : RemasterFrameAssetMetadata{},frame,i);
					++resolvedInstances;
				}
				if(original) {
					effective_[i]->hasEmission=original->hasEmission;
					effective_[i]->emissionRgba=original->emissionRgba;
					effective_[i]->emissionColorMask=original->emissionColorMask;
					effective_[i]->emissionDepth=original->emissionDepth;
					effective_[i]->emissionOmnidirectional=original->emissionOmnidirectional;
				}
				m = effective_[i].get();
			}
			instanceMetadata[i] = m;
			if (instance.hasPlacementHeight && instance.geometryStatus == RemasterOccurrenceGeometryStatus::Inherit && original)
			{
				// This legacy correction compares authored caps to replacement relief;
				// comparing effective relief to itself would erase the diagnostic mask.
				if(!sourceDirty && i<normalMasks_.size() && !forceFull) normalMasks[i]=normalMasks_[i];
				else {
					auto baselineInstance = instance; baselineInstance.hasPlacementHeight = false;
					const auto baseline = S9xRemasterResolveInstanceGeometry(*original, baselineInstance);
					normalMasks[i] = S9xRemasterContinuedDiagonalNormalMask(instance, &baseline);
				}
			}
			dirty[i] = full || sourceDirty ||
				i >= normalMasks_.size() || normalMasks[i] != normalMasks_[i];
			if(diagnose && (i>=normalMasks_.size() || normalMasks[i]!=normalMasks_[i]))reasons|=NormalMask;
		}
		contexts_=frame.contextGeometry;
		// Presence depends only on resolved occurrence metadata and material,
		// not on pixel coordinates. Compute once per update instead of repeatedly
		// chasing the same large metadata for every channel of every candidate.
		// These masks expire with this invocation; donor precedence and every
		// preferred/fallback candidate still run through the shared selector.
		std::vector<uint8_t> channelPresence(instanceMetadata.size());
		for(size_t i=0;i<instanceMetadata.size();++i)if(const auto *m=instanceMetadata[i]) {
			channelPresence[i]=uint8_t(m->hasHeight) | (uint8_t(m->hasNormals)<<1) |
				(uint8_t(m->hasOcclusion)<<2) | (uint8_t(m->hasEmission)<<3) |
				(uint8_t(m->hasMaterialSelectors || !frame.tileInstances[i].material.empty())<<4) |
				(uint8_t(m->hasDoorframeMask)<<5) | (uint8_t(m->tileRole!=RemasterTileRole::Unknown)<<6) |
				(uint8_t(m->tileRole==RemasterTileRole::OutOfBounds)<<7);
		}
		const bool alttp=frame.profileRomSha256=="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
		auto present=[&](const RemasterFramePixel &p,size_t c) {
			return p.instanceId && p.instanceId<=channelPresence.size() && p.tilePixel<64 &&
				(channelPresence[p.instanceId-1] & (1u<<c));
		};
		updatedPixels = 0;
		selectedPixels = 0;
		auto samePixel = [](const RemasterFramePixel &a,const RemasterFramePixel &b) {
			return a.owner==b.owner && a.instanceId==b.instanceId && a.tilePixel==b.tilePixel;
		};
		// Exact observed inputs, not capture counters or actor coordinates.
		// Shape changes are global; same-shape composition and source changes are
		// checked locally below, including candidates which were NOT selected.
		const std::vector<RemasterFramePixel> *rasters[7] = {&frame.mainPixels,&frame.subPixels,&frame.scenePixels,
			&frame.observedBgPixels[0],&frame.observedBgPixels[1],&frame.observedBgPixels[2],&frame.observedBgPixels[3]};
		if(diagnose) {
			for(size_t r=0;r<7;++r) {
				const auto &a=*rasters[r],&b=rasters_[r];
				if(a.size()!=b.size() || !std::equal(a.begin(),a.end(),b.begin(),samePixel))reasons|=Raster;
			}
			if(depths_!=frame.observedBgDepth)reasons|=CompositionDepth;
			trace->fieldReasons=reasons;
		}
		bool selectionDirty = std::find(dirty.begin(),dirty.end(),true)!=dirty.end();
		for(size_t r=0;!selectionFull && r<7;r++) {
			const auto &now=*rasters[r], &old=rasters_[r];
			selectionFull = now.size()!=old.size();
			if(!selectionFull && !selectionDirty)selectionDirty=!std::equal(now.begin(),now.end(),old.begin(),samePixel);
		}
		for(size_t r=0;r<4;++r)selectionFull=selectionFull || depths_[r].size()!=frame.observedBgDepth[r].size();
		selectionDirty=selectionDirty || depths_!=frame.observedBgDepth;
		// Lighting/camera/exposure and light motion do not alter any field input.
		// An exact unchanged observation has no per-pixel work at all. In
		// particular, do not overwrite the renderer's derived normal components.
		if(!selectionFull && !selectionDirty && !emissionDirty) return;
		// Material names and selectors are occurrence inputs, not pixel inputs.
		// Resolve against the same LAST-name material map once per invocation.
		// Selector tables are shared only by identical resolved metadata pointers;
		// contextual metadata remains occurrence-local. Empty default material means
		// absent, whereas an explicit empty selector still looks up the empty name.
#ifdef REMASTER_FIELD_COUNT_WORK
		materialLookups=0;
#endif
		auto findMaterial=[&](const std::string &name) -> const RemasterFrameMaterial * {
#ifdef REMASTER_FIELD_COUNT_WORK
			++materialLookups;
#endif
			const auto found=materials.find(name);
			return found==materials.end()?nullptr:found->second;
		};
		using MaterialTable=std::array<const RemasterFrameMaterial *,64>;
		std::map<const RemasterFrameAssetMetadata *,MaterialTable> selectorMaterials;
		std::vector<const MaterialTable *> instanceSelectors(instanceMetadata.size());
		std::vector<const RemasterFrameMaterial *> instanceMaterials(instanceMetadata.size());
		for(size_t i=0;i<instanceMetadata.size();++i) {
			const auto &name=frame.tileInstances[i].material;
			if(!name.empty())instanceMaterials[i]=findMaterial(name);
			const auto *m=instanceMetadata[i];
			if(m && m->hasMaterialSelectors) {
				auto inserted=selectorMaterials.emplace(m,MaterialTable{});
				if(inserted.second)for(size_t p=0;p<64;++p)inserted.first->second[p]=findMaterial(m->materialSelectors[p]);
				instanceSelectors[i]=&inserted.first->second;
			}
		}
		const RemasterHeldLampHudLookup hudLookup(frame);
		for (size_t i = 0; i < count; i++)
		{
			bool select = selectionFull;
			if(!select && selectionDirty) {
				// Every possible preferred/fallback reference is in these seven
				// buffers. Compare BOTH old and new IDs (including invalid IDs and
				// same-size index reuse), not just the previous channel donors.
				for(size_t r=0;r<7 && !select;++r)if(i<rasters[r]->size()) {
					const auto &a=(*rasters[r])[i], &b=rasters_[r][i];
					auto changed=[&](const RemasterFramePixel &p) {return p.instanceId && p.instanceId<=dirty.size() && dirty[p.instanceId-1];};
					select=!samePixel(a,b) || changed(a) || changed(b);
				}
				for(size_t r=0;r<4 && !select;++r)if(i<depths_[r].size())select=depths_[r][i]!=frame.observedBgDepth[r][i];
			}
			// No dependency of this offset changed. Retain fields, claims and
			// renderer-written derived normals verbatim. Emission-only changes
			// deliberately still visit every offset (no speculative local cut).
			if(!select && !emissionDirty)continue;
			const bool hud = hudLookup(i);
			if(select) ++selectedPixels;
			const auto selection = select ? S9xRemasterSelectVisibleChannelsWithPresence(frame,i,useScene,present,false,alttp) : channelSources[i];
			const auto &hp = selection.donors[size_t(RemasterMetadataChannel::Height)];
			const auto &np = selection.donors[size_t(RemasterMetadataChannel::Normals)];
			const auto &op = selection.donors[size_t(RemasterMetadataChannel::Occlusion)];
			const auto &ep = selection.donors[size_t(RemasterMetadataChannel::Emission)];
			const auto &mp = selection.donors[size_t(RemasterMetadataChannel::Material)];
			const auto &rp = selection.donors[size_t(RemasterMetadataChannel::Role)];
			// Geometry classification/base belong to the height donor, not artwork.
			const auto &pixel = hp.instanceId ? hp : selection.visible;
			// A HUD overlay is not world geometry. A separately rasterized world
			// pixel beneath it still casts shadows, but never receives/presents GI.
			const bool worldBehindHud = hud && useScene && pixel.owner != frame.mainPixels[i].owner &&
				!(static_cast<RemasterSourceType>(pixel.owner >> 24) == RemasterSourceType::Background &&
				  ((pixel.owner >> 16) & 255u) == 2);
			const bool valid = (!hud || worldBehindHud) && pixel.instanceId && pixel.instanceId <= frame.tileInstances.size() && pixel.tilePixel < 64;
			const bool backdropGround = !hud && S9xRemasterFrameBackdropGroundAt(frame, i, useScene);
			const uint64_t key = valid ? (uint64_t(hud) << 63) | (uint64_t(pixel.instanceId) << 8) | pixel.tilePixel :
				hud ? UINT64_MAX : backdropGround ? UINT64_MAX - 1 : 0;
			const auto *m = valid ? instanceMetadata[pixel.instanceId-1] : nullptr;
			// Sources animate independently of cached surface fields.
			std::fill_n(emission.begin()+i*4, 4, 0);
			const auto *em = ep.instanceId ? instanceMetadata[ep.instanceId-1] : nullptr;
			if (!hud && em) {
				const auto &instance=frame.tileInstances[ep.instanceId-1];
				const auto rgba=S9xRemasterEmissionSample(*em,ep.tilePixel,instance.hasPaletteArtwork ?
					instance.paletteRgb555[ep.tilePixel] : frame.originalRgb555[i]);
				std::copy(rgba.begin(),rgba.end(),emission.begin()+i*4);
			}
			depths[i] = em ? em->emissionDepth : 0;
			omnidirectional[i] = em && em->emissionOmnidirectional ? 1.0f : 0.0f;
			bool channelsDirty = !samePixel(selection.visible,channelSources[i].visible);
			for(size_t c=0;c<selection.donors.size();c++) {
				const auto &p=selection.donors[c];
				channelsDirty = channelsDirty || !samePixel(p,channelSources[i].donors[c]) || (p.instanceId && dirty[p.instanceId-1]);
			}
			channelSources[i]=selection;
			for(size_t c=0;c<selection.donors.size();c++)visualClaims[i][c]=S9xRemasterPixelVisualClaim(frame,selection.donors[c]);
			if (!full && !channelsDirty && key == keys_[i] && (!valid || !dirty[pixel.instanceId-1])) continue;
			keys_[i] = key; updatedPixels++;
			occlusion[i*2] = occlusion[i*2+1] = 0;
			height[i*2] = height[i*2+1] = 0;
			participation[i*2] = participation[i*2+1] = hud ? 0 : 255;
			opposite[i] = authoredNormals[i] = 0;
			semanticNormals[i] = 0;
			std::fill_n(surface.begin()+i*4, 4, 0.0f);
			std::fill_n(reflectance.begin()+i*4, 4, 0.0f);
			samples[i] = {};
			if (backdropGround)
			{
				// Same provisional domain as unauthored BG ground at base zero.
				// No tile identity, authored normal, material or emission is invented.
				height[i*2+1] = 1;
				occlusion[i*2] = occlusion[i*2+1] = 255;
				surface[i*4+3] = 1;
				samples[i].known = samples[i].sheet = samples[i].fallbackGround = true;
				samples[i].coverage = 1;
				samples[i].domain = uint64_t(3) << 48;
			}
			if (!valid) continue;
			const auto &instance = frame.tileInstances[pixel.instanceId-1];
			const auto *mat=instanceMaterials[pixel.instanceId-1];
			if (mp.instanceId) {
				const auto *selectors=instanceSelectors[mp.instanceId-1];
				mat=selectors?(*selectors)[mp.tilePixel]:instanceMaterials[mp.instanceId-1];
			}
			RemasterSurfaceMesh::classify(samples[i], instance.source, instance.sourceIndex,
				mat ? mat->surfaceClass : RemasterSurfaceClass::Unclassified, instance.assetGroup, pixel.instanceId,
				rp.instanceId ? instanceMetadata[rp.instanceId-1]->tileRole : RemasterTileRole::Unknown,
				instance.geometryStatus != RemasterOccurrenceGeometryStatus::Inherit);
			samples[i].wallBase = instance.heightOffset / 255.0f * frame.lightingCoordinateScale;
			if(m && m->tileRole==RemasterTileRole::OutOfBounds && instance.visualClaim.published)
				samples[i].domain=(uint64_t(4)<<48)|uint32_t(instance.visualClaim.component+1);
			if (mat && mat->surfaceClass == RemasterSurfaceClass::UserInterface) participation[i*2] = participation[i*2+1] = 0;
			else if (mat && !mat->receivesGi) participation[i*2+1] = 0;
			if (mat && mat->hasDiffuseReflectance)
			{
				std::copy(mat->diffuseReflectance.begin(), mat->diffuseReflectance.end(), reflectance.begin()+i*4);
				reflectance[i*4+3] = 1;
			}
			const bool unresolved = S9xRemasterInstanceGeometryUnresolved(instance);
			const bool terminalUnknown = m && m->tileRole==RemasterTileRole::OutOfBounds &&
				!S9xRemasterPixelVisualClaim(frame,pixel).published;
			// Provisional visible BG ground, never a claim of solved/authored geometry.
			// Explicit unresolved placements and UI remain absent. A placement base
			// alone is not relief: use a flat sheet there rather than a prop shell.
			const bool fallbackGround = !unresolved && !terminalUnknown && instance.source == RemasterSourceType::Background &&
				(participation[i*2] != 0 || worldBehindHud) && !(m && m->hasHeight);
			if (fallbackGround)
			{
				samples[i].fallbackGround = true;
				samples[i].sheet = true;
				samples[i].solidWall = false;
				// Isolate provisional flats from authored ramps and other floor bases.
				samples[i].domain = (uint64_t(3) << 48) | uint32_t(instance.heightOffset);
			}
			occlusion[i*2] = unresolved ? 0 : op.instanceId ? instanceMetadata[op.instanceId-1]->occlusion[op.tilePixel] : 255;
			occlusion[i*2+1] = unresolved ? 0 : 255;
			opposite[i] = m && m->directLightingOppositeFacing ? 255 : 0;
			// A grounded OBJ assembly may be authored progressively. Its support
			// datum is not a local flat surface for members without height arrays.
			const bool missingSpriteRelief = instance.source == RemasterSourceType::Object &&
				instance.geometryStatus == RemasterOccurrenceGeometryStatus::Resolved &&
				instance.placementTileRole == RemasterTileRole::Obstacle && !(m && m->hasHeight);
			if (!unresolved && !terminalUnknown && !missingSpriteRelief && ((m && m->hasHeight) || instance.heightOffset || fallbackGround))
			{
				const int local = m && m->hasHeight ? m->height[pixel.tilePixel] : 0;
				height[i*2] = S9xRemasterSignedHeightField(instance.heightOffset, static_cast<int16_t>(local)); height[i*2+1] = 1;
			}
			surface[i*4] = height[i*2] * frame.lightingCoordinateScale;
			if (fallbackGround) surface[i*4+3] = 1;
			if (np.instanceId)
			{
				const auto *nm=instanceMetadata[np.instanceId-1];
				const auto &ni=frame.tileInstances[np.instanceId-1];
				authoredNormals[i] = 1;
				semanticNormals[i] = nm->tileRole==RemasterTileRole::OutOfBounds && ni.visualClaim.published && ni.hasPlacementNormals;
				float nx = nm->normalXyz[np.tilePixel*3]/127.5f-1, ny = nm->normalXyz[np.tilePixel*3+1]/127.5f-1, nz = nm->normalXyz[np.tilePixel*3+2]/127.5f-1;
				if (normalMasks[np.instanceId-1] & (UINT64_C(1) << np.tilePixel)) nx = ny = nz = 1;
				S9xRemasterTransformNormalForTileInstance(ni, nx, ny, nz, nm->normalIgnoreTileTransform);
				opposite[i] = nm->directLightingOppositeFacing ? 255 : 0;
				const float length = std::sqrt(nx*nx+ny*ny+nz*nz);
				surface[i*4+1] = length > 0.0001f ? nx/length : 0;
				surface[i*4+2] = length > 0.0001f ? ny/length : 0;
				surface[i*4+3] = length > 0.0001f ? nz/length : 1;
			}
			samples[i].height = surface[i*4]; samples[i].known = height[i*2+1] != 0 && (participation[i*2] != 0 || worldBehindHud);
			samples[i].coverage = occlusion[i*2]/255.0f;
		}
		instances_ = frame.tileInstances; normalMasks_ = std::move(normalMasks);
		for(size_t r=0;r<7;r++) rasters_[r]=*rasters[r];
		depths_=frame.observedBgDepth; rasterValid_=true;
		rom_=frame.profileRomSha256; area_=frame.area.id; scene_=useScene;
		originalColors_=frame.originalRgb555;
		// Snapshot the actual last-match field producer (emplace would retain
		// the first duplicate and can miss a subsequent switchback).
		metadata_.clear(); for (const auto &m : frame.assetMetadata) metadata_[m.tileId]=m;
		hudMetadata_.clear();for(const auto &entry:hudMetadata)if(entry.second->hasMaterialSelectors)
			hudMetadata_.emplace(entry.first,entry.second->materialSelectors);
		materials_ = frame.materials; width_ = frame.width; height_ = frame.height; scale_ = frame.lightingCoordinateScale;
	}
private:
	static bool contextEqual(const RemasterTileContext &a,const RemasterTileContext &b) {
		return a.height==b.height && a.hasHeight==b.hasHeight && a.heightSampling==b.heightSampling &&
			a.normalXyz==b.normalXyz && a.hasNormals==b.hasNormals &&
			a.normalIgnoreTileTransform==b.normalIgnoreTileTransform && a.directLightingOppositeFacing==b.directLightingOppositeFacing;
	}
	std::vector<std::unique_ptr<RemasterFrameAssetMetadata>> effective_;
	std::map<uint32_t,RemasterTileContext> contexts_;
	bool rasterValid_ = false, scene_ = false;
	std::string rom_;
	uint32_t area_ = 0;
	std::array<std::vector<RemasterFramePixel>,7> rasters_;
	std::array<std::vector<uint8_t>,4> depths_;
	std::vector<uint16_t> originalColors_;
	uint32_t width_ = 0, height_ = 0;
	float scale_ = 0;
	std::vector<uint64_t> keys_, normalMasks_;
	std::vector<RemasterFrameTileInstance> instances_;
	std::map<RemasterTileContentId, RemasterFrameAssetMetadata> metadata_;
	std::map<RemasterTileContentId, std::array<std::string,64>> hudMetadata_;
	std::vector<RemasterFrameMaterial> materials_;
};
#endif
