// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <vector>
#include "build/util/ForeignRelationLookup.h"
#include <geodesk/geom/Tile.h>
#include <geodesk/geom/index/hilbert.h>
#include "VFeature.h"
#include "ValidatorPileWriter.h"

class ExportTableBuilder
{
public:
	ExportTableBuilder() :
		tileLeft_(0),
		tileBottom_(0),
		zoomDelta_(0)
	{
	}

	void init(Tile tile)
	{
		assert(exports_.empty());
		tileLeft_ = tile.leftX();
		tileBottom_ = tile.bottomY();
		zoomDelta_ = 16 - tile.zoom();
	}

	void addExport(VFeature* feature, Coordinate center)
	{
		assert(zoomDelta_ >= 4);
		int32_t x = std::clamp((center.x - tileLeft_) >> zoomDelta_, 0, hilbert::MAX_COORDINATE);
		int32_t y = std::clamp((center.y - tileBottom_) >> zoomDelta_, 0, hilbert::MAX_COORDINATE); 
		exports_.emplace_back(hilbert::calculateHilbertDistance(
			static_cast<uint32_t>(x), static_cast<uint32_t>(y)), feature);
	}

	Block<ForeignRelationLookup::Entry> build(ValidatorPileWriter& writer)
	{
		if (!exports_.empty())
		{
			// Console::msg("%d exports", exports_.size());
			std::sort(exports_.begin(), exports_.end());

			uint8_t buf[32];
			uint8_t* p;
			PileSet::Pile* pile = writer.getLocal(ProtoGol::GroupType::EXPORT_TABLE);

			int64_t prevTypedId = 0;
			p = buf;
			writeVarint(p, exports_.size());
			writer.write(pile, buf, p-buf);

			for (int i=0; i<exports_.size(); i++)
			{
				Tex tex(i);
				VFeature* feature = exports_[i].second;
				feature->tex = static_cast<int>(tex);

				if(feature->typedId() == TypedFeatureId::ofNode(4418343161))
				{
					LOGS << "Assigned TEX " << static_cast<int>(tex) << " to " << feature->typedId() << "\n";
				}

				int64_t typedId = static_cast<int64_t>(feature->typedId());
				p = buf;
				writeSignedVarint(p, typedId - prevTypedId);
				writer.write(pile, buf, p-buf);
				prevTypedId = typedId;

				if (feature->isRelation())
				{
					exportedRelations_.emplace_back(feature->id(), tex);
				}
			}
		}

		Block<ForeignRelationLookup::Entry> lookup(exportedRelations_.size());
		if (!exportedRelations_.empty())
		{
			ForeignRelationLookup::create(lookup, exportedRelations_);
		}
		exports_.clear();
		exportedRelations_.clear();
		return lookup;
	}

private:
	std::vector<std::pair<uint32_t, VFeature*>> exports_;
	std::vector<ForeignRelationLookup::Entry> exportedRelations_;
	int32_t tileLeft_;
	int32_t tileBottom_;
	int zoomDelta_;
};
