// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "SuperRelation.h"
#include <clarisma/util/varint.h>


void SuperRelation::validate()
{
	int removedRelationCount = 0;
	int highestMemberZoom = 0;
	for (const SortedChildFeature& member : members_)
	{
		if (member.typedId == 0)
		{
			removedRelationCount++;
		}
		else
		{
			highestMemberZoom = std::max(highestMemberZoom, member.tilePair.zoom());
		}
	}
	highestMemberZoom_ = static_cast<int8_t>(highestMemberZoom);
	missingMemberCount_ += removedRelationCount - removedRefcyleCount_;
	if (removedRelationCount) recode(removedRelationCount);
}

/**
 * Removes the member entries of any relations that have been removed,
 * and also re-encodes the body data.
 */
void SuperRelation::recode(int removedRelationCount)
{
	int nSource = 0;
	int nDest = 0;
	const uint8_t* pSource = body_.data();
	const uint8_t* const pSourceEnd = body_.end();
	uint8_t* pDest = body_.data();

	while (nSource < members_.size())
	{
		uint64_t typedMemberId = readVarint64(pSource);
		uint32_t refOrLen = readVarint32(pSource);
		if (members_[nSource].typedId != 0)
		{
			members_[nDest++] = members_[nSource];
			writeVarint(pDest, typedMemberId);
			writeVarint(pDest, refOrLen);
			if ((refOrLen & 1) == 0)
			{
				uint32_t len = refOrLen >> 1;
				// Must use memmove to copy role string since regions may overlap
				memmove(pDest, pSource, len);
				pDest += len;
				pSource += len;
			}
		}
		else
		{
			// skip literal role string
			if ((refOrLen & 1) == 0) pSource += refOrLen >> 1;
		}
		nSource++;
	}
	assert(removedRelationCount == nSource - nDest);
	members_ = Span<SortedChildFeature>(members_.data(), nDest);
	// move the tags at the end of the body
	size_t tagsSize = pSourceEnd - pSource;
	memmove(pDest, pSource, tagsSize);
	assert(pDest + tagsSize <= pSourceEnd);
	body_ = Span<uint8_t>(body_.data(), pDest + tagsSize);
}
