// Shared production HUD classifier and renderer scene-color pass regression/benchmark.
// clang++ -std=c++17 -O2 -DNDEBUG remaster/tests/held_lamp_hud_lookup_test.cpp -o <probe>
// <probe> [capture.s9xrmf ...]; no profile reload or capture mutation.
#include "../runtime/held_lamp.h"
#include "../runtime/lighting_field_cache.h"
#include <chrono>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace {
constexpr const char *alttp = "66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
void require(bool value, const std::string &message) {
	if (!value) throw std::runtime_error(message);
}

// Deliberately wrong test-only controls. The normal test never uses this prototype.
#if defined(HUD_LOOKUP_LAST_METADATA) || defined(HUD_LOOKUP_LAST_MATERIAL)
class PrototypeNegativeControlHud {
	struct Decision { uint64_t selectors = 0; bool fallback = false; };
	const RemasterFrame &frame;
	bool enabled;
	std::vector<Decision> decisions;
public:
	explicit PrototypeNegativeControlHud(const RemasterFrame &f) : frame(f),
		enabled(f.profileRomSha256 == alttp && f.width == 256) {
		if (!enabled) return;
		std::map<RemasterTileContentId, const RemasterFrameAssetMetadata *> metadata;
		std::unordered_map<std::string, bool> materials;
		materials.reserve(f.materials.size());
		for (const auto &m : f.assetMetadata) {
#ifdef HUD_LOOKUP_LAST_METADATA
			metadata[m.tileId] = &m; // deliberate negative-control mutation
#else
			metadata.emplace(m.tileId, &m); // linear oracle's FIRST match
#endif
		}
		for (const auto &m : f.materials) {
#ifdef HUD_LOOKUP_LAST_MATERIAL
			materials[m.name] = m.surfaceClass == RemasterSurfaceClass::UserInterface;
#else
			materials.emplace(m.name, m.surfaceClass == RemasterSurfaceClass::UserInterface);
#endif
		}
		const auto isUi = [&](const std::string &name) {
			const auto found = materials.find(name);
			return found != materials.end() && found->second;
		};
		decisions.reserve(f.tileInstances.size());
		for (const auto &instance : f.tileInstances) {
			Decision d;
			d.fallback = isUi(instance.material);
			d.selectors = d.fallback ? UINT64_MAX : 0;
			const auto found = metadata.find(instance.tileId);
			if (found != metadata.end() && found->second->hasMaterialSelectors) {
				const auto &selectors = found->second->materialSelectors;
				for (unsigned p = 0; p < 64; ++p) if (!selectors[p].empty()) {
					const uint64_t bit = UINT64_C(1) << p;
					if (isUi(selectors[p])) d.selectors |= bit;
					else d.selectors &= ~bit;
				}
			}
			decisions.push_back(d);
		}
	}
	bool operator()(size_t pixel) const {
		if (!enabled || pixel >= size_t(frame.width) * frame.height ||
			pixel >= frame.mainPixels.size()) return false;
		const auto &p = frame.mainPixels[pixel];
		if (static_cast<RemasterSourceType>(p.owner >> 24) == RemasterSourceType::Background &&
			((p.owner >> 16) & 255u) == 2) return true;
		if (!p.instanceId || p.instanceId > decisions.size()) return false;
		const auto &d = decisions[p.instanceId - 1];
		return p.tilePixel < 64 ? bool(d.selectors & (UINT64_C(1) << p.tilePixel)) : d.fallback;
	}
};
#endif

#if defined(HUD_LOOKUP_LAST_METADATA) || defined(HUD_LOOKUP_LAST_MATERIAL)
using IndexedHud = PrototypeNegativeControlHud; // wrong test-only implementations
#else
using IndexedHud = RemasterHeldLampHudLookup;
#endif

size_t compare(const RemasterFrame &frame, const std::string &label) {
	const IndexedHud indexed(frame);
	const size_t limit = std::max(frame.mainPixels.size(), size_t(frame.width) * frame.height) + 3;
	size_t hud = 0;
	for (size_t p = 0; p < limit; ++p) {
		const bool expected = S9xRemasterHeldLampHudPixel(frame, p);
		require(indexed(p) == expected, label + ": predicate mismatch offset=" + std::to_string(p));
		hud += expected;
	}
	require(!indexed(std::numeric_limits<size_t>::max()), label + ": SIZE_MAX");
	return hud;
}

void originalPass(const RemasterFrame &, std::vector<uint16_t> &);
void indexedPass(const RemasterFrame &, std::vector<uint16_t> &);

void fieldConsumer() {
	auto f = std::make_unique<RemasterFrame>();
	f->profileRomSha256 = alttp; f->width = 256; f->height = 1;
	f->mainPixels.resize(256); f->originalRgb555.assign(256, 0);
	RemasterFrameTileInstance instance; instance.tileId.hash = 77; instance.material = "world";
	instance.source = RemasterSourceType::Background;
	f->tileInstances.push_back(instance);
	RemasterFrameMaterial material; material.name = "world";
	f->materials.push_back(material);
	material.name = "ui"; material.surfaceClass = RemasterSurfaceClass::UserInterface;
	f->materials.push_back(material);
	RemasterFrameAssetMetadata meta; meta.tileId = instance.tileId;
	meta.hasHeight = meta.hasEmission = meta.hasMaterialSelectors = true;
	meta.height.fill(0); meta.emissionRgba.fill(25); meta.materialSelectors[0] = "ui";
	f->assetMetadata.push_back(meta);
	for (size_t p = 0; p < 256; ++p) f->mainPixels[p] = {0x02000000, 1, uint8_t(p % 64)};
	f->mainPixels[2] = {0x02020000, 99999, 255}; // BG3 bypass before invalid identity
	RemasterLightingFieldCache cache;
	for (unsigned step = 0; step < 5; ++step) {
		if (step == 2) f->assetMetadata[0].emissionRgba.fill(50); // emission-only invocation
		if (step == 3) f->mainPixels[0].tilePixel = 1; // ownership overwrite
		if (step == 4) { // frame growth is safe only with a freshly constructed lookup
			f->tileInstances.push_back(instance); f->mainPixels[1].instanceId = 2;
			f->assetMetadata[0].materialSelectors[1] = "ui";
		}
		cache.update(*f, false);
		if (step == 1) require(cache.selectedPixels == 0 && cache.updatedPixels == 0, "unchanged field early return");
		for (size_t p = 0; p < 256; ++p) {
			const bool hud = S9xRemasterHeldLampHudPixel(*f, p); // independent original oracle
			require(cache.height[p*2+1] == (hud ? 0 : 1), "production field HUD/valid authored zero");
			require(cache.emission[p*4] == (hud ? 0 : (step >= 2 ? 50 : 25)), "production field HUD emission");
		}
	}
	std::puts("production field consumer PASS (cold/warm/emission/overwrite/growth, original predicate oracle)");
}

void synthetic() {
	auto f = std::make_unique<RemasterFrame>();
	f->profileRomSha256 = alttp; f->width = 256; f->height = 2;
	f->mainPixels.resize(520); // valid vector offsets outside declared dimensions
	f->tileInstances.resize(4);
	f->tileInstances[0].tileId.hash = 1; f->tileInstances[0].material = "world";
	f->tileInstances[1].tileId.hash = 2; f->tileInstances[1].material = "ui";
	f->tileInstances[2].tileId.hash = 3; f->tileInstances[2].material = "missing";
	f->tileInstances[3].tileId.hash = 4; // empty fallback name is legal
	RemasterFrameMaterial m;
	m.name = "ui"; m.surfaceClass = RemasterSurfaceClass::UserInterface; f->materials.push_back(m);
	m.surfaceClass = RemasterSurfaceClass::Unclassified; f->materials.push_back(m); // duplicate
	m.name = "world"; f->materials.push_back(m);
	m.surfaceClass = RemasterSurfaceClass::UserInterface; f->materials.push_back(m); // duplicate reversed
	m.name = ""; f->materials.push_back(m);
	RemasterFrameAssetMetadata meta;
	meta.tileId = f->tileInstances[0].tileId; meta.hasMaterialSelectors = true;
	meta.materialSelectors[0] = "ui"; meta.materialSelectors[63] = "ui";
	meta.materialSelectors[1] = "missing"; f->assetMetadata.push_back(meta);
	meta.materialSelectors[0] = "world"; meta.materialSelectors[63] = "world";
	f->assetMetadata.push_back(meta); // duplicate tileId must not replace first
	meta.tileId = f->tileInstances[1].tileId;
	meta.materialSelectors[0] = "world"; meta.materialSelectors[1] = "missing";
	f->assetMetadata.push_back(meta);
	meta.tileId = f->tileInstances[2].tileId; meta.hasMaterialSelectors = false;
	meta.materialSelectors[0] = "ui"; f->assetMetadata.push_back(meta);
	for (size_t p = 0; p < f->mainPixels.size(); ++p) {
		auto &pixel = f->mainPixels[p];
		pixel.instanceId = uint32_t(p / 64 % 8); // zero, all valid, too large
		pixel.tilePixel = uint8_t(p % 64);
		pixel.owner = 0x03000000;
	}
	compare(*f, "duplicates/selectors/zero/absent");
	require(S9xRemasterHeldLampHudPixel(*f, 64), "independent first metadata/material expectation");
	require(!S9xRemasterHeldLampHudPixel(*f, 65), "missing selected material does not fall back");
	require(!S9xRemasterHeldLampHudPixel(*f, 66), "empty selector uses world first match");
	require(!S9xRemasterHeldLampHudPixel(*f, 128), "selector overrides UI fallback");
	require(S9xRemasterHeldLampHudPixel(*f, 130), "empty selector uses UI fallback");
	require(S9xRemasterHeldLampHudPixel(*f, 256), "empty material first match");
	f->sceneRgb555.assign(520, 0x1234); f->originalRgb555.assign(520, 0x4567);
	std::vector<uint16_t> original, indexed;
	originalPass(*f, original); indexedPass(*f, indexed);
	require(original == indexed, "synthetic exact output bytes");
	require(indexed[64] == 0x4567 && indexed[65] == 0x1234 && indexed[512] == 0x1234,
		"independent synthetic replacement / non-HUD / out-of-dimension bytes");
	// Incomplete color packets must never read beyond the original color plane.
	f->originalRgb555.resize(65); indexedPass(*f, indexed);
	require(indexed[64] == 0x4567 && indexed[128] == 0x1234, "short original color plane");
	f->originalRgb555.clear(); indexedPass(*f, indexed);
	require(indexed == f->sceneRgb555, "missing original color plane");
	// Metadata edits and ownership overwrite are observed on the next invocation.
	f->assetMetadata[0].materialSelectors[0] = "world";
	compare(*f, "same-frame metadata edit");
	f->assetMetadata[0].materialSelectors[0] = "ui";
	f->mainPixels[64].instanceId = 2;
	compare(*f, "overwritten owner instance");
	f->mainPixels[64].instanceId = 1;
	for (unsigned tp : {0u, 63u, 64u, 255u}) {
		for (size_t p = 0; p < f->mainPixels.size(); ++p) f->mainPixels[p].tilePixel = uint8_t(tp);
		compare(*f, "tilePixel " + std::to_string(tp));
	}
	for (uint32_t owner : {0x02020000u, 0x0202ffffu, 0x02010000u, 0x03020000u, 0xffffffffu, 0u}) {
		for (auto &p : f->mainPixels) p.owner = owner;
		compare(*f, "source/layer flags " + std::to_string(owner));
	}
	// Transform/context/RGB are not inputs to the predicate; canonical tilePixel is.
	for (auto &i : f->tileInstances) { i.hFlip = i.vFlip = true; i.sourceIndex = 2; i.hasPaletteArtwork = true; }
	f->originalRgb555.assign(520, 0x7fff); f->sceneRgb555.assign(520, 0);
	compare(*f, "irrelevant flip/source/palette/RGB");
	f->assetMetadata.clear(); compare(*f, "all metadata absent");
	f->materials[0].surfaceClass = RemasterSurfaceClass::Unclassified;
	compare(*f, "same-frame material update, fresh invocation");
	f->materials.erase(f->materials.begin()); compare(*f, "material removal");
	f->tileInstances.clear(); compare(*f, "no instances");
	f->mainPixels.assign(520, {0x02020000, 999999, 255});
	compare(*f, "BG3 unknown identity bypass");
	f->profileRomSha256 = "not ALTTP"; compare(*f, "non ALTTP including BG3");
	f->profileRomSha256 = alttp;
	for (unsigned w : {0u, 255u, 512u}) { f->width = w; compare(*f, "wrong width"); }
	f->width = 256; f->height = 0; compare(*f, "zero height");
	f->height = 2; f->mainPixels.resize(5); compare(*f, "short main plane");
	f->mainPixels.clear(); compare(*f, "missing main plane");
	std::puts("synthetic parity PASS (fresh invocation after every mutation; checks active with NDEBUG)");
}

// Deliberately identical workload and retained output allocation. Candidate construction
// AND destruction occur inside every timed invocation, with no retained index capacity.
__attribute__((noinline)) void originalPass(const RemasterFrame &f, std::vector<uint16_t> &out) {
	out = f.sceneRgb555;
	for (size_t p = 0; p < out.size(); ++p)
		if (S9xRemasterHeldLampHudPixel(f, p)) out[p] = f.originalRgb555[p];
}
__attribute__((noinline)) void indexedPass(const RemasterFrame &f, std::vector<uint16_t> &out) {
	S9xRemasterPrepareHeldLampSceneColors(f, out);
}
uint64_t checksum(const std::vector<uint16_t> &out) {
	uint64_t h = UINT64_C(14695981039346656037);
	for (uint16_t p : out) for (unsigned shift : {0u, 8u}) {
		h ^= (p >> shift) & 255; h *= UINT64_C(1099511628211);
	}
	return h;
}
double elapsed(void (*pass)(const RemasterFrame &, std::vector<uint16_t> &),
	const RemasterFrame &f, std::vector<uint16_t> &out) {
	const auto start = std::chrono::steady_clock::now();
	pass(f, out);
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
void fieldTiming(RemasterFrame &f) {
	RemasterLightingFieldCache cache;
	cache.update(f, true);
	std::vector<double> warm, changed, emission;
	for (unsigned n = 0; n < 60; ++n) {
		for (unsigned mode = 0; mode < 3; ++mode) {
			if (mode == 1) f.tileInstances[0].heightOffset += 1; // changed-frame pixel loop
			if (mode == 2) f.originalRgb555[0] ^= 1; // emission-only pixel loop
			const auto start = std::chrono::steady_clock::now();
			cache.update(f, true);
			const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			(mode == 0 ? warm : mode == 1 ? changed : emission).push_back(ms);
		}
	}
	std::sort(warm.begin(), warm.end()); std::sort(changed.begin(), changed.end()); std::sort(emission.begin(), emission.end());
	std::printf("field_update_median_ms warm=%.6f changed=%.6f emission_only=%.6f iterations=60\n", warm[30], changed[30], emission[30]);
}
void capture(const char *path) {
	auto frame = std::make_unique<RemasterFrame>();
	require(S9xReadRemasterFrame(path, *frame), std::string("cannot read capture: ") + path);
	const auto &f = *frame;
	const size_t hud = compare(f, path);
	require(f.sceneRgb555.size() == f.originalRgb555.size() && !f.sceneRgb555.empty(),
		"capture has no matching real scene color plane");
	std::vector<uint16_t> original, indexed;
	for (unsigned n = 0; n < 20; ++n) { originalPass(f, original); indexedPass(f, indexed); }
	require(original == indexed, "warmup byte mismatch");
	std::vector<double> oldTimes, newTimes;
	for (unsigned n = 0; n < 300; ++n) {
		double a, b;
		if (n % 2) { b = elapsed(indexedPass, f, indexed); a = elapsed(originalPass, f, original); }
		else { a = elapsed(originalPass, f, original); b = elapsed(indexedPass, f, indexed); }
		require(original == indexed, "timed byte mismatch"); // outside timing, full output checked
		oldTimes.push_back(a); newTimes.push_back(b);
	}
	size_t replaced = 0;
	for (size_t p = 0; p < original.size(); ++p) replaced += original[p] != f.sceneRgb555[p];
	std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
	std::printf("capture=%s schema=%u dimensions=%ux%u main=%zu scene=%zu instances=%zu metadata=%zu materials=%zu HUD=%zu changed=%zu bytes=%zu fnv1a_le=%016llx parity=PASS\n",
		path, f.schemaVersion, f.width, f.height, f.mainPixels.size(), f.sceneRgb555.size(),
		f.tileInstances.size(), f.assetMetadata.size(), f.materials.size(), hud, replaced,
		original.size() * sizeof(uint16_t), (unsigned long long)checksum(original));
	std::printf("warmup=20 pairs=300 allocation=retained_output_fresh_index original_ms_p10/median/p90=%.6f/%.6f/%.6f indexed_ms_p10/median/p90=%.6f/%.6f/%.6f median_saved_ms=%.6f ratio=%.3f\n",
		oldTimes[30], oldTimes[150], oldTimes[270], newTimes[30], newTimes[150], newTimes[270],
		oldTimes[150] - newTimes[150], oldTimes[150] / newTimes[150]);
	fieldTiming(*frame); // disposable in-memory copy only, never writes captures
}
} // namespace

int main(int argc, char **argv) {
	try {
		synthetic();
		fieldConsumer();
		for (int n = 1; n < argc; ++n) capture(argv[n]);
		return 0;
	} catch (const std::exception &error) {
		std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
	}
}
