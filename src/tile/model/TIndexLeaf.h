// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TIndexBranch.h"

class Layout;

class TIndexLeaf : public TIndexBranch
{
public:
	TIndexLeaf(const Box& bounds, TFeature* firstFeature) :
		TIndexBranch(Type::LEAF, bounds, calculateSize(firstFeature)),
		firstFeature_(firstFeature)
	{
	}

	void place(Layout& layout);

	TFeature* firstFeature() const { return firstFeature_; }

private:
	static uint32_t calculateSize(TFeature* firstFeature);

	TFeature* firstFeature_;
};
