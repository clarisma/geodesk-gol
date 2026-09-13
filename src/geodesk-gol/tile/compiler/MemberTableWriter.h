// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "AbstractMemberTableWriter.h"
#include "tile/model/TString.h"

class MemberTableWriter : public AbstractMemberTableWriter<1, 2>
{
public:
	MemberTableWriter(TElement::Handle handle, DataPtr p) :
		AbstractMemberTableWriter(handle, p)
	{
	}

	void writeLocalMember(TFeature* member, int roleChangeFlag)
	{
		writeLocal(member, roleChangeFlag);
		ofs_ += 2;
	}

	void writeForeignMember(TexDelta texDelta, int flags)
	{
		writeForeign(texDelta, flags);
		ofs_ += 2;
	}

	// TODO !!!!!!!
	//  AbstractMemberTableWriter::writeForeignMember() already
	//  writes the different_tile flag (which various among the
	//  different types of tables), the fixed flag is wrong!!!
	//  But bug has not manifested, why??
	//  This works because MemberFlags::DIFFERENT_TILE is bit 3 (value 8),
	//  which stays the same for member tables (nodes & parent relations
	//  put this flag at bif 2 (value 4)
	void writeForeignMember(TipDelta tipDelta, TexDelta texDelta, int roleChangeFlag)
	{
		writeForeign(tipDelta, texDelta, MemberFlags::DIFFERENT_TILE | roleChangeFlag);
			// TODO: Is the use of DIFFERENT_TILE correct?
			//  position of this flag can vary!
		ofs_ += 2;
	}

	void writeGlobalRole(int code)
	{
		assert(code >= 0 && code <= FeatureConstants::MAX_COMMON_ROLE);
		(pTile_ + ofs_).putUnsignedShort(static_cast<uint16_t>((code << 1) | 1));
		ofs_ += 2;
	}

	void writeLocalRole(TString *str)
	{
		(pTile_ + ofs_).putIntUnaligned((str->handle() - ofs_) << 1);
		ofs_ += 4;
	}
};
