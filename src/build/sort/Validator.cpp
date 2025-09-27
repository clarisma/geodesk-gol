// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Validator.h"
#include <algorithm> 
#include <memory>
#include <clarisma/util/BitIterator.h>
#include <clarisma/util/varint.h>
#include "build/GolBuilder.h"
#include "build/util/ProtoGol.h"
#include <geodesk/feature/types.h>
#include <geodesk/feature/TypedFeatureId.h>
#include <geodesk/geom/Box.h>
#include <geodesk/geom/LonLat.h>

// Since we now use tile pairs instead of tile quads, we only need 2 phases 
// per zoom level: black tiles (even) and white tiles (odd)

// TODO: Export the bboxes of multi-tile features only in second (odd) batch,
// because that's when they are complete

ValidatorWorker::ValidatorWorker(Validator* validator) :
	validator_(validator),
	pileWriter_(validator->builder_->tileCatalog()),
	arena_(4 * 1024 * 1024),
	currentSection_(-1)
{
}


Validator::Validator(GolBuilder* builder) :
	TaskEngine(builder->threadCount()),
	builder_(builder),
	workPerTile_ (builder->phaseWork(GolBuilder::Phase::VALIDATE) 
		/ builder->tileCatalog().tileCount()),
	exportsWriter_(builder->workPath() / "exports.bin", builder->tileCatalog().tileCount())
{
}



void ValidatorWorker::processTask(ValidatorTask& task)
{
	// Console::msg("Validating %s (Pile %d)...", task.tile().toString().c_str(), task.pile());
	currentTile_ = task.tile();
	validator_->builder_->featurePiles().load(task.pile(), data_);
	index_.init(data_.size() / 4);		// TODO: tune table size
	exportTable_.init(currentTile_);
	pileWriter_.init(task.pile(), currentTile_);
	readTile();
	useSection(SECTION_OTHER);
		// Ensure sections are initialized properly if there are no exported features
	processRelations();
	processWays();
	processNodes();
	Block<ForeignRelationLookup::Entry> foreignRelations =
		exportTable_.build(pileWriter_);

	// TODO: THis is inefficient, we just want to close the
	//  local pile where we've just written the ExportTable
	pileWriter_.closePiles();

	exportNodes();
	exportFeatures(SECTION_LOCAL_WAYS);
	exportFeatures(SECTION_LOCAL_RELATIONS);
	validator_->postOutput(ValidatorOutputTask(task.pile(),
		std::move(pileWriter_), std::move(foreignRelations)));

	arena_.clear();
	currentSection_ = -1;
	// specialNodes_.clear();		// TODO: clear after writing

	// process relations, working backward so parent relations come before child relations
	// process ways
	// build export table
	// export nodes, index/check shared locations (overwrites the tex)
	// export ways
	// export relations
	// write shared/orphan data for nodes

}


void ValidatorWorker::useSection(int section)
{
	assert(section >= currentSection_);
	if (section > currentSection_)
	{
		for (int i = currentSection_ + 1; i <= section; i++)
		{
			sections_[i] = arena_.section();
		}
		currentSection_ = section;
	}
}

void ValidatorWorker::node(uint64_t id, Coordinate xy, ByteSpan tags)
{
	if(!currentTile_.bounds().contains(xy))
	{
		LOGS << currentTile_ << ": node/" << id << " (" << LonLat(xy)
			<< ") is not in tile bounds (" << currentTile_.bounds() << ")\n";
		assert(false);
	}

	index_.addFeature(
		arena_.create<VLocalNode>(
			id, (tags.isEmpty() ? 0 : VLocalNode::TAGGED_NODE), xy));
}

void ValidatorWorker::way(uint64_t id, ParentTileLocator locator, ByteSpan body)
{
	if (locator.zoomDelta() > 0)
	{
		// This is a "ghost" way
		uint64_t nodeTiles = childExports(locator);
		const uint8_t* p = body.data();
		uint32_t nodeCount = readVarint32(p) >> 1;
			// bit 0 is closed_ring_flag
		uint64_t prevNodeId = 0;
		for (int i = 0; i < nodeCount; i++)
		{
			uint64_t nodeId = prevNodeId + readSignedVarint64(p);
			prevNodeId = nodeId;
			VNode* node = index_.getNode(nodeId);
			assert(node);				// The node must exist
			assert(!node->isForeign());	// and must be local
			node->asLocalNode()->tiles |= nodeTiles;
			node->setFlag(VNode::WAY_NODE);
		}
		return;
	}
	index_.addFeature(
		arena_.create<VLocalFeature2D>(
			FeatureType::WAY, id, locator, body.data()));
		// TODO: twin flags
}

void ValidatorWorker::relation(uint64_t id, ParentTileLocator locator, ByteSpan body)
{
	VLocalFeature2D* rel = arena_.create<VLocalFeature2D>(
		FeatureType::RELATION, id, locator, body.data());
	if(locator.zoomDelta() > 0)
	{
		// parent_zoom_delta==1 signals that this relation must always
		// be exported, since it has members at a higher zoom level
		// than itself (the actual delta holds no meaning, it is merely
		// a flag)
		assert(locator.zoomDelta() == 1);
		rel->setFlag(VLocalFeature2D::Flags::EXPORT_RELATION_ALWAYS);
	}
	index_.addFeature(rel);
}

void ValidatorWorker::membership(uint64_t relId, ParentTileLocator locator, TypedFeatureId typedMemberId)
{
	VFeature* feature = index_.getFeature(typedMemberId);
	assert(feature);
	uint64_t tiles = childExports(locator);
	if (feature->isNode())
	{
		feature->asLocalNode()->tiles |= tiles;
		feature->setFlag(VNode::RELATION_NODE);
	}
	else
	{
		feature->asLocalFeature2D()->tentativeTiles |= tiles;
	}
}

void ValidatorWorker::foreignNode(uint64_t id, Coordinate xy, ForeignFeatureRef ref)
{
	// `ref` is not used -- its TIP is always 0 
	// (because Validator does not override pileToTile)

	// Console::msg("foreign node/%lld", id);
	index_.addFeature(arena_.create<VNode>(id, VNode::FOREIGN, xy));
}

void ValidatorWorker::foreignFeature(FeatureType type, uint64_t id, const Box& bounds, ForeignFeatureRef ref)
{
	// `ref` is not used -- its TIP is always 0 
	// (because Validator does not override pileToTile)
	if (!bounds.isEmpty())
	{
		index_.addFeature(arena_.create<VForeignFeature2D>(type, id, bounds));
	}
}

// TODO: handle case where way's geometry needs to be calculated because
// its parent relation is exported, but way is not; later, a different relation
// requires the way to be exported
// --> is this actually possible?? YES
// --> ned to add a feature to an export table when it is first exported,
// not when processed -- but we need the hilbert number for that, which
// we only know once we have the geometry
// Easy: we add a feature to the export table as we pass it (if it has exports),
//  not in processXX itself

void ValidatorWorker::addToExportTable(VLocalFeature2D* f)
{
	if (f->bounds && f->bounds->tiles)
	{
		exportTable_.addExport(f, f->bounds->bounds.center());
	}
}


void ValidatorWorker::processWays()
{
	auto iter = iterate<VLocalFeature2D>(SECTION_LOCAL_WAYS);
	while (iter.hasNext())
	{
		VLocalFeature2D* way = iter.next();
		assert(way->isWay() && !way->isForeign());
		// Console::msg("Processing way/%lld", way->id());
		if (!way->isProcessed())
		{
			if (way->tentativeTiles) createBounds(way);
			processWay(way);
		}
		if (way->bounds && way->bounds->tiles)
		{
			exportTable_.addExport(way, way->bounds->bounds.center());
		}
	}
}


void ValidatorWorker::processWay(VLocalFeature2D* way)
{
	//Console::msg("Processing way/%lld", way->id());
	uint64_t nodeTiles = childExports(way);
	const uint8_t* p = way->body;
	uint32_t nodeCount = readVarint32(p) >> 1;
		// bit 0 is closed_ring_flag
	uint64_t prevNodeId = 0;
	Box* pBounds = way->bounds ? &way->bounds->bounds : nullptr;
	for (int i = 0; i < nodeCount; i++)
	{
		uint64_t nodeId = prevNodeId + readSignedVarint64(p);
		prevNodeId = nodeId;
		VNode* node = index_.getNode(nodeId);
		if (node)
		{
			if (pBounds) pBounds->expandToInclude(node->xy);
			if (!node->isForeign())
			{
				node->asLocalNode()->tiles |= nodeTiles;
			}
			node->setFlag(VNode::WAY_NODE);
		}
	}
	way->setFlag(VFeature::PROCESSED);
}

void ValidatorWorker::processRelations()
{
	// We work backwards to ensure that we process parent relations
	// before child relations
	auto iter = iterateReverse<VLocalFeature2D>(SECTION_LOCAL_RELATIONS);
	while (iter.hasNext())
	{
		VLocalFeature2D* rel = iter.next();
		assert(rel->isRelation() && !rel->isForeign());
		// Console::msg("Processing relation/%lld", rel->id());
		if (!rel->isProcessed())
		{
			if (rel->tentativeTiles || rel->isRelationAlwaysExported())
			{
				createBounds(rel);
			}
			processRelation(rel);
		}

		// Relation must be exported if it has member at higher zoom levels,
		// even if it is not referenced by other relations
		if ((rel->bounds && rel->bounds->tiles) || rel->isRelationAlwaysExported())
		{
			exportTable_.addExport(rel, rel->bounds->bounds.center());
		}
	}
}

// TODO: We need to mark relations that need to be exported
// We could determine from members, but that means lookng up non-node
// features; instead let's have Sorter convey this info (because we use
// it to write memberships). Ideally, store flag as top bit of ParentTileLocator
// TODO: Has thix been fixed? (EXPORT_RELATION_ALWAYS flag)

void ValidatorWorker::processRelation(VLocalFeature2D* rel)
{
	uint64_t memberExports = childExports(rel);
	const uint8_t* p = rel->body;
	uint32_t memberCount = readVarint32(p);
	Box* pBounds = rel->bounds ? &rel->bounds->bounds : nullptr;
	for (int i = 0; i < memberCount; i++)
	{
		TypedFeatureId typedMemberId(readVarint64(p));
		ProtoGol::skipString(p);
		VFeature* member;
		if (typedMemberId.isNode())
		{
			member = index_.getFeature(typedMemberId);
			if (member)
			{
				VNode* memberNode = member->asNode();
				if (!memberNode->isForeign())
				{
					memberNode->asLocalNode()->tiles |= memberExports;
				}
				if (pBounds) pBounds->expandToInclude(memberNode->xy);
				memberNode->setFlag(VNode::RELATION_NODE);
			}
		}
		else if (pBounds || memberExports)
		{
			member = index_.getFeature(typedMemberId);
			if (member)
			{
				if (member->isForeign())
				{
					if (pBounds)
					{
						pBounds->expandToIncludeSimple(member->asForeignFeature2D()->bounds);
					}
				}
				else
				{
					VLocalFeature2D* local = member->asLocalFeature2D();
					if (!local->isProcessed())
					{
						createBounds(local);
							// turns tentativeTiles into bounds
						if (local->isWay())
						{
							processWay(member->asLocalFeature2D());
						}
						else
						{
							assert(member->isRelation());
							processRelation(member->asLocalFeature2D());
						}
					}
					local->bounds->tiles |= memberExports;
					if (pBounds)
					{
						pBounds->expandToIncludeSimple(local->bounds->bounds);
					}
				}
			}
		}
	}
	rel->setFlag(VFeature::PROCESSED);
}

void ValidatorWorker::processNodes()
{
	index_.clear();
		// We've previously indexed features by ID; now we need to index
		// local nodes by their location so we can determine which have
		// shared locations

	auto it = iterate<VLocalNode>(SECTION_LOCAL_NODES);
	while (it.hasNext())
	{
		VLocalNode* node = it.next();

		// checkSharedLocation() uses `next`, thereby invalidating
		// the ability to look up nodes by ID (That's why processNodes
		// must be called after processWays and processRelations)

		VLocalNode* otherNode = index_.checkSharedLocation(node);
		if(otherNode)
		{
			if(otherNode->isExported())
			{
				if(!otherNode->isFeatureNode())
				{
					// If the other node is exported, but it wasn't
					// a feature before (and hence was't added to
					// the export table), add it now (because it
					// is a feature node now, since we determined it
					// has a shared location)
					exportTable_.addExport(otherNode, otherNode->xy);
				}
			}
			otherNode->setFlag(VNode::NODE_SHARES_LOCATION);
			node->setFlag(VNode::NODE_SHARES_LOCATION);
		}

		if (node->isExported() && node->isFeatureNode())
		{
			exportTable_.addExport(node, node->xy);
		}
	}
}

void ValidatorWorker::exportNodes()
{
	auto it = iterate<VLocalNode>(SECTION_LOCAL_NODES);
	while (it.hasNext())
	{
		VLocalNode* node = it.next();
		assert((node->tiles & 1) == 0);
			// Node must never be exported to the current tile
		BitIterator iter(node->tiles);
		for (;;)
		{
			int tile = iter.next();
			if (tile < 0) break;

			assert(currentTile_.bounds().contains(node->xy));
			int tex = node->isFeatureNode() ? node->tex : -1;

			pileWriter_.writeForeignNode(tile, node->id(), node->xy, tex);
		}

		bool hasSharedLocation = node->hasSharedLocation();
		bool isOrphan = node->isOrphan();
		bool isUntaggedMember = node->isRelationMember() &
			!node->hasTags();
		bool isSpecial = hasSharedLocation | isOrphan |
			isUntaggedMember;

		if(isSpecial)	[[unlikely]]
		{
			int specialNodeFlags =
				static_cast<int>(hasSharedLocation) |
					(static_cast<int>(isOrphan) << 1);
			pileWriter_.writeSpecialNode(node->id(), specialNodeFlags);
		}

		// We need to write untagged nodes that
		// belong to relations as "special nodes", so they are
		// promoted to feature status. Currently, we will promote
		// them as we encounter them while processing relations,
		// but if they are part of a way that is processed before
		// (since it may be part of a relation that is processed first,
		// or comes before the node in the same relation), the way
		// will erroneously treat the node as an anon node

		// TODO: Is this fixed by change above? (made 2/15/25)

		// TODO: If a node is a duplicate, we may need to track
		//  whether it belongs to a way; otherwise, duplicate
		//  nodes that belong only to foreign ways may not be
		//  marked as waynodes, since processing of ghost ways
		//  (which causes foreign ndoes ot be waynode-marked)
		//  happens before duplicate nodes are promotoed to
		//  feature status

	}
	pileWriter_.closePiles();
}


void ValidatorWorker::exportFeatures(int section)
{
	assert(section == SECTION_LOCAL_WAYS || section == SECTION_LOCAL_RELATIONS);
	bool isOddTile = ValidatorTask::isOdd(currentTile_);
	auto it = iterate<VLocalFeature2D>(section);
	while (it.hasNext())
	{
		VLocalFeature2D* feature = it.next();
		VLocalBounds* bounds = feature->bounds;
		if (bounds)
		{
			BitIterator iter(bounds->tiles);
			for (;;)
			{
				int tile = iter.next();
				if (tile < 0) break;
				bool exportBounds = feature->twinCode() == 0 || isOddTile;
					// For multi-tile features, export bounds only when its
					// odd tile is being processed, because that's when its
					// geometry will be complete
				pileWriter_.writeForeignFeature(tile, section, feature->id(), 
					exportBounds ? bounds->bounds : Box(), feature->tex);
			}
		}
	}
	pileWriter_.closePiles();
}

void Validator::validate()
{
	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		Console::log("Started validating");
	}
	Console::get()->setTask("Validating...");

	const TileCatalog& tc = builder_->tileCatalog();
	int tileCount = builder_->tileCatalog().tileCount();
	std::vector<ValidatorTask> tasks;
	tasks.reserve(tileCount);
	for (int pile = 1; pile <= tileCount; pile++)
	{
		tasks.emplace_back(tc.tileOfPile(pile), pile);
		// Pile numbers start at 1, not 0
	}
	std::sort(tasks.begin(), tasks.end());

	int batchCount = 0;
	constexpr int MAX_BATCHES = 25; // 2 each for levels 1-12, and zoom 0
	int batchSizes[25] = {};

	int prevBatchId = -1;
	for (auto task : tasks)
	{
		if (task.batchId() != prevBatchId)
		{
			batchCount++;
			prevBatchId = task.batchId();
		}
		batchSizes[batchCount-1]++;
	}
	assert(batchCount >= 1);
	assert(batchCount < MAX_BATCHES);

	start();

	ValidatorTask* pTask = tasks.data();
	for (int currentBatch = 0; currentBatch < batchCount; currentBatch++)
	{
		batchCountdown_ = batchSizes[currentBatch];
		for (int i = 0; i < batchSizes[currentBatch]; i++)
		{
			postWork(std::move(*pTask++));
		}
		awaitBatchCompletion();
	}
	end();
	exportsWriter_.close();
}


void Validator::awaitBatchCompletion()
{
	std::unique_lock lock(countdownMutex_);
	batchCompleted_.wait(lock, [&] { return batchCountdown_ == 0; });
}


void Validator::processTask(ValidatorOutputTask& task)
{
	task.piles_.writeTo(builder_->featurePiles());
	exportsWriter_.write(task.pile_, std::move(task.foreignRelations_));
	builder_->progress(workPerTile_);

	std::unique_lock lock(countdownMutex_);
	if (--batchCountdown_ == 0)
	{
		// Console::msg("Batch completed.");
		batchCompleted_.notify_one();
	}
}