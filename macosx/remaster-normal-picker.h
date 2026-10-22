#ifndef SNES9X_REMASTER_NORMAL_PICKER_H
#define SNES9X_REMASTER_NORMAL_PICKER_H

#include <algorithm>
#include <array>
#include <cmath>

// Change tilt without changing azimuth. Keep the caller's saved XY direction
// even at a pole, where the resulting vector has no XY component.
inline std::array<float, 3> RemasterNormalPickerTilt(float directionX, float directionY,
	float z, float delta, bool back)
{
	const float length = std::hypot(directionX, directionY);
	if (length > 0.0f)
	{
		directionX /= length;
		directionY /= length;
	}
	else
	{
		// A pure Z normal has no azimuth; use +X until one is chosen.
		directionX = 1.0f;
		directionY = 0.0f;
	}
	const float magnitude = std::max(0.0f, std::min(1.0f, std::abs(z) + delta));
	const float radius = std::sqrt(std::max(0.0f, 1.0f - magnitude * magnitude));
	return {directionX * radius, directionY * radius, back ? -magnitude : magnitude};
}

// Hold one component exactly; move the other within the remaining unit disk.
inline std::array<float, 3> RemasterNormalPickerAxisLock(float x, float y,
	float deltaX, float deltaY, bool lockX, bool lockY, bool back)
{
	if (!lockX)
	{
		const float limit = std::sqrt(std::max(0.0f, 1.0f - y * y));
		x = std::max(-limit, std::min(limit, x + deltaX));
	}
	if (!lockY)
	{
		const float limit = std::sqrt(std::max(0.0f, 1.0f - x * x));
		y = std::max(-limit, std::min(limit, y + deltaY));
	}
	const float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
	return {x, y, back ? -z : z};
}

#endif
