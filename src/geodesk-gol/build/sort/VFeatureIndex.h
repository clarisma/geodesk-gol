// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/alloc/Block.h>
#include "VFeature.h"

class VFeatureIndex
{
public:
	void init(size_t size)
	{
		table_ = Block<VFeature*>(size);
		clear();
	}

	void clear()
	{
		memset(table_.data(), 0, sizeof(VFeature*) * table_.size());
	}

	void addFeature(VFeature* f)
	{
		size_t slot = static_cast<uint64_t>(f->typedId()) % table_.size();
		f->next = table_[slot];
		table_[slot] = f;
	}

	VFeature* getFeature(TypedFeatureId typedId) const
	{
		size_t slot = static_cast<uint64_t>(typedId) % table_.size();
		VFeature* f = table_[slot];
		while (f)
		{
			if (typedId == f->typedId()) return f;
			f = f->next;
		}
		return nullptr;
	}

	VNode* getNode(uint64_t id) const
	{
		// NOLINTNEXTLINE cast is safe
		VNode* node = static_cast<VNode*>(getFeature(TypedFeatureId::ofNode(id)));
		assert(node == nullptr || node->isNode());
		return node;
	}

	VLocalNode* checkSharedLocation(VLocalNode* node)
	{
		size_t slot = std::hash<Coordinate>()(node->xy) % table_.size();
		VFeature* first = table_[slot];
		VFeature* f = first;
		while (f)
		{
			VLocalNode* otherNode = f->asLocalNode();
			assert(std::hash<Coordinate>()(otherNode->xy) % table_.size() == slot);
			if (node->xy == otherNode->xy)
			{
				return otherNode;
					// no need to index this node, we're just interested
					// if there is at least one node at this location
			}
			f = f->next;
		}
		node->next = first;
		table_[slot] = node;
		return nullptr;
	}

private:
	Block<VFeature*> table_;
};
