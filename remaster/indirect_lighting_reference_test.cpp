/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                 This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "indirect_lighting_reference.h"

#include <cmath>
#include <cstdio>

using namespace RemasterIndirectReference;

static bool near (double actual, double expected, double relativeTolerance = 1e-10)
{
	return std::isfinite(actual) && std::abs(actual - expected) <=
		1e-12 + std::abs(expected) * relativeTolerance;
}

static bool near (const Rgb &actual, const Rgb &expected, double relativeTolerance = 1e-10)
{
	return near(actual.r, expected.r, relativeTolerance) &&
		near(actual.g, expected.g, relativeTolerance) &&
		near(actual.b, expected.b, relativeTolerance);
}

static SurfacePatch patch (Vec3 position, Vec3 normal, Rgb reflectance, Rgb radiance = {})
{
	SurfacePatch result;
	result.position = position;
	result.normal = normal;
	result.reflectance = reflectance;
	result.previousRadiance = radiance;
	return result;
}

int main ()
{
	unsigned checks = 0;
	unsigned failures = 0;
	auto check = [&](const char *name, bool passed) {
		checks++;
		if (!passed)
		{
			failures++;
			std::fprintf(stderr, "FAIL %s\n", name);
		}
	};

	constexpr double pi = 3.14159265358979323846264338327950288;
	Scene pair;
	pair.width = 3;
	pair.height = 1;
	pair.patches.push_back(patch({ 0.5, 0.5, 0.0 }, { 1.0, 0.0, 0.0 },
		{ 0.8, 0.6, 0.4 }));
	pair.patches.push_back(patch({ 2.5, 0.5, 0.0 }, { -1.0, 0.0, 0.0 },
		{ 1.0, 1.0, 1.0 }, { 2.0, 1.0, 0.5 }));
	const Rgb expectedPair = { 0.8 * 2.0 / (4.0 * pi), 0.6 / (4.0 * pi),
		0.4 * 0.5 / (4.0 * pi) };
	check("finite patch Lambertian transfer", near(exhaustiveBounce(pair)[0], expectedPair));

	Scene floorWall;
	floorWall.width = 2;
	floorWall.height = 2;
	floorWall.patches.push_back(patch({ 0.5, 0.5, 0.0 }, { 1.0, 0.0, 1.0 },
		{ 0.7, 0.7, 0.7 }));
	floorWall.patches.push_back(patch({ 1.5, 0.5, 1.0 }, { -1.0, 0.0, -1.0 },
		{ 1.0, 1.0, 1.0 }, { 3.0, 3.0, 3.0 }));
	const Rgb floorBounce = exhaustiveBounce(floorWall)[0];
	check("neutral wall illuminates shadowed floor", floorBounce.r > 0.0 &&
		near(floorBounce.r, floorBounce.g) && near(floorBounce.g, floorBounce.b));

	Scene colored;
	colored.width = 2;
	colored.height = 2;
	colored.patches.push_back(patch({ 0.5, 0.5, 0.0 }, { 1.0, 0.0, 0.0 },
		{ 0.0, 0.0, 0.0 }, { 4.0, 1.0, 0.25 }));
	colored.patches.push_back(patch({ 1.5, 0.5, 0.0 }, { -1.0, 0.0, 0.0 },
		{ 0.25, 0.5, 0.75 }));
	colored.patches.push_back(patch({ 0.5, 1.5, 0.0 }, { 1.0, -1.0, 0.0 },
		{ 0.5, 0.5, 0.5 }));
	const std::vector<Rgb> firstOrder = exhaustiveBounce(colored);
	for (size_t index = 0; index < colored.patches.size(); index++)
		colored.patches[index].previousRadiance = firstOrder[index];
	const Rgb secondOrder = exhaustiveBounce(colored)[2];
	const double relayFactor = 0.5 * std::sqrt(0.5) / (2.0 * pi * pi);
	check("colored relay transfers expected hue", near(secondOrder,
		{ relayFactor, relayFactor * 0.5, relayFactor * 0.1875 }));

	Scene black = pair;
	black.patches[0].reflectance = {};
	check("black reflectance absorbs energy", near(exhaustiveBounce(black)[0], {}));

	Scene sidedness = pair;
	sidedness.patches[0].normal = { 0.0, 0.0, 1.0 };
	sidedness.patches[1].normal = { 0.0, 0.0, 1.0 };
	check("coplanar patches do not exchange", near(exhaustiveBounce(sidedness)[0], {}));
	sidedness = pair;
	sidedness.patches[0].normal = { -1.0, 0.0, 0.0 };
	check("back-facing receiver rejects transfer", near(exhaustiveBounce(sidedness)[0], {}));
	sidedness = pair;
	sidedness.patches[1].normal = { 1.0, 0.0, 0.0 };
	check("back-facing source rejects transfer", near(exhaustiveBounce(sidedness)[0], {}));

	Scene blocked = pair;
	blocked.blockers.push_back({ 1, 0, 2.0, 1.0, true });
	check("thin opaque blocker stops transfer", near(exhaustiveBounce(blocked)[0], {}));
	blocked.patches[0].position.z = 3.0;
	blocked.patches[1].position.z = 3.0;
	check("elevated path passes low blocker", near(exhaustiveBounce(blocked)[0], expectedPair));
	blocked = pair;
	blocked.blockers.push_back({ 1, 0, 2.0, 0.5, true });
	check("fractional blocker attenuates once", near(exhaustiveBounce(blocked)[0], expectedPair * 0.5));
	blocked = pair;
	blocked.blockers.push_back({ 1, 0, 0.0, 1.0, false });
	check("unknown-height blocker is conservative", near(exhaustiveBounce(blocked)[0], {}));

	Scene tiny;
	tiny.width = 2;
	tiny.height = 1;
	tiny.patches.push_back(patch({ 0.5, 0.5, 0.0 }, { 1.0, 0.0, 0.0 },
		{ 1.0, 1.0, 1.0 }));
	for (unsigned source = 0; source < 16; source++)
		tiny.patches.push_back(patch({ 1.5, 0.5, 0.0 }, { -1.0, 0.0, 0.0 },
			{ 0.0, 0.0, 0.0 }, source == 7 ? Rgb{ 1.0, 0.5, 0.25 } : Rgb{}));
	const Rgb exactTiny = exhaustiveBounce(tiny)[0];
	check("tiny bright patch appears in exhaustive result", exactTiny.r > 0.0);
	const std::vector<Rgb> deterministicA = sampledBounce(tiny, 16, 12345);
	const std::vector<Rgb> deterministicB = sampledBounce(tiny, 16, 12345);
	check("fixed seed is deterministic", near(deterministicA[0], deterministicB[0], 0.0));

	double previousVariance = std::numeric_limits<double>::infinity();
	for (unsigned sampleCount : { 4u, 8u, 16u })
	{
		constexpr unsigned ensembleSize = 8192;
		double mean = 0.0;
		double squareMean = 0.0;
		for (unsigned seed = 1; seed <= ensembleSize; seed++)
		{
			const double estimate = sampledBounce(tiny, sampleCount, seed)[0].r;
			mean += estimate;
			squareMean += estimate * estimate;
		}
		mean /= ensembleSize;
		squareMean /= ensembleSize;
		const double variance = squareMean - mean * mean;
		check("sampled ensemble preserves mean", near(mean, exactTiny.r, 0.035));
		check("increasing samples reduces variance", variance < previousVariance);
		previousVariance = variance;
	}

	std::printf("%u/%u checks passed; %u failed\n", checks - failures, checks, failures);
	return failures ? 1 : 0;
}
