// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "THeader.h"
#include "Layout.h"
#include "TExportTable.h"
#include "TFeature.h"
#include "TileModel.h"
#include "tile/compiler/IndexSettings.h"

void THeader::write(const TileModel& tile) const
{
	assert(location() == 4);
	MutableDataPtr p(tile.newTileData() + location());
	p.putUnsignedInt(0); // TODO: revision
	p += 4;
	int ofs = location() + 4;
	for (auto& index : indexes_)
	{
		p.putInt(index.isEmpty() ? 0 : (index.location() - ofs));
		p += 4;
		ofs += 4;
	}
	p.putInt(exportTable_ == nullptr ? 0 :
		(exportTable_->location() + exportTable_->anchor() - ofs));
}



/**
 * If we right-shift the feature flags by 1, then take the bottom 4 bits,
 * we can tell to which index the feature belongs without having to branch
 * (We're interested in the type bits and the area-flag; we'll ignore the
 *  member flag)
 */
const uint8_t THeader::FLAGS_TO_TYPE[16] =
{
	NODES, INVALID, NODES, INVALID,
	WAYS, AREAS, WAYS, AREAS,
	RELATIONS, AREAS, RELATIONS, AREAS,
	INVALID, INVALID, INVALID, INVALID
};


void THeader::addFeatures(TileModel& tile)
{
	FeatureTable::Iterator iter = tile.features().iter();
	while (iter.hasNext())
	{
		TFeature* feature = iter.next();

		// LOG("Indexing %s...", feature->feature().toString().c_str());
		int typeFlags = (feature->flags() >> 1) & 15;
		int type = FLAGS_TO_TYPE[typeFlags];
		assert(type != INVALID);		// TODO: make this a proper runtime check?
		TTagTable* tags = feature->tags(tile);
		/*
		if (!tags)
		{
			if (!feature->feature().tags().hasLocalKeys())
			{
				LOG("  No tags (no locals)");
			}
			continue;
		}
		*/
		assert(tags);
		/*
		if (feature->feature().tags().hasLocalKeys())
		{
			LOG("  Tags (locals)");
		}
		*/
		int category = tags->category();

		// TODO: What about "no category" (0) ?????

		uint32_t indexBits;
		if (category >= TIndex::MULTI_CATEGORY)
		{
			// Category is unassigned or multi-category
			// In both cases, we need to construct indexBits
			indexBits = tags->assignIndexCategory(settings_);
			category = tags->category();
		}
		else
		{
			indexBits = 1 << (category - 1);
		}
		indexes_[type].addFeature(feature, category, indexBits);
	}
}


void THeader::build(TileModel& tile)
{
	for (auto& index : indexes_)
	{
		index.build(tile, settings_);
	}
}

void THeader::place(Layout& layout)
{
	layout.place(this);
	for (auto& index : indexes_)
	{
		index.place(layout);
	}
	if(exportTable_) layout.place(exportTable_);
}


