#include "emitter_patches.h"
#include <cassert>
int main()
{
	RemasterFrame frame;
	frame.width = frame.height = 8;
	frame.mainPixels.resize(64); frame.subPixels.resize(64);
	std::vector<uint8_t> emission(256, 255), participation(128, 255);
	std::vector<float> surface(256, 0);
	std::vector<RemasterSurfaceMesh::Cell> mesh(64);
	for (unsigned i = 0; i < 64; i++) {
		frame.mainPixels[i].owner = 0x02000001;
		frame.mainPixels[i].instanceId = 1;
		frame.mainPixels[i].tilePixel = i;
		surface[i*4+3] = 1;
	}
	auto build = [&] { return S9xRemasterEmitterPatches(frame, emission.data(), participation.data(), mesh); };
	// Disabling representatives always retains one source per emitting pixel.
	auto singletons = build();
	assert(singletons.size() == 64);
	for (unsigned i = 0; i < singletons.size(); i++) {
		assert(singletons[i].count == 1 && singletons[i].members[0] == i);
	}
	emission[3] = 0;
	auto hole = build();
	for (const auto &p : hole) for (unsigned k = 0; k < p.count; k++) assert(p.members[k] != 0);
	emission[3] = 255;
	frame.mainPixels[0].instanceId = 2;
	assert(build()[0].count == 1);
	frame.mainPixels[0].instanceId = 1;
	emission[0] = 255; emission[1] = emission[2] = 0;
	assert(build()[0].count == 1);
	emission[1] = emission[2] = 255;
	mesh[0].emissionDepth = 2;
	assert(build()[0].count == 1);
	mesh[0].emissionDepth = 0;
	mesh[0].emissionOmnidirectional = 1;
	assert(build()[0].count == 1);
	frame.representativeEmitters = 1;
	auto mixedDirections = build();
	assert(mixedDirections.size() == 2);
	for (const auto &patch : mixedDirections)
		for (unsigned i = 1; i < patch.count; i++)
			assert(mesh[patch.members[i]].emissionOmnidirectional == mesh[patch.members[0]].emissionOmnidirectional);
	frame.representativeEmitters = 0;
	mesh[0].emissionOmnidirectional = 0;
	surface[0] = 0.5f;
	for (auto &corner : mesh[0].corners) corner = 0.5f;
	assert(build()[0].count == 1);
	surface[0] = 0;
	for (auto &corner : mesh[0].corners) corner = 0;
	surface[1] = 0.1f;
	assert(build()[0].count == 1);
	for (unsigned k = 1; k <= 8; k++) {
		frame.representativeEmitters = k;
		auto clusters = build(), repeated = build();
		assert(clusters.size() <= k && clusters.size() == repeated.size());
		std::array<unsigned,64> seen = {};
		for (unsigned c = 0; c < clusters.size(); c++) {
			assert(clusters[c].mode & 1u);
			assert(clusters[c].members == repeated[c].members);
			for (unsigned j = 0; j < clusters[c].count; j++) seen[clusters[c].members[j]]++;
		}
		for (unsigned n : seen) assert(n == 1);
		if (k == 1) assert(clusters[0].count == 64);
	}
	frame.representativeEmitters = 1;
	emission[3] = 0;
	assert(build()[0].count == 63);
	emission[3] = 255;
	frame.mainPixels[0].instanceId = 2;
	assert(build().size() == 2);
	frame.mainPixels[0].instanceId = 1;
	frame.subPixels[0] = frame.mainPixels[0];
	frame.mainPixels[0].instanceId = 0; frame.mainPixels[0].owner = 0;
	assert(build().size() == 2); // Selected main/sub channels cannot merge.
	mesh[0].emissionDepth = 2;
	frame.subPixels[0].tilePixel = 64;
	auto fallback = build();
	assert(fallback[0].count == 1 && fallback[0].mode == 3);
	// Four adjacent tile types: full 256-member shape, and sparse flame gaps.
	frame = RemasterFrame{}; frame.width = frame.height = 16;
	frame.representativeEmitters = 1;
	frame.mainPixels.resize(256); frame.subPixels.resize(256);
	frame.tileInstances.resize(4); frame.assetMetadata.resize(4);
	for (unsigned t = 0; t < 4; t++) {
		auto &instance = frame.tileInstances[t]; instance.tileId = {100+t,1,4};
		instance.source = RemasterSourceType::Background;
		frame.assetMetadata[t].tileId = instance.tileId;
		for (unsigned peer = 0; peer < 4; peer++) if (t != peer)
			frame.assetMetadata[t].combineEmitters.push_back({100+peer,1,4});
	}
	emission.assign(1024,255); participation.assign(512,255); surface.assign(1024,0); mesh.resize(256);
	for (unsigned i = 0; i < 256; i++) {
		unsigned x = i%16, y = i/16, t = x/8+(y/8)*2;
		frame.mainPixels[i] = {0x01000000,t+1,uint8_t((y%8)*8+x%8)};
	}
	std::vector<RemasterTileContentId> selected;
	for (auto &instance : frame.tileInstances) selected.push_back(instance.tileId);
	assert(S9xRemasterEmitterSelectionConnected(frame,selected));
	assert(build().size() == 1 && build()[0].count == 256);
	frame.representativeEmitters = 3;
	auto combined = build(); assert(combined.size() <= 3);
	std::array<unsigned,256> coverage = {};
	for (auto &p : combined) for (unsigned i = 0; i < p.count; i++) coverage[p.members[i]]++;
	for (auto c : coverage) assert(c == 1);
	frame.representativeEmitters = 1;
	for (unsigned i = 0; i < 256; i++) if (i%16 == 7 || i%16 == 8) emission[i*4+3] = 0;
	assert(build().size() == 1 && build()[0].count == 224);
	frame.assetMetadata[0].combineEmitters.clear(); // One-way metadata never joins.
	assert(build().size() == 2);
	frame.assetMetadata[1].combineEmitters.clear();
	frame.assetMetadata[2].combineEmitters.clear();
	frame.assetMetadata[3].combineEmitters.clear();
	assert(build().size() == 4);
	assert(!S9xRemasterEmitterSelectionConnected(frame,{selected[0],selected[3]})); // diagonal
	frame.tileInstances[1].sourceIndex = 1;
	assert(!S9xRemasterEmitterSelectionConnected(frame,{selected[0],selected[1]}));
	frame.tileInstances[1].sourceIndex = 0;
	frame.tileInstances[1].hFlip = true;
	for (auto &p : frame.mainPixels) if (p.instanceId == 2) p.tilePixel ^= 7;
	assert(S9xRemasterEmitterSelectionConnected(frame,{selected[0],selected[1]}));
	for (auto &p : frame.mainPixels) if (p.instanceId == 2 && p.tilePixel != 7) p = RemasterFramePixel{};
	assert(S9xRemasterEmitterSelectionConnected(frame,{selected[0],selected[1]})); // clipped origin
	// Repeated matching types at separate placements never join remotely.
	frame = RemasterFrame{}; frame.width = 40; frame.height = 8; frame.representativeEmitters = 1;
	frame.mainPixels.resize(320); frame.subPixels.resize(320); frame.tileInstances.resize(4);
	frame.assetMetadata.resize(2);
	for (unsigned t = 0; t < 4; t++) {
		frame.tileInstances[t].tileId = selected[t%2]; frame.tileInstances[t].source = RemasterSourceType::Background;
	}
	for (unsigned t = 0; t < 2; t++) { frame.assetMetadata[t].tileId = selected[t]; frame.assetMetadata[t].combineEmitters = {selected[1-t]}; }
	emission.assign(1280,0); participation.assign(640,255); surface.assign(1280,0); mesh.resize(320);
	for (unsigned i = 0; i < 320; i++) {
		unsigned x = i%40, y = i/40; if (x/8 == 2) continue;
		unsigned t = x/8-(x >= 24 ? 1 : 0);
		frame.mainPixels[i] = {0x01000000,t+1,uint8_t(y*8+x%8)};
		if (x%8 == 3) for (unsigned c = 0; c < 4; c++) emission[i*4+c] = 255;
	}
	assert(build().size() == 2);
	// Combining a current shape authors every captured frame, including frames
	// absent from the current viewport. Broad semantic groups must not override
	// the observed animation track.
	const RemasterTileContentId animatedA = {200,1,4}, animatedB = {201,1,4};
	RemasterFrameAssetGroup groupA; groupA.name = "capture_animation_1";
	groupA.tileIds = {selected[0], animatedA};
	RemasterFrameAssetGroup groupB; groupB.name = "capture_animation_2";
	groupB.tileIds = {selected[1], animatedB};
	frame.assetGroups = {groupA, groupB};
	RemasterFrameAssetGroup semantic; semantic.name = "all_torches";
	semantic.tileIds = selected;
	frame.assetGroups.push_back(semantic);
	for (auto &instance : frame.tileInstances) instance.assetGroup = semantic.name;
	auto expanded = S9xRemasterEmitterSelectionAnimationVariants(frame, {selected[0],selected[1]});
	assert(expanded.size() == 4);
	frame.assetMetadata.clear();
	for (const auto &tile : expanded) {
		RemasterFrameAssetMetadata metadata; metadata.tileId = tile;
		for (const auto &peer : expanded) if (!(tile == peer)) metadata.combineEmitters.push_back(peer);
		frame.assetMetadata.push_back(metadata);
	}
	for (unsigned phase = 0; phase < 4; phase++) {
		for (unsigned t = 0; t < 4; t++) frame.tileInstances[t].tileId = t%2 ?
			(phase & 2 ? animatedB : selected[1]) : (phase & 1 ? animatedA : selected[0]);
		auto patches = build();
		assert(patches.size() == 2); // independent fixtures, even when out of phase
		unsigned members = 0;
		for (const auto &patch : patches) members += patch.count;
		assert(members == 32);
	}
	assert(S9xRemasterEmitterSelectionAnimationVariants(frame, {selected[2]}) ==
		std::vector<RemasterTileContentId>{selected[2]}); // static tile remains itself
}
