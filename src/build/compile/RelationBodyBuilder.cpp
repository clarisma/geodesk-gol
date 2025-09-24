// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "RelationBodyBuilder.h"
#include <string_view>
#include <clarisma/util/log.h>
#include <clarisma/util/varint.h>
#include <tile/model/Membership.h>

#include "build/util/ProtoGol.h"
#include "tile/model/TileModel.h"
#include "tile/model/TRelation.h"
#include "tile/model/TRelationTable.h"
#include "tile/compiler/MemberTableWriter.h"

// TODO: Cannot be a singleton in Worker, relations are built recursively

void RelationBodyBuilder::build(TileModel& tile, TRelationBody* body, Membership* firstParent)
{
	TElement::Handle bodyHandle = tile.newHandle();
	body->setHandle(bodyHandle);
	uint32_t relTablePtrSize = firstParent ? 4 : 0;
	body->setAnchor(relTablePtrSize);

	if(members_.empty())
	{
		LOGS << "Empty relation";
		assert(false);
	}

	// pre-alloc space with the most pessimistic assumption: each member is foreign,
	// wide tip-delta, wide tex-delta, with its own local-string role, requiring
	// 12 bytes per member
	// TODO: We could calculate the true size based on the staged members, but
	// it's preferable to keep all the encoding logic in MemberTableWriter
	size_t maxBodySize = members_.size() * 12 + relTablePtrSize;
	uint8_t* pBodyData = tile.arena().alloc(maxBodySize, alignof(uint16_t));

	ForeignFeatureRef prevForeign(Tip(), Tex::MEMBERS_START_TEX);
		// leave previous TIP as 0 (invalid), because we always flag the first 
		// foreign member with `tile_changed`
	Role prevRole(0, nullptr);
	MemberTableWriter writer(bodyHandle, pBodyData + relTablePtrSize);
	bool needsFixup = false;
	for (RelationMember member : members_)
	{
		int roleChangeFlag = (member.role != prevRole) ? MemberFlags::DIFFERENT_ROLE : 0;
		if (member.local)
		{
			writer.writeLocalMember(member.local, roleChangeFlag);
			needsFixup = true;
		}
		else
		{
			if (member.foreign.tip != prevForeign.tip)
			{
				if (prevForeign.tip.isNull()) prevForeign.tip = FeatureConstants::START_TIP;
				writer.writeForeignMember(member.foreign.tip - prevForeign.tip,
					member.foreign.tex - prevForeign.tex, roleChangeFlag);
			}
			else
			{
				writer.writeForeignMember(member.foreign.tex - prevForeign.tex, roleChangeFlag);
			}
			prevForeign = member.foreign;
		}
		if (roleChangeFlag)
		{
			if (member.role.isGlobal())
			{
				writer.writeGlobalRole(member.role.code());
			}
			else
			{
				writer.writeLocalRole(member.role.localString());
				needsFixup = true;
			}
			prevRole = member.role;
		}
	}
	writer.markLast();

	size_t actualBodySize = writer.ptr().ptr() - pBodyData;
	assert(actualBodySize <= maxBodySize);
	tile.arena().reduceLastAlloc(maxBodySize - actualBodySize);
	body->setData(pBodyData + relTablePtrSize);
	body->setSize(actualBodySize);
	body->setAnchor(relTablePtrSize);
	body->setFlag(TRelationBody::Flags::NEEDS_FIXUP, needsFixup);

	// TODO: parent rels
	
	prevStagedTip_ = Tip();
	prevAltRef_ = ForeignFeatureRef();

	#ifdef GOL_BUILD_STATS
	memberCount = writer.memberCount;
	foreignMemberCount = writer.foreignMemberCount;
	wideTexMemberCount = writer.wideTexMemberCount;
	#endif
}
