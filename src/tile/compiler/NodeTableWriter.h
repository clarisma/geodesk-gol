// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "AbstractMemberTableWriter.h"
#include "tile/model/TNode.h"
#include "tile/model/TileModel.h"

class NodeTableWriter : public AbstractMemberTableWriter<0,-2>
{
public:
	NodeTableWriter(TElement::Handle handle, DataPtr p) :
		AbstractMemberTableWriter(handle, p)
	{
	}

	void writeLocalNode(TNode* node)
	{
		ofs_ -= 2;
		writeLocal(node);
	}

	void writeForeignNode(TexDelta texDelta)
	{
		ofs_ -= 2;
		writeForeign(texDelta);
	}

	void writeForeignNode(TipDelta tipDelta, TexDelta texDelta)
	{
		ofs_ -= 2;
		writeForeign(tipDelta, texDelta);
	}
};
