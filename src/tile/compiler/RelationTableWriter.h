// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "AbstractMemberTableWriter.h"
#include "RelationTableHasher.h"
#include "tile/model/TileModel.h"
#include "tile/model/TRelation.h"


class RelationTableWriter : public AbstractMemberTableWriter<0, 2>
{
public:
	RelationTableWriter(TElement::Handle handle, DataPtr p) :
		AbstractMemberTableWriter(handle, p)
	{
	}

	size_t hash() const { return hasher_.hash(); }

	void writeLocalRelation(TRelation* rel)
	{
		hasher_.addLocalRelation(rel->handle());
		writeLocal(rel);
		ofs_ += 2;		// writeLocal() already stepped by 2
	}

	void writeForeignRelation(TexDelta texDelta)
	{
		hasher_.addTexDelta(texDelta);
		writeForeign(texDelta);
		ofs_ += 2;
	}

	void writeForeignRelation(TipDelta tipDelta, TexDelta texDelta)
	{
		hasher_.addTipDelta(tipDelta);
		hasher_.addTexDelta(texDelta);
		writeForeign(tipDelta, texDelta);
		ofs_ += 2;
	}

private:
	RelationTableHasher hasher_;
};
