#include "direct_batches.h"
#include <cassert>
#include <limits>

int main()
{
	constexpr size_t pixels = 256 * 224, budget = 64 * 1024 * 1024;
	for (uint32_t sources : {0u, 1u, 8u, 9u, 576u, 577u, 2048u, 2560u, 4096u,
		std::numeric_limits<uint32_t>::max()})
	{
		const auto b = S9xRemasterDirectBatches(pixels, sources);
		assert(b.samples >= 8 && b.bytes <= budget);
		assert(uint64_t(b.count) * b.samples >= sources);
		assert(!sources || uint64_t(b.count - 1) * b.samples < sources);
		assert(b.bytes == pixels * 16 * b.count);
	}
	assert(S9xRemasterDirectBatches(pixels, 576).samples == 8);
	assert(S9xRemasterDirectBatches(pixels, 584).samples == 8);
	assert(S9xRemasterDirectBatches(pixels, 585).samples == 9);
	assert(S9xRemasterDirectBatches(pixels, 2560).samples == 36);
	assert(S9xRemasterDirectBatches(0, 2560).bytes == 0);
	assert(S9xRemasterDirectBatches(budget, 2560).bytes == 0);
}
