#include "../runtime/lighting_field_cache.h"
#include "lighting_field_cache_serial_reference.h"
#include "../runtime/surface_mesh_cache.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>

template<class A, class B> void equal(const A &a, const B &b)
{
	assert(a.occlusion == b.occlusion && a.emission == b.emission && a.participation == b.participation);
	assert(a.opposite == b.opposite && a.authoredNormals == b.authoredNormals && a.semanticNormals == b.semanticNormals);
	assert(a.height == b.height && a.surface == b.surface && a.reflectance == b.reflectance);
	assert(a.depths == b.depths && a.omnidirectional == b.omnidirectional && a.visualClaims == b.visualClaims);
	assert(a.selectedPixels == b.selectedPixels && a.updatedPixels == b.updatedPixels);
	assert(a.channelSources.size() == b.channelSources.size());
	auto pixel = [](const RemasterFramePixel &x, const RemasterFramePixel &y) {
		return x.owner == y.owner && x.instanceId == y.instanceId && x.tilePixel == y.tilePixel;
	};
	for (size_t i = 0; i < a.samples.size(); ++i) {
		const auto &x = a.samples[i], &y = b.samples[i];
		assert(x.height == y.height && x.coverage == y.coverage && x.domain == y.domain && x.known == y.known &&
			x.sheet == y.sheet && x.solidWall == y.solidWall && x.wallBase == y.wallBase && x.fallbackGround == y.fallbackGround);
		assert(pixel(a.channelSources[i].visible,b.channelSources[i].visible));
		for (size_t c = 0; c < a.channelSources[i].donors.size(); ++c)
			assert(pixel(a.channelSources[i].donors[c],b.channelSources[i].donors[c]));
	}
}

int main(int argc, char **argv)
{
	RemasterLightingFieldCache parallel;
	RemasterLightingFieldCacheSerialReference serial;
	RemasterSurfaceMesh::Cache mesh;
	unsigned checked = 0;
	auto check = [&](const RemasterFrame &frame) {
		parallel.update(frame,!frame.scenePixels.empty()); serial.update(frame,!frame.scenePixels.empty());
		equal(parallel,serial);
		const auto actual = mesh.update(frame.width,frame.height,parallel.samples,frame.lightingCoordinateScale/255.f);
		const auto expected = RemasterSurfaceMesh::build(frame.width,frame.height,serial.samples,frame.lightingCoordinateScale/255.f);
		assert(actual.size() == expected.size() && !std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(actual[0])));
		++checked;
	};
	for (int i = 1; i < argc; ++i) {
		RemasterFrame frame; assert(S9xReadRemasterFrame(argv[i],frame)); check(frame);
	}
	RemasterFrame frame;
	frame.width = 256; frame.height = 224;
	const size_t count = frame.width * frame.height;
	frame.originalRgb555.assign(count,0x4210); frame.mainPixels.resize(count); frame.subPixels.resize(count);
	for (unsigned i = 0; i < 64; ++i) {
		RemasterFrameTileInstance instance; instance.tileId = {i+1,1,4};
		instance.source = RemasterSourceType::Background; instance.sourceIndex = 1;
		frame.tileInstances.push_back(instance);
		RemasterFrameAssetMetadata metadata; metadata.tileId = instance.tileId;
		metadata.hasHeight = metadata.hasNormals = metadata.hasEmission = true;
		metadata.height.fill(int16_t(i)); metadata.normalXyz.fill(128); metadata.emissionRgba.fill(64);
		frame.assetMetadata.push_back(metadata);
	}
	for (size_t i = 0; i < count; ++i) frame.mainPixels[i] = {0x02010000,uint32_t(i%64+1),uint8_t(i%64)};
	check(frame);
	for (unsigned n = 0; n < 100; ++n) {
		// Broad dirty raster/occurrence changes exercise eight workers; alternating
		// palette-only and isolated changes cover donor retention and serial work.
		if (n%4 == 0) for (auto &p : frame.mainPixels) p.tilePixel = (p.tilePixel+1)%64;
		if (n%4 == 1) for (auto &color : frame.originalRgb555) color ^= 0x421;
		if (n%4 == 2) frame.tileInstances[n%64].heightOffset++;
		if (n%4 == 3) frame.assetMetadata[n%64].emissionDepth += 0.25f;
		check(frame); check(frame);
	}
	std::mt19937 random(1972);
	const unsigned width = 128, height = 96;
	std::vector<RemasterSurfaceMesh::Sample> world(256*256), screen(width*height);
	for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x) {
		auto &sample = world[y*256+x]; sample.known = true; sample.coverage = 1;
		sample.height = float((x/11+y/13)%4); sample.domain = 1+x/32; sample.sheet = true;
	}
	for (unsigned n = 0; n < 400; ++n) {
		const unsigned ox = 32+n%30, oy = 40+(n/4)%35;
		for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
			screen[y*width+x] = world[(y+oy)*256+x+ox];
		for (unsigned j = 0; j < 12; ++j) {
			auto &sample = screen[random()%screen.size()]; sample.height = float(random()%8);
			sample.known = random()%2; sample.domain = random()%4;
			sample.solidWall = random()%2; sample.wallBase = float(random()%3);
		}
		const auto actual = mesh.update(width,height,screen,1);
		const auto expected = RemasterSurfaceMesh::build(width,height,screen,1);
		assert(actual.size() == expected.size() && !std::memcmp(actual.data(),expected.data(),actual.size()*sizeof(actual[0])));
		++checked;
	}
	std::cout << "PASS " << checked << " parallel/serial complete field states, donors, claims and counters\n";
}
