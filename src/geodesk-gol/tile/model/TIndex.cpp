// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TIndex.h"
#include <clarisma/util/log.h>
#include "Layout.h"
#include "TFeature.h"
#include "TIndexTrunk.h"
#include "TileModel.h"
#include "tile/compiler/HilbertIndexBuilder.h"
#include "tile/compiler/IndexSettings.h"



TIndex::TIndex() :
	TElement(Type::INDEX, 0, 0, TElement::Alignment::DWORD),
	firstRoot_(-1),
	rootCount_(0)
{
	memset(&roots_, 0, sizeof(roots_));
	memset(&next_, -1, sizeof(next_));
}

void TIndex::Root::addFeature(TFeature* feature, uint32_t indexBits)
{
	assert((featureCount == 0) == (firstFeature == nullptr));
	if (firstFeature)
	{
		feature->setNext(firstFeature->next());
		firstFeature->setNext(feature);
	}
	else
	{
		firstFeature = feature;
		feature->setNext(feature);
	}
	featureCount++;
	this->indexBits |= indexBits;
}

void TIndex::Root::add(Root& other)
{
	assert((featureCount == 0) == (firstFeature == nullptr));
	assert(&other != this);
	if (other.isEmpty()) return;
	indexBits |= other.indexBits;
	if (isEmpty())
	{
		firstFeature = other.firstFeature;
	}
	else
	{
		TFeature* next = firstFeature->nextFeature();
		firstFeature->setNext(other.firstFeature->nextFeature());
		other.firstFeature->setNext(next);
	}
	featureCount += other.featureCount;
	other.featureCount = 0;
	other.firstFeature = nullptr;
}


void TIndex::Root::build(HilbertIndexBuilder& rtreeBuilder)
{
	trunk = rtreeBuilder.build(firstFeature, featureCount);
}


void TIndex::build(TileModel& tile, const IndexSettings& settings)
{
	int maxRootCount = settings.maxKeyIndexes();
	int minFeaturesPerRoot = settings.keyIndexMinFeatures();

	// LOG("Building index for tile %s", tile.tile().toString().c_str());
	HilbertIndexBuilder rtreeBuilder(tile, settings.rtreeBucketSize());

	// - Sort roots by number of features
	// - Consolidate Roots with less features than minFeaturesPerRoot 
	//   into multi-cat root.
	
	for (int i = 0; i < MULTI_CATEGORY; i++)
	{
		Root& root = roots_[i];
		if (root.featureCount < minFeaturesPerRoot)
		{
			// Consolidate this root into the multi-cat root
			roots_[MULTI_CATEGORY].add(root);
		}
		else
		{
			// Otherwise, place this root into a linked list, 
			// largest roots first

			int8_t* pNextRoot = &firstRoot_;
			for (;;)
			{
				int nextRoot = *pNextRoot;
				if (nextRoot >= 0 && roots_[nextRoot].featureCount > root.featureCount)
				{
					pNextRoot = &next_[nextRoot];
					continue;
				}
				*pNextRoot = static_cast<int8_t>(i);
				next_[i] = nextRoot;
				break;
			}
			rootCount_++;
		}
	}

	// rootCount_ is now the total number of non-empty roots, 
	// excluding the multi-cat root

	// include multi-cat if it has any features
	int rootCountWithMultiCat = rootCount_ + (roots_[MULTI_CATEGORY].isEmpty() ? 0 : 1);
	// TODO: check this, or remove (unused)

	// The number of roots (excluding multi-cat) to keep as-is
	int keepRootCount = std::min(rootCount_, maxRootCount - 1);
	// (If we have 4 roots and multi-cat is empty, and the limit is 4,
	// this simply means the smallest root turns into the multi-cat
	// root, which simply consists of a single category)

	// Build the rtree for all roots that are below maxRootCount
	// (except for multi-cat root)

	int8_t* pNextRoot = &firstRoot_;
	for (int i = 0; i < keepRootCount; i++)
	{
		int nextRoot = *pNextRoot;
		assert (nextRoot >= 0);
		roots_[nextRoot].build(rtreeBuilder);
		pNextRoot = &next_[nextRoot];
	}

	// If there are more roots than maxRootCount, consolidate the
	// roots with the lowest numbers of features into the multi-cat root

	int8_t* pLastRoot = pNextRoot;

	int consolidateRootCount = rootCount_ - keepRootCount;
	for (int i=0; i < consolidateRootCount; i++)
	{
		int nextRoot = *pNextRoot;
		assert(nextRoot >= 0);
		roots_[MULTI_CATEGORY].add(roots_[nextRoot]);
		pNextRoot = &next_[nextRoot];
	}

	*pLastRoot = -1;

	// Adjust the root count
	rootCount_ = keepRootCount;

	if (!roots_[MULTI_CATEGORY].isEmpty())
	{
		// If there are any features in the multi-cat root,
		// add it to the end and build its rtree 
		*pLastRoot = MULTI_CATEGORY;
		roots_[MULTI_CATEGORY].build(rtreeBuilder);
		rootCount_++;
	}

	setSize(rootCount_ * 8);
}


void TIndex::place(Layout& layout)
{
	if (isEmpty()) return;

	layout.place(this);
	int rootNumber = firstRoot_;
	do
	{
		roots_[rootNumber].trunk->place(layout);
		rootNumber = next_[rootNumber];
	}
	while (rootNumber >= 0);
}



void TIndex::write(const TileModel& tile) const
{
	int pos = location();
	MutableDataPtr p(tile.newTileData() + pos);
	int rootNumber = firstRoot_;
#ifndef NDEBUG
	int myRootCount = 0;
#endif
	do
	{
		int nextRootNumber = next_[rootNumber];
		const Root& root = roots_[rootNumber];
		int trunkLoc = root.trunk->location();
		assert(trunkLoc != 0);
		assert(!root.trunk->isLeaf());
		assert(trunkLoc != pos);
		p.putInt(
			(trunkLoc - pos) |
			(nextRootNumber < 0 ? 1 : 0));
		(p + 4).putUnsignedInt(root.indexBits);
		p += 8;
		pos += 8;
		rootNumber = nextRootNumber;
#ifndef NDEBUG
		myRootCount++;
#endif
	}
	while (rootNumber >= 0);
#ifndef NDEBUG
	assert(myRootCount == rootCount_);
#endif
	assert(pos-location() == size());
}
