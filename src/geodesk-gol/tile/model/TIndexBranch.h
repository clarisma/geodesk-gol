// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TElement.h"
#include <geodesk/geom/Box.h>

using namespace geodesk;
class TFeature;

class TIndexBranch : public TElement
{
public:
	TIndexBranch(Type type, const Box& bounds, uint32_t size) :
		TElement(type, 0, size, TElement::Alignment::DWORD),
		bounds_(bounds),
		nextSibling_(nullptr)
	{
	}

	bool isLeaf() const { return type() == Type::LEAF; }
	const Box& bounds() const { return bounds_; }
	TIndexBranch* nextSibling() const { return nextSibling_; }
	void setNextSibling(TIndexBranch* next) { nextSibling_ = next; }

private:
	const Box bounds_;
	TIndexBranch* nextSibling_;
};

