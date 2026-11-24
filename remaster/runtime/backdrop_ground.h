#ifndef S9X_REMASTER_BACKDROP_GROUND_H
#define S9X_REMASTER_BACKDROP_GROUND_H

#include "frame.h"

// A color-zero pixel has no raster owner. Recover only its placement datum,
// from the opaque part of the very same BG2 cell, never from neighboring
// artwork or local relief. This is still provisional, non-selectable ground.
class RemasterBackdropGroundBases
{
	static constexpr int Missing = 32768, Conflict = 32769;
	unsigned phaseX_ = 0, phaseY_ = 0, columns_ = 0;
	std::vector<int> bases_;

public:
	RemasterBackdropGroundBases(const RemasterFrame &frame, bool alttp)
	{
		if (!alttp || (frame.area.id != 320 && frame.area.id != 321) || !frame.width || !frame.height)
			return;
		const auto &bg = frame.observedBgPixels[1];
		if (bg.size() != size_t(frame.width) * frame.height ||
			std::none_of(frame.tileInstances.begin(), frame.tileInstances.end(), [](const auto &tile) {
				return tile.source == RemasterSourceType::Background && tile.sourceIndex == 1 && tile.savedPlacementOffsetStatus >= 2;
			})) return;
		for (unsigned y = 0; y < frame.height; ++y)
			for (unsigned x = 0; x < frame.width; ++x)
			{
				const auto &pixel = bg[size_t(y) * frame.width + x];
				if (!pixel.instanceId || pixel.instanceId > frame.tileInstances.size() || pixel.tilePixel >= 64) continue;
				const auto &tile = frame.tileInstances[pixel.instanceId - 1];
				if (tile.source != RemasterSourceType::Background || tile.sourceIndex != 1) continue;
				const unsigned px = ((pixel.tilePixel & 7) ^ (tile.hFlip ? 7 : 0)),
					py = ((pixel.tilePixel >> 3) ^ (tile.vFlip ? 7 : 0));
				const unsigned phaseX = (px - x) & 7, phaseY = (py - y) & 7;
				if (!columns_)
				{
					phaseX_ = phaseX; phaseY_ = phaseY;
					columns_ = (frame.width + phaseX + 7) / 8;
					bases_.assign(size_t(columns_) * ((frame.height + phaseY + 7) / 8), int{Missing});
				}
				if (phaseX != phaseX_ || phaseY != phaseY_)
				{
					// An inconsistent raster cannot prove one display-space BG2 grid.
					bases_.clear();
					return;
				}
				const int datum = tile.savedPlacementOffsetStatus >= 2 && !S9xRemasterInstanceGeometryUnresolved(tile)
					? tile.savedPlacementOffset : Conflict;
				auto &base = bases_[cell(x, y)];
				if (base == Missing) base = datum;
				else if (base != datum) base = Conflict;
			}
	}

	int at(unsigned x, unsigned y) const
	{
		if (bases_.empty()) return 0;
		const int base = bases_[cell(x, y)];
		return base == Missing || base == Conflict ? 0 : base;
	}

private:
	size_t cell(unsigned x, unsigned y) const
	{
		return size_t((y + phaseY_) / 8) * columns_ + (x + phaseX_) / 8;
	}
};

#endif
