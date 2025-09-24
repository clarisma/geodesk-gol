// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TIndexLeaf.h"
#include "Layout.h"
#include "TNode.h"
#include "TWay.h"
#include "TRelation.h"

uint32_t TIndexLeaf::calculateSize(TFeature* firstFeature)
{
	uint32_t size = 0;
	TFeature* p = firstFeature;
	do
	{
		size += p->size();
		p = p->nextFeature();
	}
	while (p);
	return size;
}


/**
 * Place the feature in this leaf branch, then place any uncommon tag tables
 * that haven't already been placed.
 */
void TIndexLeaf::place(Layout& layout)
{
	LinkedQueue<TTagTable> tagTables;

	// Note: We don't place the index leaf itself; instead, we place the 
	// feature stubs that are contained in this leaf

	TFeature* feature = firstFeature();
	for (;;)
	{
		TFeature* nextFeature = feature->nextFeature();
		layout.place(feature);	// place() changes `next`
		TTagTable* tags = feature->tags(layout.tile());

		if (feature->feature().id() == 1302237668)
		{
			LOG("way/%lld with tags %s", feature->feature().id(),
				tags->toString(layout.tile()).c_str());
		}

		assert(tags);
		if (tags->location() == 0)
		{
			tagTables.addTail(tags);
			tags->setLocation(-1);
		}
		switch (feature->feature().typeCode())
		{
		case 0:
			reinterpret_cast<TNode*>(feature)->placeBody(layout);
			break;
		case 1:
			reinterpret_cast<TWay*>(feature)->placeBody(layout);
			break;
		case 2:
			reinterpret_cast<TRelation*>(feature)->placeBody(layout);
			break;
		default:
			assert(false);
			break;
		}
		if (!nextFeature)
		{
			feature->markLast();
			break;
		}
		feature = nextFeature;
	}

	TTagTable* tags = tagTables.first();
	while (tags)
	{
		TTagTable* nextTags = tags->nextTags();
		layout.place(tags);		// place() changes `next`
		tags->placeStrings(layout);
		tags = nextTags;
	}

	/*
	// The location of the leaf branch is that of its first child
	assert(firstFeature()->location() != 0);
	setLocation(firstFeature()->location());
	*/
}

