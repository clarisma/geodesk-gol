// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/MutableDataPtr.h>
#include <geodesk/feature/Tex.h>
#include "tile/model/TFeature.h"

// TODO: Check if this class can handle negative offsets
//  The handle for a way's body could be 4 (first assigned handle),
//  so offsets in the feature-node table could be negative, since
//  this table is placed ahead of the anchor (which is the handle)
//
template<int ExtraFlags, int Step>
class AbstractMemberTableWriter
{
public:
	AbstractMemberTableWriter(TElement::Handle handle, DataPtr p) :
		pTile_(p - handle),
		ofs_(handle),
		lastMemberOfs_(0)
	{
		#ifdef GOL_BUILD_STATS
		memberCount = 0;
		foreignMemberCount = 0;
		wideTexMemberCount = 0;
		#endif
	}

	DataPtr ptr() const { return pTile_ + ofs_; }

	// TODO: This assumes at least one feature node has been written
	// TODO: Don't assume lastMemberOfs_ == 0 has any special meaning,
	//  as offsets could be 0 or even negative
	void markLast()
	{
		MutableDataPtr p = pTile_ + lastMemberOfs_;
		p.putShort(p.getShort() | MemberFlags::LAST);
	}

protected:
	void writeTipDelta(TipDelta tipDelta)
	{
		static_assert(Step == 2 || Step == -2);
		bool isWideTipDelta = tipDelta.isWide();
		(pTile_ + ofs_).putShort(static_cast<int16_t>(
			(static_cast<int>(tipDelta) << 1) |
			static_cast<int>(isWideTipDelta)));
		if (isWideTipDelta)
		{
			ofs_ += Step;
			(pTile_ + ofs_).putShort(static_cast<int16_t>(tipDelta >> 15));
		}
	}


	void writeForeign(TexDelta texDelta, int flags = 0)
	{
		static_assert(ExtraFlags  == 0 || ExtraFlags  == 1);
		static_assert(Step == 2 || Step == -2);
		constexpr int narrowBits = 12 - ExtraFlags;
		int wideTexFlag = texDelta.isWide(narrowBits)  ? (1 << (15 - narrowBits)) : 0;
		(pTile_ + ofs_).putShort(static_cast<int16_t>(
			(texDelta << (16 - narrowBits)) | MemberFlags::FOREIGN | flags | wideTexFlag));
		lastMemberOfs_ = ofs_;
		if (wideTexFlag)
		{
			ofs_ += Step;
			(pTile_ + ofs_).putShort(static_cast<uint16_t>(texDelta >> narrowBits));
			#ifdef GOL_BUILD_STATS
			wideTexMemberCount++;
			#endif
		}
		#ifdef GOL_BUILD_STATS
		memberCount++;
		foreignMemberCount++;
		#endif
	}

	void writeForeign(TipDelta tipDelta, TexDelta texDelta, int flags = 0)
	{
		constexpr int differentTileFlag = 1 << (2 + ExtraFlags);
		writeForeign(texDelta, flags | differentTileFlag);
		ofs_ += Step;
		writeTipDelta(tipDelta);
	}



	// TODO: Fix this!!!!!!
	// Clear up spec re rebasing of offsets when referencing features
	// (feature are 4-byte-aligned, but feature table offsets are only 2-byte aligned)
	void writeLocal(TFeature* feature, int flags = 0)
	{
		lastMemberOfs_ = ofs_;
		int_fast32_t ptr;
		if (ExtraFlags > 0)
		{
			// MemberTable entries have 3 flags (i.e. 1 "extra" flag),
			// so we need to perform pointer rebasing
			ptr = feature->handle() - (ofs_ & 0xffff'fffc);
		}
		else
		{
			ptr = feature->handle() - ofs_;
		}
		(pTile_ + ofs_).putUnsignedShort(static_cast<uint16_t>((ptr << 1) | flags));
		ofs_ += Step;
		(pTile_ + ofs_).putShort(static_cast<int16_t>(ptr >> 15));
		#ifdef GOL_BUILD_STATS
		memberCount++;
		#endif
	}

	MutableDataPtr pTile_;
	int_fast32_t ofs_;
	int_fast32_t lastMemberOfs_;
	#ifdef GOL_BUILD_STATS
public:
	int memberCount;
	int foreignMemberCount;
	int wideTexMemberCount;
	#endif
};
