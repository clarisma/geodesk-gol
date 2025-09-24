// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Compiler.h"

#include <clarisma/io/FileTime.h>
#include <clarisma/util/log.h>
#include <geodesk/geom/LonLat.h>
#include "build/GolBuilder.h"
#include "tile/compiler/IndexSettings.h"
#include "tile/compiler/NodeTableWriter.h"
#include "tile/model/Layout.h"
#include "tile/model/Membership.h"
#include "tile/model/MutableFeaturePtr.h"
#include "tile/model/THeader.h"
#include "tile/model/TNode.h"
#include "tile/model/TWay.h"
#include "tile/model/TRelation.h"
#include "PropertyTableBuilder.h"
#include "RelationBodyBuilder.h"
#include "RelationTableBuilder.h"
#include "geodesk/feature/TileIndexEntry.h"

// TODO: We should add features to the TIndex as we read them; their geometries
// don't need to be complete, they're just being grouped by type and category
// This way, we won't need to look up the tagtable again
// --> but then how do we iterate them by type? we cannot use `next` because it 
// is used to form a chain in the index buckets

CompilerWorker::CompilerWorker(Compiler* compiler) :
	compiler_(compiler),
	strings_(compiler->builder_->stringCatalog()),
	tileCatalog_(compiler->builder_->tileCatalog()),
	tagsBuilder_(tile_, compiler->areaClassifier_, compiler->builder_->stringCatalog()),
	tileMinX_(0),
	tileMaxY_(0),
	duplicateTags_(nullptr),
	orphanTags_(nullptr),
	includeWayNodeIds_(compiler->builder_->settings().includeWayNodeIds())
{
}

void CompilerWorker::processTask(int pile)
{
	// TODO
	Tile tile = tileCatalog_.tileOfPile(pile);
	Tip tip = tileCatalog_.tipOfPile(pile);
	LOGS << "Compiling " << tile << " (" << tip << ")\n";
	compiler_->builder_->featurePiles().load(pile, data_);
	tile_.init(tile, data_.size());
	Box tileBounds = tile_.bounds();
	tileMinX_ = tileBounds.minX();
	tileMaxY_ = tileBounds.maxY();
	readTile();
	buildRelations();
	buildWays();
	buildNodes();

	const BuildSettings& settings = compiler_->builder_->settings();
	FeatureStore::IndexedKeyMap keysToCategories = settings.keysToCategories();
	IndexSettings indexSettings(keysToCategories,
		settings.rtreeBranchsize(), settings.maxKeyIndexes(),
		settings.keyIndexMinFeatures());
	THeader indexer(indexSettings);
	indexer.addFeatures(tile_);
		// TODO: more efficient to use the lists
	indexer.setExportTable(tile_.exportTable());
	indexer.build(tile_);

	Layout layout(tile_);
	indexer.place(layout);
	layout.flush();
	layout.placeBodies();

	uint8_t* newTileData = tile_.write(layout);
	compiler_->postOutput(CompilerOutputTask(tip,
		ByteBlock(std::move(std::unique_ptr<uint8_t[]>(newTileData)),
			static_cast<size_t>(layout.size() + 4))));
			// TODO: cleanup
	#ifdef GOL_BUILD_STATS
	compiler_->addStats(stats_);
	stats_.clear();
	#endif
	reset();
}


void CompilerWorker::node(uint64_t id, Coordinate xy, ByteSpan protoTags)
{
	if(!tile_.bounds().contains(xy))
	{
		LOGS << "node/" << id << " (" << LonLat(xy)
			<< ") is not in tile bounds (" << tile_.bounds() << ")\n";
		assert(false);
	}

	if (protoTags.isEmpty())
	{
		// Untagged node: Store only its coordinates
		coords_[id] = xy;
	}
	else
	{
		TNode* node = tile_.createFeature<TNode, SNode>(id);
		MutableFeaturePtr pFeature(node->feature());
		pFeature.setNodeXY(xy);
		pFeature.setTags(node->handle(), readTags(protoTags, false));
		// TODO: Don't build tagtable yet, we may still receive
		//  special-node markers for shared_location & orphan,
		//  for which we need to generate geodesk::duplicate (optional)
		//  and/or geodesk::orphan
		// But where do we stash the pointer??
		// We've already created the stub, so we can't use data(),
		// and TNode does not have a body with its own data ptr
		// but duplicates and orphans don't have tagtables!
		// just set tagtable to <null> and deal with it later?
		nodes_.addHead(node);
	}
}

void CompilerWorker::way(uint64_t id, ParentTileLocator locator, ByteSpan body)
{
	if (locator.zoomDelta() > 0)
	{
		const uint8_t* p = body.data();
		int nodeCount = static_cast<int>(readVarint32(p) >> 1);
		int64_t nodeId = 0;
		for(int i=0; i<nodeCount; i++)
		{
			nodeId += readSignedVarint64(p);
			TNode* node = tile_.getNode(nodeId);
			if(node)
			{
				MutableFeaturePtr(node->feature()).setFlag(FeatureFlags::WAYNODE, true);
			}
		}
		assert(p == body.end());
		return;
	}

	TWay* way = tile_.createFeature<TWay, SFeature>(id);

	// Stash the ProtoGol-encoded body data; we'll build the actual body later
	way->body()->setData(body.data());
	way->body()->setSize(body.size());
	ways_.addHead(way);
}

void CompilerWorker::relation(uint64_t id, ParentTileLocator locator, ByteSpan body)
{
	TRelation* rel = tile_.createFeature<TRelation,SFeature>(id);

	// Stash the ProtoGol-encoded body data; we'll build the actual body later
	rel->body()->setData(body.data());
	rel->body()->setSize(body.size());
	relations_.addHead(rel);
		// Local relations are always ordered by level (children before parents)
		// The head-first order ensures the topmost relations are at the head 
		// of the list; later, we can build the relation body and its own reltable 
		// (if any) in the same step, because we can be assured that no more
		// parent relations will be added to a relation's reltable (because we'll
		// build parents before children)
}

void CompilerWorker::membership(uint64_t relId, ParentTileLocator locator, TypedFeatureId typedMemberId)
{
	TFeature* member = tile_.getFeature(typedMemberId);
	if (!member) [[unlikely]]
	{
		if(typedMemberId.isNode())
		{
			// Upgrade an untagged node to TFeature if it is a member
			// of a foreign relation
			member = promoteAnonymousMemberNode(typedMemberId.id());
		}
		else
		{
			// TODO: SAFEMODE: The Feature must exist; otherwise, there's a bug in the Sorter
			LOGS << typedMemberId << " exported by relation/" << relId << " not found locally.";
			assert(false);
		}
	}

	ForeignFeatureRef ref = compiler_->lookupForeignRelation(
		tile_.tile(), locator, relId);
	member->addMembership(tile_.arena().create<Membership>(relId, ref));
}


void CompilerWorker::foreignNode(uint64_t id, Coordinate xy, ForeignFeatureRef ref)
{
	if(ref.isNull())
	{
		// If the exported node is not a feature, treat it as a
		// simple coordinate
		coords_[id] = xy;
#ifdef GOL_BUILD_STATS
		stats_.importedNodeCount++;
#endif
		return;
	}

	auto it = foreignNodes_.find(id);
	if (it != foreignNodes_.end())
	{
		LOGS << "Duplicate foreign node/" << id << " (old: " << it->second
			<< " @ " << LonLat(it->second.xy) << ", new"
			<< ref << LonLat(xy);
	}
	foreignNodes_.emplace(id, ForeignNode(ref, xy));

#ifdef GOL_BUILD_STATS
	stats_.importedFeatureCount++;
#endif
}

void CompilerWorker::foreignFeature(FeatureType type, uint64_t id, const Box& bounds, ForeignFeatureRef ref)
{
	ForeignFeature& ff = foreignFeatures_[TypedFeatureId::ofTypeAndId(type, id)];
	if (!bounds.isEmpty())
	{
		assert(ff.bounds.isEmpty());
		ff.bounds = bounds;
	}
	ForeignFeatureRef& ffRef = (ff.ref1.isNull()) ? ff.ref1 : ff.ref2;
	assert(ffRef.isNull());
	ffRef = ref;
	assert(!ff.ref1.isNull());

#ifdef GOL_BUILD_STATS
	stats_.importedFeatureCount++;
#endif
}



void CompilerWorker::specialNode(uint64_t id, int specialNodeFlags)
{
	TNode* node = tile_.getNode(id);
	if (!node)
	{
		node = promoteAnonymousMemberNode(id);
	}
	if(!node->tags(tile_)->tags().isEmpty())
	{
		// TODO: tags() is inefficient, since it performs
		//  a lookup; better to cache the handle of the
		//  empty tag table and compare it directly
		if(specialNodeFlags != 0 &&
			specialNodeFlags != ProtoGol::SpecialNodeFlags::SHARED)
		{
			LOGS << "Tagged node/" << id << " has special flags " << specialNodeFlags;
		}
		assert(specialNodeFlags == 0 ||
			specialNodeFlags==ProtoGol::SpecialNodeFlags::SHARED);

		MutableFeaturePtr pFeature(node->feature());
		pFeature.setFlag(FeatureFlags::SHARED_LOCATION,
			specialNodeFlags & ProtoGol::SpecialNodeFlags::SHARED);
	}
	else
	{
		MutableFeaturePtr pFeature(node->feature());
		if(specialNodeFlags & ProtoGol::SpecialNodeFlags::SHARED)
		{
			// TODO: Can we be sure that "geodesk:" keys are always local strings?
			tagsBuilder_.addLocalTag("geodesk:duplicate", GlobalStrings::YES);
			pFeature.setFlag(FeatureFlags::SHARED_LOCATION |
				FeatureFlags::EXCEPTION_NODE, true);
		}
		if(specialNodeFlags & ProtoGol::SpecialNodeFlags::ORPHAN)
		{
			// TODO: Can we be sure that "geodesk:" keys are always local strings?
			tagsBuilder_.addLocalTag("geodesk:orphan", GlobalStrings::YES);
			pFeature.setFlag(FeatureFlags::EXCEPTION_NODE, true);
		}
		pFeature.setTags(node->handle(), tagsBuilder_.getTagTable(false));

		// TODO: make more efficient by using cached tag-tables
		//  (duplicates/orphans are typically in clusters)
	}
}

void CompilerWorker::readExportTable(int count, const uint8_t*& p)
{
	assert(count);
	TypedFeatureId* exports = tile_.arena().allocArray<TypedFeatureId>(count);
	int64_t typedId = 0;
	for(int i=0; i<count; i++)
	{
		typedId += readSignedVarint64(p);
		exports[i] = TypedFeatureId(typedId);

		// Cannot resolve nodes yet, because some are
		// anonymous nodes that are upgraded to feature nodes
		// once relations are processed
		// TODO: Check this, upgrading is now done by
		//  readSpecialNodes()
		//  but looks like export table is written *before*
		//  special nodes
	}
	tile_.createExportTable(nullptr, exports, count);

	#ifdef GOL_BUILD_STATS
	stats_.grossExportedFeatureCount = count;
	#endif
}

void CompilerWorker::setBounds(MutableFeaturePtr feature, const Box& bounds)
{
	feature.setBounds(bounds);
	feature.setFlag(FeatureFlags::MULTITILE_WEST, bounds.minX() < tileMinX_);
	feature.setFlag(FeatureFlags::MULTITILE_NORTH, bounds.maxY() > tileMaxY_);
}

void CompilerWorker::buildWay(TWay* way)
{
	assert(!way->isBuilt());
	TWayBody* wayBody = way->body();
	const uint8_t* p = wayBody->data();
	const uint8_t* pEnd = p + wayBody->size();
	uint32_t taggedNodeCount = readVarint32(p);
	int nodeCount = static_cast<int>(taggedNodeCount >> 1);
	bool isClosedRing = taggedNodeCount & 1;
	uint32_t relTablePtrSize = way->firstMembership() ? 4 : 0;

	MutableFeaturePtr pWay(way->feature());

	assert(wayNodes_.empty());
	wayNodes_.reserve(nodeCount);

	const uint8_t* pWayNodeIds = p;
	Box bounds;
	int64_t nodeId = 0;
	int featureNodeCount = 0;
	for (int i = 0; i < nodeCount; i++)
	{
		nodeId += readSignedVarint64(p);
		auto it = coords_.find(nodeId);
		if (it != coords_.end())
		{
			// Plain coordinate (local or foreign) -- most likely case
			wayNodes_.emplace_back(FeatureRef(), it->second);
		}
		else
		{
			TNode* local = tile_.getNode(nodeId);
			if (local)
			{
				MutableFeaturePtr(local->feature()).setFlag(FeatureFlags::WAYNODE, true);
				wayNodes_.emplace_back(local, local->xy());
			}
			else
			{
				// Must be a foreign feature node
				auto it = foreignNodes_.find(nodeId);
				if (it == foreignNodes_.end())
				{
					// TODO: We have a problem
					assert(false);
				}
				wayNodes_.emplace_back(it->second);
			}
			featureNodeCount++;
		}
		bounds.expandToInclude(wayNodes_.back().xy);
	}
	const uint8_t* pWayNodeIdsEnd = p;

	assert(wayNodes_.size() == nodeCount);
	assert(featureNodeCount <= nodeCount);
	assert(!bounds.isEmpty());
	assert(bounds.intersects(tile_.bounds()));

	if (way->id() == 254596487)
	{
		LOGS << "!!!";
	}

	setBounds(pWay, bounds);
	TTagTable* tags = readTags(ByteSpan(p, pEnd), isClosedRing);
		// only check if the TagTable has area-tags if the way is a closed ring
	pWay.setTags(way->handle(), tags);
	bool isArea = isClosedRing && tags->isArea(false);
		// We have to explicitly check for isClosedRing again, because
		// to be an area, the way must have area-tags AND be a closed ring
	pWay.setFlag(FeatureFlags::AREA, isArea);

	// We pre-allocate space for the way's body using the most conservative
	// assumptions:
	// - Each feature node is a foreign feature node, with wide tip/tex
	//   deltas (8 bytes each)
	// - Each coordinate pair requires 10 bytes to varint-encode
	// - We assume one extra node (first node duplicated for non-area closed ways)
	// - Optional: space for waynode IDs (copied verbatim from Proto-GOL)
	//   (We add 8 to account for the duplicate encoding of the first ID for
	//    non-area closed rings; this is the max size of a 52-bit varint)

	size_t wayNodeIdsSize = pWayNodeIdsEnd-pWayNodeIds;
	size_t maxWayNodeIdsSize = includeWayNodeIds_ ?
		(wayNodeIdsSize + 8) : 0;
	uint32_t maxBodySize = (nodeCount + 1) * 10 + (featureNodeCount + 1) * 8
		+ relTablePtrSize + maxWayNodeIdsSize;
	uint8_t* pBodyStart = tile_.arena().alloc(maxBodySize, 2);
	TElement::Handle bodyHandle = tile_.newHandle();
	uint8_t* pCoords = pBodyStart + relTablePtrSize;

	bool needsFixup = false;
	if(featureNodeCount)
	{
		pWay.setFlag(FeatureFlags::WAYNODE, true);
		uint8_t* pTempNodeTableEnd = pBodyStart + maxBodySize;
		NodeTableWriter writer(bodyHandle - relTablePtrSize, pTempNodeTableEnd);
			// TODO: Check if this handle adjustment is needed
		Tip prevTip;
		Tex prevTex = Tex::WAYNODES_START_TEX;

		for(int i=0; i<nodeCount; i++)
		{
			const WayNode& wayNode = wayNodes_[i];
			if(!wayNode.isNull())
			{
				if(wayNode.isForeign())
				{
					if(wayNode.tip() != prevTip)
					{
						if(prevTip.isNull()) prevTip = FeatureConstants::START_TIP;
						// Remember, DIFFERENT_TILE flag must
						// be set for first node even if its TIP
						// is the same as the starting TIP
						writer.writeForeignNode(wayNode.tip()-prevTip, wayNode.tex()-prevTex);
						prevTip = wayNode.tip();
					}
					else
					{
						writer.writeForeignNode(wayNode.tex()-prevTex);
					}
					prevTex = wayNode.tex();
				}
				else
				{
					writer.writeLocalNode(wayNode.local());
					needsFixup = true;
				}
			}
		}

		if(isClosedRing)
		{
			// If first node is a feature node and the way forms a closed loop,
			// and the first node is a feature node, repeat it as the last node
			// (it does not matter whether the way is an area; this differs
			// from the way we treat coordinates, where the first coordinate
			// is only repeated as the last for a way that forms a closed loop,
			// but is not an area)

			WayNode& firstNode = wayNodes_[0];
			if(!firstNode.isNull())
			{
				if(firstNode.isForeign())
				{
					if(firstNode.tip() != prevTip)
					{
						writer.writeForeignNode(firstNode.tip()-prevTip, firstNode.tex()-prevTex);
					}
					else
					{
						writer.writeForeignNode(firstNode.tex()-prevTex);
					}
				}
				else
				{
					writer.writeLocalNode(firstNode.local());
				}
			}
		}

		writer.markLast();
		uint8_t* pTempNodeTableStart = writer.ptr().ptr();
		assert(pTempNodeTableStart >= pBodyStart);
		// Make sure we didn't write the feature-node table beyond the buffer start
		size_t nodeTableSize = pTempNodeTableEnd - pTempNodeTableStart;
		assert(nodeTableSize >= 4);
		// Move the feature-node table into its proper place
		memmove(pBodyStart, pTempNodeTableStart, nodeTableSize);
		pCoords += nodeTableSize;
		#ifdef GOL_BUILD_STATS
		stats_.grossFeatureWayNodeCount += writer.memberCount;
		stats_.grossForeignWayNodeCount += writer.foreignMemberCount;
		stats_.grossWideTexWayNodeCount += writer.wideTexMemberCount;
		#endif
	}

	// Encode the coordinates
	bool repeatFirstCoord = isClosedRing && !isArea;
	const uint8_t* pCoordsStart = pCoords;
	writeVarint(pCoords, nodeCount + repeatFirstCoord);
	Coordinate prevXY = bounds.bottomLeft();
	for (int i = 0; i < nodeCount; i++)
	{
		Coordinate xy = wayNodes_[i].xy;
		writeSignedVarint(pCoords, static_cast<int64_t>(xy.x) - prevXY.x);
		writeSignedVarint(pCoords, static_cast<int64_t>(xy.y) - prevXY.y);
		prevXY = xy;
	}
	if (repeatFirstCoord)
	{
		// For a way that is a closed ring but not an area, we
		// need to append the first coordinate as the last coordinate
		Coordinate firstXY = wayNodes_[0].xy;
		writeSignedVarint(pCoords, static_cast<int64_t>(firstXY.x) - prevXY.x);
		writeSignedVarint(pCoords, static_cast<int64_t>(firstXY.y) - prevXY.y);
	}

	// Write the optional waynode IDs
	if(includeWayNodeIds_)
	{
		// pCoords points to the next byte after the coordinates,
		// which is where we'll place the waynode IDs
		memcpy(pCoords, pWayNodeIds, wayNodeIdsSize);
		pCoords += wayNodeIdsSize;
		if(repeatFirstCoord)
		{
			// nodeId holds the last ID we've read
			// we need to write the delta between this ID
			// and the first ID
			p = pWayNodeIds;
			int64_t firstNodeId = readSignedVarint64(p);
			writeSignedVarint(pCoords, firstNodeId - nodeId);
		}
	}

	size_t trueBodySize = pCoords - pBodyStart;
	assert(trueBodySize <= maxBodySize);
	// Ensure we did not write past end of the allocated space
	tile_.arena().reduceLastAlloc(maxBodySize - trueBodySize);

	uint32_t anchor = pCoordsStart - pBodyStart;
	wayBody->setHandle(bodyHandle);
	wayBody->setData(pBodyStart + anchor);
	wayBody->setSize(trueBodySize);
	wayBody->setAnchor(anchor);
	wayBody->setFlag(TWayBody::Flags::NEEDS_FIXUP, needsFixup);
	wayBody->setAlignment(anchor ? TElement::Alignment::WORD : TElement::Alignment::BYTE);

	wayNodes_.clear();
	way->setFlag(TWay::Flags::BUILT, true);

	#ifdef GOL_BUILD_STATS
	stats_.grossWayNodeCount += nodeCount + repeatFirstCoord;
	#endif;
}

void CompilerWorker::buildNodes()
{
	auto iter = nodes_.iter();
	while (iter.hasNext())
	{
		TNode* node = iter.next();
		Membership* firstMembership = node->firstMembership();
		assert(node->size() == 20);
		if(firstMembership)
		{
			TRelationTable* rels = RelationTableBuilder::build(
				tile_, firstMembership);
			node->setParentRelations(rels);
			// TODO: usage count
			node->setSize(24);
		}
		#ifdef GOL_BUILD_STATS
		stats_.featureNodeCount++;
		#endif
	}
}

void CompilerWorker::buildWays()
{
	auto iter = ways_.iter();
	while (iter.hasNext())
	{
		TWay* way = iter.next();
		if(!way->isBuilt()) buildWay(way);
		buildRelationTable(way);
		#ifdef GOL_BUILD_STATS
		stats_.grossWayCount++;
		#endif
	}
}

void CompilerWorker::buildRelations()
{
	auto iter = relations_.iter();
	while (iter.hasNext())
	{
		TRelation* rel = iter.next();
		if(!rel->isBuilt()) buildRelation(rel);
		buildRelationTable(rel);
		#ifdef GOL_BUILD_STATS
		stats_.grossRelationCount++;
		#endif
	}
}


void CompilerWorker::buildRelationTable(TFeature2D* feature)
{
	Membership* firstMembership = feature->firstMembership();
	if(!firstMembership) return;
	TRelationTable* rels = RelationTableBuilder::build(tile_, firstMembership);
	feature->setParentRelations(rels);
	// TODO: usage count
}


TNode* CompilerWorker::promoteAnonymousMemberNode(uint64_t nodeId)
{
	// We use extract to remove the node's coordinates from
	// coords_, since coords_ must only contain the coordinates of
	// anonymous nodes (buildWay() checks coords_ first since anon
	// nodes are the most likely case; if we leave the coordinates
	// in coords_, buildWay() will miss the fact that the node
	// is now a feature node, and won't add it to its node table)

	auto entry = coords_.extract(nodeId);
	if (entry.empty())		[[unlikely]]
	{
		Console::msg("Missing local node/%lld", nodeId);
		assert(false);
	}
	TNode* node = tile_.createFeature<TNode, SNode>(nodeId);
	// TODO: different struct (with pointer to rels)
	MutableFeaturePtr pFeature(node->feature());
	pFeature.setNodeXY(entry.mapped());
	pFeature.setTags(node->handle(), readTags(ByteSpan(), false));
		// TODO: make more efficient, can cache empty tagtable
	nodes_.addHead(node);

	return node;
}

/// Expands the given bbox to include the bounds of the given
/// feature, building it if necessary.
///
void CompilerWorker::addToBounds(TFeature* f, Box& bounds)
{
	FeatureType type = f->featureType();
	if(type == FeatureType::WAY)
	{
		TWay* way = static_cast<TWay*>(f);	// NOLINT safe cast
		if(!way->isBuilt()) buildWay(way);
		bounds.expandToIncludeSimple(way->feature().bounds());
	}
	else if (type == FeatureType::NODE)
	{
		TNode* node = static_cast<TNode*>(f);  // NOLINT safe cast
		bounds.expandToInclude(node->xy());
	}
	else
	{
		assert(type == FeatureType::RELATION);
		TRelation* rel = static_cast<TRelation*>(f);  // NOLINT safe cast
		if(!rel->isBuilt()) buildRelation(rel);
		bounds.expandToIncludeSimple(rel->feature().bounds());
	}
}

// TODO: We still have a nasty edge-case bug: what about untagged nodes
//  that are part of a relation, and hence will need to be upgraded to
//  feature nodes? This is handled by this method, which will call
//  promoteAnonymousMemberNode() to create a feature node from coords_;
//  but what happens if a way that also has this node as a waynode
//  also belongs to a relation, but is built *before* the node is
//  encountered and transformed into a feature node? In that case,
//  the node will not be included in the way's node table be buildWay()
//  (because the node is still anonymous at the time when it is called)
//  We need to ensure all node promotions are completed *before* building
//  ways (Note that this works find for foreign nodes, since the Validator
//  will determine that they are features). Maybe make Validator also mark
//  local nodes that are upgraded to feature status, so we won't need an
//  extra pass in Compiler (Validator already checks local nodes for
//  duplicate/orphan status)
//  - maybe add an extra flag to SpecialNode, to indicate upgrade?
// TODO: assert member bboxes are non-empty
// TODO: Is this fixed by change to Validator::exportNodes() ? (made 2/15/25)
void CompilerWorker::buildRelation(TRelation* rel)
{
	assert(!rel->isBuilt());
	const uint8_t* p = rel->body()->data();
	const uint8_t* pEnd = p + rel->body()->size();

	Box bounds;
	std::string_view prevRoleString;
	TString* localRoleStr = nullptr;
	uint32_t memberCount = readVarint32(p);
	assert(memberCount > 0);
	auto members = tile_.arena().allocSpan<RelationMember>(memberCount);
	RelationBodyBuilder relBodyBuilder(members);
	bool hasOuterMember = false;
	for (int i = 0; i < memberCount; i++)
	{
		TypedFeatureId typedMemberId = TypedFeatureId(readVarint64(p));
		std::pair<int, std::string_view> rolePair = ProtoGol::readRoleString(p, strings_);
		if (rolePair.second != prevRoleString)
		{
			prevRoleString = rolePair.second;
			localRoleStr = (rolePair.first < 0) ? tile_.addString(prevRoleString) : nullptr;
		}
		if(rolePair.first == GlobalStrings::OUTER) hasOuterMember = true;
		Role role(rolePair.first, localRoleStr);
		TFeature* local = tile_.getFeature(typedMemberId);

		if (local)
		{
			// local member
			local->addMembership(tile_.arena().create<Membership>(rel));
				// Must add the membership before call to addToBounds(),
				// because otherwise space for the reltable pointer may
				// not be properly allocated when addToBounds() builds
				// the member feature on demand
			relBodyBuilder.addLocal(local, role);
			addToBounds(local, bounds);
		}
		else
		{
			if (typedMemberId.isNode())
			{
				Coordinate xy;
				auto it = foreignNodes_.find(typedMemberId.id());
					// lookup is just by ID (without type)
				if (it == foreignNodes_.end())
				{
					TNode* localNode = promoteAnonymousMemberNode(typedMemberId.id());
					assert(localNode);
					relBodyBuilder.addLocal(localNode, role);
					xy = localNode->xy();
					localNode->addMembership(tile_.arena().create<Membership>(rel));
				}
				else
				{
					relBodyBuilder.addForeign(it->second, {}, role);
					xy = it->second.xy;
				}
				bounds.expandToInclude(xy);
			}
			else
			{
				auto it = foreignFeatures_.find(typedMemberId);
				if (it == foreignFeatures_.end())
				{
					Console::msg("relation/%lld: Missing member %s",
						rel->id(), typedMemberId.toString().c_str());
					assert(false);
				}
				// TODO: choose ref depending on parent tile overlap
				relBodyBuilder.addForeign(it->second.ref1, it->second.ref2, role);
				bounds.expandToIncludeSimple(it->second.bounds);
			}
		}
	}
	// LOGS << "Building body of relation/" << rel->id();
	relBodyBuilder.build(tile_, rel->body(), rel->firstMembership());

	MutableFeaturePtr pFeature(rel->feature());
	setBounds(pFeature, bounds);
	assert(!bounds.isEmpty());
	TTagTable* tags = readTags(ByteSpan(p, pEnd), hasOuterMember);
	pFeature.setTags(rel->handle(), tags);
	bool isArea = hasOuterMember && tags->isArea(true);
	// We have to explicitly check for hasOuterMember again, because
	// to be an area, the relation must have area-tags AND at least one
	// member whose role is "outer"
	pFeature.setFlag(FeatureFlags::AREA, isArea);
	rel->setFlag(TRelation::Flags::BUILT, true);
	// Careful here, feature flags vs. tile model element flags

	#ifdef GOL_BUILD_STATS
	stats_.grossMemberCount += relBodyBuilder.memberCount;
	stats_.grossForeignMemberCount += relBodyBuilder.foreignMemberCount;
	stats_.grossWideTexMemberCount += relBodyBuilder.wideTexMemberCount;
	#endif
}

void CompilerWorker::reset()
{
	tile_.clear();
	coords_.clear();
	nodes_.clear();
	ways_.clear();
	relations_.clear();
	foreignNodes_.clear();
	foreignFeatures_.clear();
	duplicateTags_ = nullptr;
	orphanTags_ = nullptr;
}


Compiler::Compiler(GolBuilder* builder) :
	TaskEngine(builder->threadCount()),
	builder_(builder),
	areaClassifier_(
		builder->settings().areaRules(),
		[this](std::string_view str)
		{
			return builder_->stringCatalog().getGlobalCode(str);
		}),
	workPerTile_(builder->phaseWork(GolBuilder::Phase::COMPILE)
		/ builder->tileCatalog().tileCount()),
	exportFile_(builder->workPath() / "exports.bin"),
	transaction_(store_)
{
}

std::unique_ptr<uint32_t[]> Compiler::createIndexedKeySchema() const
{
	const StringCatalog& strings = builder_->stringCatalog();
	const std::vector<IndexedKey>& indexedKeys = builder_->settings().indexedKeys();
	uint32_t count = static_cast<uint32_t>(indexedKeys.size());
	uint32_t* schema = new uint32_t[count + 1];
	uint32_t* p = schema;
	*p++ = count;
	for(IndexedKey key: indexedKeys)
	{
		int keyCode = strings.getGlobalCode(key.key);
		*p++ = static_cast<uint32_t>(keyCode | (key.category << 16));
	}
	return std::unique_ptr<uint32_t[]>(schema);
}

void Compiler::initStore()
{
	PropertyTableBuilder props;
	const OsmPbfMetadata& osmMetadata = builder_->metadata();
	props.add("source", osmMetadata.source);
	props.add("copyright", "(C) OpenStreetMap contributors");
	props.add("license", "Open Database License 1.0");
	ByteBlock propsBlock = props.take();

	const BuildSettings& buildSettings = builder_->settings();
	FeatureStore::Settings settings;
	settings.zoomLevels = buildSettings.zoomLevels();
	settings.reserved = 0;
	settings.rtreeBranchSize = buildSettings.rtreeBranchsize();
	settings.rtreeAlgo = 0; // TODO
	settings.maxKeyIndexes = buildSettings.maxKeyIndexes();
	settings.keyIndexMinFeatures = buildSettings.keyIndexMinFeatures();

	FeatureStore::Metadata metadata(clarisma::UUID::create());
	metadata.flags |= buildSettings.includeWayNodeIds() ?
		FeatureStore::Header::Flags::WAYNODE_IDS : 0;
	metadata.settings = &settings;
	metadata.revision = osmMetadata.replicationSequence;
	metadata.revisionTimestamp = osmMetadata.replicationTimestamp;
	if(osmMetadata.replicationTimestamp == 0)
	{
		// If no replication timestamp is provided,
		//  use file creation time instead
		FileTime ft(builder_->settings().sourcePath().c_str());
		metadata.revisionTimestamp = ft.created();
	}
	std::unique_ptr<const uint32_t[]> indexedKeys = createIndexedKeySchema();
	ByteBlock stringTable = builder_->stringCatalog().createGlobalStringTable();
	metadata.indexedKeys = indexedKeys.get();
	metadata.stringTable = stringTable.data();
	metadata.stringTableSize = stringTable.size();
	metadata.properties = propsBlock.data();
	metadata.propertiesSize = propsBlock.size();

	store_.open(builder_->golPath().string().c_str(), FreeStore::OpenMode::WRITE |
		FreeStore::OpenMode::CREATE | FreeStore::OpenMode::EXCLUSIVE);
	transaction_.begin();
	transaction_.setup(metadata);
	tileIndex_ = std::move(builder_->takeTileIndex());
}

#ifdef GOL_BUILD_STATS

void Compiler::addStats(const TileStats& stats)
{
	std::lock_guard lock(statsMutex_);
	stats_ += stats;
	reportedTileCount_++;
}

void Compiler::reportStat(const char* label, int64_t count, int64_t baseCount)
{
	ConsoleWriter out;
	char buf[200];
	snprintf(buf, sizeof(buf), "  %-40s %12lld\n", label, count);
	out.timestamp() << buf;
}
#endif

void Compiler::compile()
{
	builder_->console().setTask("Compiling...");
	initStore();
	start();
	int tileCount = builder_->tileCatalog().tileCount();
	for (int i = 0; i < tileCount; i++)
	{
		postWork(i+1);
		// Pile numbers start at 1, not 0
	}
	end();
	builder_->console().setTask("Cleaning up...");

	FeatureStore::Header& header = transaction_.header();

	// builder_->featurePiles().clear();
	uint32_t tipCount = tileIndex_[0];
	uint32_t tileIndexSize = (tipCount + 1) * 4;
	tileIndex_[0] = tileIndexSize - 4;
	header.snapshots[0].tileIndex = transaction_.addBlob(
		{reinterpret_cast<uint8_t*>(tileIndex_.get()), tileIndexSize});
	header.snapshots[0].tileCount = tileCount;
	header.tipCount = tipCount;

	transaction_.commit();
	transaction_.end();
	store_.close();

#ifdef GOL_DIAGNOSTICS
	// std::lock_guard lock(statsMutex_);
	if(Console::verbosity() >= Console::Verbosity::VERBOSE)
	{
		reportStat("Total features:", stats_.featureNodeCount + stats_.grossWayCount + stats_.grossRelationCount);
		reportStat("  of these, feature nodes:", stats_.featureNodeCount);
		reportStat("  of these, ways:", stats_.grossWayCount);
		reportStat("  of these, relations:", stats_.grossRelationCount);
		reportStat("  of these, exported:", stats_.grossExportedFeatureCount);
		reportStat("Total imported features:", stats_.importedFeatureCount);
		reportStat("Total imported nodes:", stats_.importedNodeCount);
		reportStat("Total waynodes:", stats_.grossWayNodeCount);
		reportStat("  of these, features:", stats_.grossFeatureWayNodeCount);
		reportStat("    of these, foreign:", stats_.grossForeignWayNodeCount);
		reportStat("      of these, wide TEX:", stats_.grossWideTexWayNodeCount);
		reportStat("Total relation members:", stats_.grossMemberCount);
		reportStat("  of these, foreign:", stats_.grossForeignMemberCount);
		reportStat("    of these, wide TEX:", stats_.grossWideTexMemberCount);
	}
#endif
}

void Compiler::processTask(CompilerOutputTask& task)
{
	// Console::debug("Saving tile %d", task.tip());
	assert(*reinterpret_cast<const uint32_t*>(task.data().data()) == task.data().size() - 4);
	assert(FeatureStore::isTileValid(reinterpret_cast<const std::byte*>(
		task.data().data())));
	uint32_t page = transaction_.addBlob(task.data());
	tileIndex_[task.tip()] = TileIndexEntry(page, TileIndexEntry::CURRENT);
	builder_->progress(workPerTile_);
	// Console::debug(" Saved tile %d", task.tip());
	assert(FeatureStore::isTileValid(reinterpret_cast<const std::byte*>(
		task.data().data())));
}

ForeignFeatureRef Compiler::lookupForeignRelation(Tile childTile, ParentTileLocator locator, uint64_t id)
{
	Tile tile = childTile.zoomedOut(childTile.zoom() - locator.zoomDelta());
	int pile =  builder_->tileCatalog().pileOfTile(tile);
	Tip tip =  builder_->tileCatalog().tipOfPile(pile);
	Tex tex = exportFile_.texOfRelation(pile, id);
	return ForeignFeatureRef(tip, tex);
}

