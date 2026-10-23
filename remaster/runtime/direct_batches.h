#ifndef REMASTER_DIRECT_BATCHES_H
#define REMASTER_DIRECT_BATCHES_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

struct RemasterDirectBatches
{
	uint32_t samples = 8;
	uint32_t count = 1;
	size_t bytes = 0;
};

// Keep exhaustive transport parallel when eight-source partials exceed the
// per-slot budget. Only summation grouping changes; every source is retained.
inline RemasterDirectBatches S9xRemasterDirectBatches(size_t pixels, uint32_t sources,
	size_t budget = 64 * 1024 * 1024)
{
	RemasterDirectBatches result;
	if (!pixels || pixels > budget / 16) return result;
	const size_t capacity = budget / (pixels * 16);
	result.samples = static_cast<uint32_t>(std::max<size_t>(8,
		size_t(sources) / capacity + (size_t(sources) % capacity != 0)));
	result.count = std::max(1u, sources / result.samples + uint32_t(sources % result.samples != 0));
	result.bytes = pixels * 16 * result.count;
	return result;
}

#endif
