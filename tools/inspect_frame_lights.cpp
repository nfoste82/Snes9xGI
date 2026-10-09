#include "../remaster/frame.h"
#include "../remaster/sphere_samples.h"
#include <cstdio>
#include <memory>

// Packet-local GPU sphere workload; does not infer missing runtime state.
int main(int argc, char **argv)
{
	if (argc < 2) return 1;
	for (int n = 1; n < argc; n++)
	{
		auto frame = std::make_unique<RemasterFrame>();
		if (!S9xReadRemasterFrame(argv[n], *frame)) return 1;
		unsigned count = 0, samples = 0;
		std::printf("%s: %ux%u\n", argv[n], frame->width, frame->height);
		for (const auto &light : frame->lights)
		{
			if (light.radius <= 0 || light.intensity <= 0) continue;
			const unsigned rays = S9xRemasterSphereSamples(light.x, light.y, light.radius, frame->width, frame->height);
			count++; samples += rays;
			std::printf("  xy=%.2f,%.2f z=%.2f radius=%.2f intensity=%.2f samples=%u\n",
				light.x, light.y, light.z, light.radius, light.intensity, rays);
		}
		std::printf("  active spheres=%u adaptive samples=%u representative=%u\n", count, samples, frame->representativeEmitters);
	}
}
