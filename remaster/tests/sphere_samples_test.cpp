#include "../runtime/sphere_samples.h"
#include <cassert>
#include <limits>

int main()
{
	assert(S9xRemasterSphereSamples(100, 100, 1, 256, 224) == 128);
	assert(S9xRemasterSphereSamples(256, 100, 1, 256, 224) == 128);
	assert(S9xRemasterSphereSamples(600, 100, 1, 256, 224) == 8);
	assert(S9xRemasterSphereSamples(-344, 100, 1, 256, 224) == 8);
	assert(S9xRemasterSphereSamples(300, 100, 1, 256, 224) == 16);
	assert(S9xRemasterSphereSamples(280, 100, 1, 256, 224) == 32);
	assert(S9xRemasterSphereSamples(270, 100, 1, 256, 224) == 64);
	assert(S9xRemasterSphereSamples(std::numeric_limits<float>::infinity(), 100, 1, 256, 224) == 128);
	assert(S9xRemasterSphereSamples(300, 100, 1, 0, 224) == 128);
}
