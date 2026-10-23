#ifndef REMASTER_SURFACE_MESH_CACHE_H
#define REMASTER_SURFACE_MESH_CACHE_H

#include "surface_mesh.h"

namespace RemasterSurfaceMesh
{
// Screen-space reuse of resolved geometry, never an assertion that a tile is
// static. Each changed sample invalidates outputs within three pixels. Rebuild
// tiles with another three-pixel input halo to preserve continuation and welding.
class Cache
{
public:
	const std::vector<Cell> &update(unsigned width, unsigned height,
		const std::vector<Sample> &samples, float quantum)
	{
		if (samples.size() != size_t(width) * height)
		{
			previous_.clear(); width_ = height_ = 0; quantum_ = -1;
			cells_ = build(width, height, samples, quantum);
			rebuiltTiles = 0;
			return cells_;
		}
		constexpr unsigned tile = 16, halo = 3;
		const unsigned columns = (width + tile - 1) / tile, rows = (height + tile - 1) / tile;
		std::vector<uint8_t> dirty(size_t(columns) * rows, 0);
		bool full = width != width_ || height != height_ || quantum != quantum_ || samples.size() != previous_.size();
		if (!full)
			for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
			{
				const size_t i = size_t(y) * width + x;
				const Sample &a = samples[i], &b = previous_[i];
				if (a.height == b.height && a.known == b.known && (a.coverage > 0) == (b.coverage > 0) &&
					a.domain == b.domain && a.sheet == b.sheet && a.solidWall == b.solidWall && a.wallBase == b.wallBase)
					continue;
				const unsigned x0 = x > halo ? x - halo : 0, y0 = y > halo ? y - halo : 0;
				for (unsigned ty = y0 / tile; ty <= std::min(height - 1, y + halo) / tile; ty++)
					for (unsigned tx = x0 / tile; tx <= std::min(width - 1, x + halo) / tile; tx++) dirty[size_t(ty) * columns + tx] = 1;
			}
		rebuiltTiles = std::count(dirty.begin(), dirty.end(), uint8_t(1));
		full |= rebuiltTiles > dirty.size() / 2;
		if (full)
		{
			cells_ = build(width, height, samples, quantum);
			rebuiltTiles = dirty.size();
		}
		else
			for (unsigned ty = 0; ty < rows; ty++) for (unsigned tx = 0; tx < columns; tx++)
			{
				if (!dirty[size_t(ty) * columns + tx]) continue;
				const unsigned x0 = tx * tile, y0 = ty * tile;
				const unsigned x1 = std::min(width, x0 + tile), y1 = std::min(height, y0 + tile);
				const unsigned left = x0 > halo ? x0 - halo : 0, top = y0 > halo ? y0 - halo : 0;
				const unsigned right = std::min(width, x1 + halo), bottom = std::min(height, y1 + halo);
				patch_.resize(size_t(right - left) * (bottom - top));
				for (unsigned y = top; y < bottom; y++)
					std::copy(samples.begin() + size_t(y) * width + left, samples.begin() + size_t(y) * width + right,
						patch_.begin() + size_t(y - top) * (right - left));
				const auto rebuilt = build(right - left, bottom - top, patch_, quantum);
				for (unsigned y = y0; y < y1; y++)
					std::copy(rebuilt.begin() + size_t(y - top) * (right - left) + x0 - left,
						rebuilt.begin() + size_t(y - top) * (right - left) + x1 - left, cells_.begin() + size_t(y) * width + x0);
			}
		previous_ = samples;
		width_ = width; height_ = height; quantum_ = quantum;
		return cells_;
	}
	size_t rebuiltTiles = 0;
private:
	unsigned width_ = 0, height_ = 0;
	float quantum_ = -1;
	std::vector<Sample> previous_, patch_;
	std::vector<Cell> cells_;
};
}
#endif
