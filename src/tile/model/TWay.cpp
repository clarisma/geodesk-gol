// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TWay.h"
#include "Layout.h"
#include "TileModel.h"
#include <clarisma/util/varint.h>
#include "tile/compiler/NodeTableFixer.h"

void TWay::placeBody(Layout& layout)
{
	layout.addBodyElement(body());
	if (isRelationMember())
	{
		placeRelationTable(layout);
	}
}


void TWayBody::write(const TileModel& tile) const
{
	// LOGS << "Writing body of way/" << constFeature()->feature().id();
	/*
	if (1302426690 == constFeature()->feature().id())
	{
		LOG("!!!");
	}
	*/

	uint8_t* p = tile.newTileData() + location();
	memcpy(p, dataStart(), size());

	if(constFeature()->feature().isRelationMember())
	{
		fixRelationTablePtr(p, tile);
	}

	if (needsFixup())
	{
		// Adjust the pointers to local nodes
		NodeTableFixer(this, p + anchor()).fix(tile);
	}
}

int TWayBody::nodeCount() const
{
	const uint8_t* p = data();
	return static_cast<int>(readVarint32(p));
}



