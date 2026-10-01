#ifndef REMASTER_SURFACE_MESH_H
#define REMASTER_SURFACE_MESH_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Compact, pixel-binned triangle mesh ABI shared with Metal. Corners run
// clockwise: NW, NE, SE, SW. Four triangles meet at the authored pixel center,
// so shading positions remain exactly on the generated surface.
namespace RemasterSurfaceMesh
{
struct Cell
{
	float corners[4] = {};
	float normal[3] = {0, 0, 1};
	float thickness = 1;
	float wallBase = 0;
	float solidWall = 0;
	float emissionDepth = 0;
	float reserved = 0;
};
static_assert(sizeof(Cell) == 48, "Metal mesh cell ABI");

struct Sample
{
	float height = 0;
	float coverage = 0;
	bool known = false;
	bool sheet = false;
	uint64_t domain = 0;
	bool solidWall = false;
	float wallBase = 0;
};

// Domains describe placement/object identity, not tile hashes. Floor and wall
// domains span draw records. Object domains must never join a background domain.
inline std::vector<Cell> build(unsigned width, unsigned height,
	const std::vector<Sample> &samples, float quantum)
{
	std::vector<Cell> result(samples.size());
	if (samples.size() != static_cast<size_t>(width) * height)
		return result;
	auto valid = [&](int x, int y) { return x >= 0 && y >= 0 && x < int(width) && y < int(height); };
	auto index = [&](int x, int y) { return size_t(y) * width + x; };
	auto same = [&](size_t a, size_t b) {
		return samples[a].known && samples[b].known && samples[a].coverage > 0 &&
			samples[b].coverage > 0 && samples[a].domain != 0 && samples[a].domain == samples[b].domain &&
			samples[a].sheet == samples[b].sheet;
	};
	std::vector<uint8_t> edges(samples.size(), 0);
	const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
	const unsigned opposite[] = {1, 0, 3, 2};
	const float tolerance = std::max(0.0001f, quantum * 1.05f);
	for (unsigned y = 0; y < height; y++)
		for (unsigned x = 0; x < width; x++)
		{
			const size_t a = index(x, y);
			for (unsigned d : {1u, 3u})
			{
				const int bx = int(x) + dx[d], by = int(y) + dy[d];
				if (!valid(bx, by)) continue;
				const size_t b = index(bx, by);
				if (!same(a, b)) continue;
				const float rise = samples[b].height - samples[a].height;
				bool continuous = std::abs(rise) <= std::max(0.0001f, quantum * 1.5f);
				// Larger ramps need continuation evidence, not a height threshold
				// that would bridge an arbitrary ledge or touching sprite pieces.
				for (int side : {-1, 1})
				{
					const int cx = side < 0 ? int(x) - dx[d] : bx + dx[d];
					const int cy = side < 0 ? int(y) - dy[d] : by + dy[d];
					if (!valid(cx, cy)) continue;
					const size_t c = index(cx, cy);
					if (same(side < 0 ? a : b, c))
					{
						const float continuation = side < 0 ? samples[a].height - samples[c].height :
							samples[c].height - samples[b].height;
						continuous |= std::abs(continuation - rise) <= tolerance;
					}
				}
				if (continuous) { edges[a] |= uint8_t(1 << d); edges[b] |= uint8_t(1 << opposite[d]); }
			}
		}
	// Reconstruct boundary corners with one-sided slopes; never shrink pixel
	// silhouettes. A separate weld pass handles each vertex's local components.
	for (unsigned y = 0; y < height; y++)
		for (unsigned x = 0; x < width; x++)
		{
			const size_t p = index(x, y);
			Cell &cell = result[p];
			cell.thickness = samples[p].sheet ? 0.0f : 1.0f;
			cell.solidWall = samples[p].solidWall ? 1.0f : 0.0f;
			cell.wallBase = samples[p].wallBase;
			float gradients[4] = {};
			for (unsigned d = 0; d < 4; d++) if (edges[p] & (1 << d))
			{
				gradients[d] = (samples[index(int(x) + dx[d], int(y) + dy[d])].height - samples[p].height) * (dx[d] + dy[d]);
			}
			auto slope = [&](unsigned a, unsigned b) {
				const bool hasA = edges[p] & (1 << a), hasB = edges[p] & (1 << b);
				if (!hasA) return hasB ? gradients[b] : 0.0f;
				if (!hasB) return gradients[a];
				// Monotone reconstruction: averaging a flat continuation with a
				// descending edge invents a raised ridge on the plateau. Minmod
				// preserves linear ramps but stops slopes at flats and extrema.
				if (gradients[a] * gradients[b] <= 0.0f) return 0.0f;
				return std::copysign(std::min(std::abs(gradients[a]), std::abs(gradients[b])), gradients[a]);
			};
			const float gx = slope(0, 1), gy = slope(2, 3);
			for (unsigned corner = 0; corner < 4; corner++)
			{
				const int sx = corner == 0 || corner == 3 ? -1 : 1;
				const int sy = corner < 2 ? -1 : 1;
				cell.corners[corner] = samples[p].height + (gx * sx + gy * sy) * 0.5f;
			}
		}
	for (unsigned vy = 0; vy <= height; vy++)
		for (unsigned vx = 0; vx <= width; vx++)
		{
			const int xs[] = {int(vx) - 1, int(vx), int(vx), int(vx) - 1};
			const int ys[] = {int(vy) - 1, int(vy) - 1, int(vy), int(vy)};
			const unsigned corners[] = {2, 3, 0, 1};
			unsigned parent[] = {0, 1, 2, 3};
			auto root = [&](unsigned i) { while (parent[i] != i) i = parent[i]; return i; };
			for (unsigned i = 0; i < 4; i++)
			{
				unsigned j = (i + 1) % 4;
				if (!valid(xs[i], ys[i]) || !valid(xs[j], ys[j])) continue;
				const unsigned direction = i == 0 ? 1 : i == 1 ? 3 : i == 2 ? 0 : 2;
				if (edges[index(xs[i], ys[i])] & (1 << direction)) parent[root(j)] = root(i);
			}
			float sum[4] = {}; unsigned count[4] = {};
			for (unsigned i = 0; i < 4; i++) if (valid(xs[i], ys[i]))
			{ sum[root(i)] += result[index(xs[i], ys[i])].corners[corners[i]]; count[root(i)]++; }
			for (unsigned i = 0; i < 4; i++) if (valid(xs[i], ys[i]))
				result[index(xs[i], ys[i])].corners[corners[i]] = sum[root(i)] / count[root(i)];
		}
	for (Cell &cell : result)
		{
			// Geometric normal is separate from authored corner/relief normals.
			float gx = (cell.corners[1] + cell.corners[2] - cell.corners[0] - cell.corners[3]) * 0.5f;
			float gy = (cell.corners[2] + cell.corners[3] - cell.corners[0] - cell.corners[1]) * 0.5f;
			const float inverseLength = 1 / std::sqrt(gx * gx + gy * gy + 1);
			cell.normal[0] = -gx * inverseLength; cell.normal[1] = -gy * inverseLength; cell.normal[2] = inverseLength;
		}
	return result;
}
}
#endif
