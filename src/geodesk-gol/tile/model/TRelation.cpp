// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TRelation.h"
#include <clarisma/util/log.h>
#include "TileModel.h"
#include "Layout.h"
#include "tile/compiler/MemberTableFixer.h"

void TRelationBody::write(const TileModel& tile) const
{
	//LOG("Writing body of relation/%lld", constFeature()->feature().id());
	uint8_t* p = tile.newTileData() + location();
	memcpy(p, dataStart(), size());

	if(constFeature()->feature().isRelationMember())
	{
		fixRelationTablePtr(p, tile);
	}

	if (needsFixup())
	{
		// Adjust the pointers to local members and local roles
		MemberTableFixer(this, p + anchor()).fix(tile);
	}
}


void TRelation::placeBody(Layout& layout)
{
	TRelationBody* body = this->body();
	layout.addBodyElement(body);
	if (isRelationMember())
	{
		placeRelationTable(layout);
	}

	// TODO: Need a flag to indicate presence of local role strings
	//  (which are rare), so we can skip this step

	MemberTableIterator iter(body->handle(), body->data());
	while(iter.next())
	{
		if(iter.hasDifferentRole() && iter.hasLocalRole())
		{
			TString* str = layout.tile().getString(iter.localRoleHandleFast());
			assert(str);
			if (str->location() == 0)
			{
				layout.addBodyElement(str);
			}
		}
	}
}
