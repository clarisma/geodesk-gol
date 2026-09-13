// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TIndexBranch.h"

class Layout;
class TileModel;

class TIndexTrunk : public TIndexBranch
{
public:
	TIndexTrunk(const Box& bounds, TIndexBranch* firstBranch, int count) :
		TIndexBranch(Type::TRUNK, bounds, count * 20),
		firstBranch_(firstBranch)
	{
	}

	TIndexBranch* firstChildBranch() const { return firstBranch_; }

	void place(Layout& layout);
	void write(const TileModel& tile) const;

private:
	TIndexBranch* firstBranch_;
};
