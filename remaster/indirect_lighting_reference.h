/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                 This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#ifndef REMASTER_INDIRECT_LIGHTING_REFERENCE_H
#define REMASTER_INDIRECT_LIGHTING_REFERENCE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace RemasterIndirectReference
{

struct Vec3
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Rgb
{
	double r = 0.0;
	double g = 0.0;
	double b = 0.0;
};

struct SurfacePatch
{
	Vec3 position;
	Vec3 normal;
	Rgb reflectance;
	Rgb previousRadiance;
	double area = 1.0;
	bool participates = true;
	bool receivesGi = true;
};

struct Blocker
{
	int x = 0;
	int y = 0;
	double height = 0.0;
	double coverage = 1.0;
	bool hasKnownHeight = true;
};

struct Scene
{
	int width = 0;
	int height = 0;
	std::vector<SurfacePatch> patches;
	std::vector<Blocker> blockers;
};

inline Vec3 operator - (const Vec3 &left, const Vec3 &right)
{
	return { left.x - right.x, left.y - right.y, left.z - right.z };
}

inline double dot (const Vec3 &left, const Vec3 &right)
{
	return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline Vec3 normalized (const Vec3 &value)
{
	const double lengthSquared = dot(value, value);
	if (lengthSquared <= 0.0)
		return {};
	const double inverseLength = 1.0 / std::sqrt(lengthSquared);
	return { value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
}

inline Rgb operator + (const Rgb &left, const Rgb &right)
{
	return { left.r + right.r, left.g + right.g, left.b + right.b };
}

inline Rgb operator * (const Rgb &left, const Rgb &right)
{
	return { left.r * right.r, left.g * right.g, left.b * right.b };
}

inline Rgb operator * (const Rgb &value, double scale)
{
	return { value.r * scale, value.g * scale, value.b * scale };
}

inline Rgb &operator += (Rgb &left, const Rgb &right)
{
	left = left + right;
	return left;
}

// Mirrors represented 2.5D blocker semantics: reflection and occlusion remain
// independent, unknown blocker heights are conservative, and endpoint cells
// cannot shadow their own connection.
inline double visibility (const Scene &scene, const Vec3 &from, const Vec3 &to)
{
	constexpr double heightTolerance = 0.05;
	const Vec3 segment = to - from;
	int cellX = static_cast<int>(std::floor(from.x));
	int cellY = static_cast<int>(std::floor(from.y));
	const int endpointX = static_cast<int>(std::floor(to.x));
	const int endpointY = static_cast<int>(std::floor(to.y));
	const int stepX = segment.x > 0.0 ? 1 : segment.x < 0.0 ? -1 : 0;
	const int stepY = segment.y > 0.0 ? 1 : segment.y < 0.0 ? -1 : 0;
	const double infinity = std::numeric_limits<double>::infinity();
	const double deltaX = stepX ? std::abs(1.0 / segment.x) : infinity;
	const double deltaY = stepY ? std::abs(1.0 / segment.y) : infinity;
	const double edgeX = cellX + (stepX > 0 ? 1.0 : 0.0);
	const double edgeY = cellY + (stepY > 0 ? 1.0 : 0.0);
	double nextX = stepX ? (edgeX - from.x) / segment.x : infinity;
	double nextY = stepY ? (edgeY - from.y) / segment.y : infinity;
	double result = 1.0;

	while (cellX != endpointX || cellY != endpointY)
	{
		const double entry = std::min(nextX, nextY);
		if (entry >= 1.0)
			break;
		const bool crossX = nextX <= nextY;
		const bool crossY = nextY <= nextX;
		if (crossX)
		{
			cellX += stepX;
			nextX += deltaX;
		}
		if (crossY)
		{
			cellY += stepY;
			nextY += deltaY;
		}
		if (cellX == endpointX && cellY == endpointY)
			break;
		if (cellX < 0 || cellY < 0 || cellX >= scene.width || cellY >= scene.height)
			break;

		const double exit = std::min(1.0, std::min(nextX, nextY));
		const double entryHeight = from.z + segment.z * entry;
		const double exitHeight = from.z + segment.z * exit;
		const double rayHeight = std::min(entryHeight, exitHeight);
		for (const Blocker &blocker : scene.blockers)
		{
			if (blocker.x != cellX || blocker.y != cellY || blocker.coverage <= 0.0)
				continue;
			if (!blocker.hasKnownHeight || blocker.height > rayHeight + heightTolerance)
			{
				result *= 1.0 - std::max(0.0, std::min(1.0, blocker.coverage));
				if (result <= 0.0)
					return 0.0;
			}
		}
	}
	return result;
}

// Point-to-finite-patch Lambertian transfer. The source area is represented by
// one visible pixel by default; receiver area is not part of outgoing radiance.
inline double patchTransfer (const SurfacePatch &receiver, const SurfacePatch &source,
	double connectionVisibility)
{
	constexpr double pi = 3.14159265358979323846264338327950288;
	const Vec3 toSource = source.position - receiver.position;
	const double distanceSquared = dot(toSource, toSource);
	if (distanceSquared <= 0.0 || source.area <= 0.0 || connectionVisibility <= 0.0)
		return 0.0;
	const Vec3 direction = normalized(toSource);
	const Vec3 receiverNormal = normalized(receiver.normal);
	const Vec3 sourceNormal = normalized(source.normal);
	const double receiverCosine = std::max(0.0, dot(receiverNormal, direction));
	const double sourceCosine = std::max(0.0, -dot(sourceNormal, direction));
	return connectionVisibility * source.area * receiverCosine * sourceCosine /
		(pi * distanceSquared);
}

inline Rgb connectionContribution (const Scene &scene, const SurfacePatch &receiver,
	const SurfacePatch &source)
{
	const double transfer = patchTransfer(receiver, source,
		visibility(scene, receiver.position, source.position));
	return (receiver.reflectance * source.previousRadiance) * transfer;
}

inline std::vector<Rgb> exhaustiveBounce (const Scene &scene)
{
	std::vector<Rgb> result(scene.patches.size());
	for (size_t receiverIndex = 0; receiverIndex < scene.patches.size(); receiverIndex++)
	{
		const SurfacePatch &receiver = scene.patches[receiverIndex];
		if (!receiver.participates || !receiver.receivesGi)
			continue;
		for (size_t sourceIndex = 0; sourceIndex < scene.patches.size(); sourceIndex++)
		{
			if (sourceIndex == receiverIndex || !scene.patches[sourceIndex].participates)
				continue;
			result[receiverIndex] += connectionContribution(scene, receiver,
				scene.patches[sourceIndex]);
		}
	}
	return result;
}

inline uint32_t nextRandom (uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

// Uniform source sampling is intentionally simple: this is an independent
// normalization/convergence oracle, not the future production proposal mixture.
inline std::vector<Rgb> sampledBounce (const Scene &scene, unsigned samplesPerReceiver,
	uint32_t seed)
{
	std::vector<Rgb> result(scene.patches.size());
	if (samplesPerReceiver == 0)
		return result;
	for (size_t receiverIndex = 0; receiverIndex < scene.patches.size(); receiverIndex++)
	{
		const SurfacePatch &receiver = scene.patches[receiverIndex];
		if (!receiver.participates || !receiver.receivesGi)
			continue;
		std::vector<size_t> sources;
		for (size_t sourceIndex = 0; sourceIndex < scene.patches.size(); sourceIndex++)
			if (sourceIndex != receiverIndex && scene.patches[sourceIndex].participates)
				sources.push_back(sourceIndex);
		if (sources.empty())
			continue;

		uint32_t randomState = seed ^ (static_cast<uint32_t>(receiverIndex) * 0x9e3779b9u);
		if (randomState == 0)
			randomState = 0x6d2b79f5u;
		Rgb sum;
		for (unsigned sample = 0; sample < samplesPerReceiver; sample++)
		{
			const size_t sourceIndex = sources[nextRandom(randomState) % sources.size()];
			sum += connectionContribution(scene, receiver, scene.patches[sourceIndex]) *
				static_cast<double>(sources.size());
		}
		result[receiverIndex] = sum * (1.0 / samplesPerReceiver);
	}
	return result;
}

} // namespace RemasterIndirectReference

#endif
