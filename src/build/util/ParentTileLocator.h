// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/cli/Console.h>
#include <geodesk/geom/TilePair.h>

using namespace geodesk;

// Ideally, empty state is 0xf, so twinCode is always 0 by default
// Even better: zoom level could be expressed as a delta
// 1 = "parent leves one level below" etc.
// The default can be 0
// store explicit export flag for rleations in Bit 7

class ParentTileLocator
{
public:
	ParentTileLocator() : loc_(0) {}
	ParentTileLocator(uint_fast8_t loc) : loc_(loc) {}

	static ParentTileLocator fromTileToPair(Tile source, TilePair target)
	{
		assert(source.zoom() >= target.zoom());
		int sourceZoom = source.zoom();
		source = source.zoomedOut(target.zoom());
		uint_fast8_t twinCode = target.isTwinOf(source);
		if (twinCode == Tile::INVALID_TWIN)
		{
			clarisma::Console::msg("%s is not a twin of %s", target.toString().c_str(),
				source.toString().c_str());
			assert(twinCode != Tile::INVALID_TWIN);
		}
		return ParentTileLocator((twinCode << 4) | (sourceZoom - target.zoom()));
	}

	int zoomDelta()
	{
		return loc_ & 15;
	}

	uint_fast8_t twinCode()
	{
		return loc_ >> 4;
	}

	operator uint_fast8_t() const noexcept
	{
		return loc_;
	}

	bool isEmpty() const noexcept
	{
		return loc_ == 0;		// i.e. just its own tile
	}

private:
	uint8_t loc_;
};
