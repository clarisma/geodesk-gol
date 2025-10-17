// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "Sorter.h"
#include <cassert>
#include <clarisma/thread/Threads.h>
#include "build/GolBuilder.h"
#include "gol/debug.h"
#include <geodesk/geom/Mercator.h>
#include <geodesk/geom/TilePair.h>
#include "SuperRelationResolver.h"

// TODO: Remember to always flush the BufferWriter, and reset buffer

// TODO: drop the last node from ways that form closed rings, and instead
// encode a marker bit? (Less work to do for Validator and Compiler)
// Over two-thirds of ways are closed rings

SorterWorker::SorterWorker(Sorter* sorter) :
    OsmPbfContext<SorterWorker, Sorter>(sorter),
    builder_(sorter->builder()),
    osmStrings_(nullptr),
    tempBuffer_(4096),
    tempWriter_(&tempBuffer_),
    currentPhase_(0),
    pileWriter_(sorter->builder()->tileCatalog().tileCount()),
    pileCount_(sorter->builder()->tileCatalog().tileCount()),
    batchCount_(0),
    superRelationData_( 1 * 1024),      // TODO
    isMainWorker_(false)
{
    indexes_[0] = FastFeatureIndex(builder_->featureIndex(0));
    indexes_[1] = FastFeatureIndex(builder_->featureIndex(1));
    indexes_[2] = FastFeatureIndex(builder_->featureIndex(2));
}

SorterWorker::~SorterWorker()
{
}

void SorterWorker::stringTable(ByteSpan strings)
{
    assert(stringTranslationTable_.empty());

    // Look up the ProtoStringEncoding for each string in the string table,
    // and store it in the String Translation Table. We have to do this for
    // each OSM block, since the same string (e.g. "highway") may have a
    // different code in each block. When we write the tags, we simply look
    // up the varint that represents the proto-string code for that string
    // (if the string is frequent enough in the entire .osm.pbf file to 
    // warrant inclusion in the Proto-String Table)
    // If the string is not in the Proto-String Table (because it occurs
    // infrequently), we store the offset of the string instead.

    osmStrings_ = strings.data();
    const uint8_t* p = osmStrings_;
    while(p < strings.end())
    {
        uint32_t marker = readVarint32(p);
        if (marker != OsmPbf::STRINGTABLE_ENTRY)
        {
            throw OsmPbfException("Bad string table. Unexpected field: %d", marker);
        }
        const ShortVarString* str = reinterpret_cast<const ShortVarString*>(p);
        stringTranslationTable_.push_back(
            builder_->stringCatalog().protoStringPair(str, osmStrings_));
        p += str->totalSize();
    }
}


void SorterWorker::encodeString(uint32_t stringNumber, int type)
{
    assert(stringNumber < stringTranslationTable_.size());  // TODO: exception?
    ProtoString str = stringTranslationTable_[stringNumber].get(type);
    str.writeTo(tempWriter_, osmStrings_);
}

void SorterWorker::encodeTags(ByteSpan keys, ByteSpan values)
{
    const uint8_t* pKey = keys.data();
    const uint8_t* pValue = values.data();
    while (pKey < keys.end())
    {
        uint32_t key = readVarint32(pKey);
        uint32_t value = readVarint32(pValue);
        encodeString(key, ProtoStringPair::KEY);
        encodeString(value, ProtoStringPair::VALUE);
    }

    // In new Proto-GOL format, we don't write the tag count
    // We just directly encode the tags into the temp buffer
    // The caller can then write the bodyLen and body contents
    // to the tile pile
}


const uint8_t* SorterWorker::encodeTags(ByteSpan tags)
{
    const uint8_t* p = tags.data();
    while (p < tags.end())
    {
        uint32_t key = readVarint32(p);
        if (key == 0) break;
        uint32_t value = readVarint32(p);
        encodeString(key, ProtoStringPair::KEY);
        encodeString(value, ProtoStringPair::VALUE);
    }
        // TODO: This feels hacky; start/end should be immutable, and node()
        // should not have the responsibility to advance start pointer.
        // However, this is the fastest approach
    return p;
}


void SorterWorker::indexFeature(int64_t id, int pile)
{
    assert(currentPhase_ <= 2);
        // Can't use this for super-relations, which are phase 3
        // and use different batching approach
    FastFeatureIndex& index = indexes_[currentPhase_];
    // assert(pile >= 0 && pile <= pileCount_);
    // pile can be a regular pile, or a pile pair
    index.put(id, pile);  
    batchCount_++;
    if (batchCount_ >= batchSize(currentPhase_)) flushPiles();
}

// TODO: Could deadlock if all tasks have been processed, but 
//  ways are still remaining unflushed while other threads are
//  attempting to start relations
//  afterTasks() should solve this


void SorterWorker::afterTasks()
{
    /*
    if (blockBytesProcessed())
    {
        flushPiles();
    }
    flushIndex();
    */
    advancePhase(Sorter::Phase::SUPER_RELATIONS);
    for (int i = 0; i < 2; i++)
    {
        assert(!indexes_[i].hasPendingWrites());
    }

    // TODO: Fix !!!!!!

    // TODO: Must ensure that *all* piles are flushed!!!
    // (We need another phase *after* afterTasks()

    // No, should not be needed because advancePhase() blocks
    // explicit flushing should not be needed, either
    
    // Only one worker is designated as the "main worker": It is responsible
    // for resolving the super-relations
    if (isMainWorker_) resolveSuperRelations();
}

void SorterWorker::advancePhase(int futurePhase)
{
    flushPiles();
    flushIndex();
    reader()->advancePhase(currentPhase_, futurePhase);
    // This will block until all worker threads have switched to futurePhase
    currentPhase_ = futurePhase;
}


void SorterWorker::flushIndex()
{
    assert(currentPhase_ <= 2);
    indexes_[currentPhase_].endBatch();
}

void SorterWorker::flushPiles()
{
    pileWriter_.closePiles();
    SorterOutputTask task(blockBytesProcessed(), std::move(pileWriter_));
    reader()->postOutput(std::move(task));
    resetBlockBytesProcessed();
    batchCount_ = 0;
    // printf("Thread %s: Flushed.\n", Threads::currentThreadId().c_str());
}

const uint8_t* SorterWorker::node(int64_t id, int32_t lon100nd, int32_t lat100nd, ByteSpan tags)
{
    /*
    if (id == 319561907)
    {
        LOG("!!!");
    }
    */
    assert(tempWriter_.isEmpty());
    assert(id < 1'000'000'000'000ULL);
    // project lon/lat to Mercator
    // TODO: clamp range
    Coordinate xy(Mercator::xFromLon100nd(lon100nd), Mercator::yFromLat100nd(lat100nd));
    int pile = builder_->tileCatalog().pileOfCoordinate(xy);
    assert(pile > 0 && pile <= pileCount_);  // pile numbers are 1-based
    if (!pile)
    {
        Console::msg("node/%lld: Unable to assign to tile\n", id);
    }
    const uint8_t* nextTagSet = encodeTags(tags);
    pileWriter_.writeNode(pile, id, xy, tempWriter_);
    tempWriter_.clear();
    indexFeature(id, pile);
    stats_.nodeCount++;
    return nextTagSet;
}


void SorterWorker::beginWayGroup()
{
    if (currentPhase_ != Sorter::Phase::WAYS)
    {
        advancePhase(Sorter::Phase::WAYS);
    }
}

void SorterWorker::way(int64_t id, ByteSpan keys, ByteSpan values, ByteSpan nodes)
{
    assert(tempWriter_.isEmpty());
    encodeTags(keys, values);

    /*
    if (56084429 == id)
    {
        Console::msg("way/%lld", id);
    }
    */

    int64_t nodeId = 0;
    int prevNodePile = 0;
    int nodeCount = 0;
    int pileDiversity = 0;
    int64_t firstNodeId = 0;
    
    const uint8_t* p = nodes.data();
    while (p < nodes.end())
    {
        nodeId += readSignedVarint64(p);
        firstNodeId = firstNodeId == 0 ? nodeId : firstNodeId;
        int nodePile = indexes_[0].get(nodeId);
        assert(nodePile >= 0 && nodePile <= pileCount_);  // pile numbers are 1-based
        if (nodePile == 0) [[unlikely]]
        {
            Console::msg("node/%lld not found in node index", nodeId);
        }
        pileDiversity += (nodePile != prevNodePile) ? 1 : 0;
        if (pileDiversity > 1)
        {
            multiTileWay(id, nodes);
            return;
        }
        prevNodePile = nodePile;
        nodeCount++;
    }

    if(nodeCount < 2) [[unlikely]]
    {
        if(Console::verbosity() >= Console::Verbosity::VERBOSE)
        {
            Console::msg("Rejected way/%lld with %lld nodes", id, nodeCount);
        }
        tempWriter_.clear();
        return;
    }

    bool isClosedRing = false;
    if(nodeId == firstNodeId)
    {
        const uint8_t* pLast = nodes.end();
        skipVarintsBackwardUnsafe(pLast, 1);
        nodes = ByteSpan(nodes.data(), pLast - nodes.data());
        isClosedRing = true;
        nodeCount--;
        if(nodeCount < 3) [[unlikely]]
        {
            if(Console::verbosity() >= Console::Verbosity::VERBOSE)
            {
                Console::msg("Rejected way/%lld (invalid closed ring)", id);
            }
            tempWriter_.clear();
            return;
        }
    }
    int taggedNodeCount = (nodeCount << 1) | isClosedRing;

    int wayPile = prevNodePile;
    if (wayPile)        // TODO
    {
        pileWriter_.writeWay(wayPile, id, ParentTileLocator(), nodes, taggedNodeCount, tempWriter_);
        indexFeature(id, wayPile << 2);
        //printf("way/%lld sorted into pile #%d\n", id, wayPile);
    }
    else
    {
        Console::msg("Can't sort way/%lld: All nodes are missing", id);
    }
    tempWriter_.clear();
    //stats_.wayCount++;
}

/// Checks if the first and last node in children_
/// has the same ID. If so, removes the last node from
/// children_, adjusts nodes to omit the last node ID as
/// well, and returns true.
///
/*
bool SorterWorker::checkClosedRing(ByteSpan* nodes)
{
    assert(children_.size() >= 2);
    if(children_[0].id == children_.back().id)
    {
        children_.pop_back();
        const uint8_t* p = nodes->end();
        skipVarintsBackwardUnsafe(p, 1);
        *nodes = ByteSpan(nodes->data(), p-nodes->data());
        return true;
    }
    return false;
}
*/

void SorterWorker::multiTileWay(int64_t id, ByteSpan nodes)
{
    assert(children_.empty());
    assert(childPiles_.empty());
    const TileCatalog& tc = builder_->tileCatalog();
    int64_t nodeId = 0;
    int prevNodePile = 0;
    Tile nodeTile;
    TilePair tilePair;
    int highestNodeZoom = 0;

    const uint8_t* p = nodes.data();
    while (p < nodes.end())
    {
        nodeId += readSignedVarint64(p);
        int nodePile = indexes_[0].get(nodeId);
        assert(nodePile >= 0 && nodePile <= pileCount_);  // pile numbers are 1-based
        if (nodePile == 0)  [[unlikely]]
        {
            Console::msg("node/%lld not found in node index", nodeId);
            continue;
        }
        if (nodePile != prevNodePile)
        {
            nodeTile = tc.tileOfPile(nodePile);
            tilePair += nodeTile;
            highestNodeZoom = std::max(highestNodeZoom, nodeTile.zoom());
            prevNodePile = nodePile;
        }
        children_.emplace_back(nodeId, nodePile, nodeTile);
    }

    if(children_.size() < 2)  [[unlikely]]
    {
        Console::msg("Rejected way/%lld with %lld nodes", id, children_.size());
        children_.clear();
        tempWriter_.clear();
        return;
    }

    bool isClosedRing = false;
    if(children_[0].id == children_.back().id)
    {
        children_.pop_back();
        const uint8_t* pLast = nodes.end();
        skipVarintsBackwardUnsafe(pLast, 1);
        nodes = ByteSpan(nodes.data(), pLast - nodes.data());
        isClosedRing = true;
        if(children_.size() < 3)  [[unlikely]]
        {
            Console::msg("Rejected way/%lld (invalid closed ring)", id);
            children_.clear();
            tempWriter_.clear();
            return;
        }
    }

    int taggedNodeCount = static_cast<int>(children_.size() << 1) | isClosedRing;

    tilePair = tc.normalizedTilePair(tilePair);

    int pilePair = tc.pilePairOfTilePair(tilePair);
    assert(pilePair != 0);
    int firstPile = pilePair >> 2;
    pileWriter_.writeWay(firstPile, id, 
        ParentTileLocator::fromTileToPair(tilePair.first(), tilePair),
        nodes, taggedNodeCount, tempWriter_);
    if (tilePair.hasSecond())
    {
        int secondPile = tc.pileOfTile(tilePair.second());
        assert(secondPile != 0);
        assert(tilePair.first() != tilePair.second());
        assert(firstPile != secondPile);
        pileWriter_.writeWay(secondPile, id, 
            ParentTileLocator::fromTileToPair(tilePair.second(), tilePair),
            nodes, taggedNodeCount, tempWriter_);
    }

    tempWriter_.clear();        
        // we're done with tags (we don't write them for ghost ways)

    if (highestNodeZoom > tilePair.zoom())
    {
        // need to write "ghost" ways since there are nodes in tiles
        // where this way does not live

        for (auto& child : children_)
        {
            if (child.tilePair.zoom() > tilePair.zoom())
            {
                int ghostPile = child.pile;
                if (childPiles_.insert(ghostPile).second)
                {
                    int ghostNodeCount = 0;
                    uint64_t prevGhostChildId = 0;
                    for (auto& ghostChild : children_)
                    {
                        if (ghostChild.pile == ghostPile)
                        {
                            tempWriter_.writeSignedVarint(ghostChild.id - prevGhostChildId);
                            prevGhostChildId = ghostChild.id;
                            ghostNodeCount++;
                        }
                    }
                    pileWriter_.writeWay(ghostPile, id, 
                        ParentTileLocator::fromTileToPair(child.tilePair, tilePair),
                        ByteSpan(nullptr, nullptr),
                        ghostNodeCount << 1, tempWriter_);
                    tempWriter_.clear();
                }
            }
        }
        childPiles_.clear();
    }
    children_.clear();

    indexFeature(id, pilePair);
}

void SorterWorker::beginRelationGroup()
{
    if (currentPhase_ != Sorter::Phase::RELATIONS)
    {
        advancePhase(Sorter::Phase::RELATIONS);
    }
}

void SorterWorker::relation(int64_t id, ByteSpan keys, ByteSpan values,
    ByteSpan roles, ByteSpan memberIds, ByteSpan memberTypes)
{
    /*
    if (id == 43199)
    {
        Console::msg("relation/%lld", id);
    }
    */

    assert(tempWriter_.isEmpty());
    assert(children_.empty());
    assert(childPiles_.empty());
    const TileCatalog& tc = builder_->tileCatalog();

    int64_t memberId = 0;
    int prevMemberPilePair = 0;
    int missingMemberCount = 0;
    int highestMemberZoom = 0;
    bool isSuperRelation = false;
    TilePair tilePair;
    TilePair memberTilePair;
    
    const uint8_t* pMemberId = memberIds.data();
    const uint8_t* pMemberType = memberTypes.data();
    const uint8_t* pRole = roles.data();
    while (pMemberId < memberIds.end())
    {
        memberId += readSignedVarint64(pMemberId);
        int memberType = *pMemberType++;
        uint32_t role = readVarint32(pRole);
        int memberPilePair;

        if (memberType == 2)
        {
            if (memberId == id)
            {
                if(Console::verbosity() >= Console::Verbosity::VERBOSE)
                {
                    Console::msg("relation/%lld: Removed self-reference", id);
                }
                continue;
            }
            isSuperRelation = true;
            memberPilePair = 0;
        }
        else
        {
            memberPilePair = indexes_[memberType].get(memberId);
            memberPilePair <<= (memberType == 0) ? 2 : 0;
            // For nodes (type 0), we store just the pile, so we
            // need to left-shift by 2 bits to turn the pile into a pile pair

            if (memberPilePair == 0)
            {
                missingMemberCount++;
                continue;
                // TODO: Instead of omitting missing members, we should store a 
                //  full copy of the relation in a Purgatory-like store, so we
                //  can update it when the missing feature shows up later
                //  (Need to clarify referential integrity of .osc files)
            }
            else if (memberPilePair != prevMemberPilePair)
            {
                memberTilePair = tc.tilePairOfPilePair(memberPilePair);
                tilePair += memberTilePair;
                highestMemberZoom = std::max(highestMemberZoom, memberTilePair.zoom());
                prevMemberPilePair = memberPilePair;
            }
        }
        uint64_t typedMemberId = (memberId << 2) | memberType;
        tempWriter_.writeVarint(typedMemberId);
        encodeString(role, ProtoStringPair::VALUE);
        children_.emplace_back(typedMemberId, memberPilePair, memberTilePair);
        stats_.memberCount++;
    }
    
    encodeTags(keys, values);

    // TODO: Add geodesk:missing_members, if needed
    // TODO: If *all* members are missing, we can't place the relation

    if (children_.empty())
    {
        // Omit empty relation
        // TODO: differentiate empty rels and rels with all members missing?
        stats_.emptyRelationCount++;
    }
    else if (isSuperRelation)
    {
        deferSuperRelation(id, tilePair, missingMemberCount);
        stats_.superRelationCount++;
    }
    else
    {
        tilePair = tc.normalizedTilePair(tilePair);
        int pilePair = tc.pilePairOfTilePair(tilePair);
        writeRelation(id, pilePair, tilePair,
            children_, highestMemberZoom, tempWriter_,
            missingMemberCount, 0 /* no removed cyclicals */);
        indexFeature(id, pilePair);
    }

    tempWriter_.clear();
    children_.clear();
    stats_.relationCount++;
}


void SorterWorker::writeRelation(uint64_t id, int pilePair, TilePair tilePair,
    Span<SortedChildFeature> members, int highestMemberZoom,
    ByteSpan body, int missingMemberCount, int removedMemberCount)
{
    const TileCatalog& tc = builder_->tileCatalog();
    assert(!tilePair.isNull());
    assert(pilePair != 0);
    assert(highestMemberZoom >= 0 && highestMemberZoom <= 12);

    uint8_t extraTags[128];
    // geodesk::missing_members={n}
    // geodesk::removed_refcycles={n}

    uint8_t* p = extraTags;
    if (missingMemberCount)
    {
        ProtoGol::writeLiteralString(p, "geodesk:missing_members");
        ProtoGol::writeLiteralInt(p, missingMemberCount);
    }
    if (removedMemberCount)
    {
        ProtoGol::writeLiteralString(p, "geodesk:removed_refcycles");
        ProtoGol::writeLiteralInt(p, removedMemberCount);
    }
    uint8_t* extraTagsEnd = p;

    bool hasHigherLevelMembers = highestMemberZoom > tilePair.zoom();

    // If a relation has members that live at higher zoom levels, it must
    // always be exported, even if it is not referenced by another (foreign)
    // relation; we set parent_zoom_delta of the ParentTileLocator to 1
    // to signal this to the Validator
    // This assumes parent_zoom_delta is in lower bits of ParentTileLocator

    auto locator = ParentTileLocator::fromTileToPair(tilePair.first(), tilePair) |
        hasHigherLevelMembers;

    int firstPile = pilePair >> 2;
    assert(firstPile != 0);
    pileWriter_.writeRelation(firstPile, id, locator,
        members.size(), body, ByteSpan(extraTags, extraTagsEnd));
    if (tilePair.hasSecond())
    {
        int secondPile = tc.pileOfTile(tilePair.second());
        assert(secondPile != 0);
        assert(tc.tileOfPile(secondPile) == tilePair.second());
        assert(tilePair.first() != tilePair.second());
        assert(tilePair.first().zoom() == tilePair.second().zoom());
        assert(firstPile != secondPile);

        locator = ParentTileLocator::fromTileToPair(tilePair.second(), tilePair) |
            hasHigherLevelMembers;

        pileWriter_.writeRelation(secondPile, id, locator,
            members.size(), body, ByteSpan(extraTags, extraTagsEnd));
    }

    if (hasHigherLevelMembers)
    {
        // need to write memberships since there are members in tiles
        // where this relation does not live

        for (const SortedChildFeature& child : members)
        {
            if (child.tilePair.zoom() > tilePair.zoom())
            {
                int memberPilePair = child.pile;
                int firstMemberPile = memberPilePair >> 2;
                assert(firstMemberPile != 0);
                pileWriter_.writeMembership(firstMemberPile, id,
                    ParentTileLocator::fromTileToPair(child.tilePair.first(), tilePair),
                    child.typedId);
                if (child.tilePair.hasSecond())
                {
                    int secondMemberPile = tc.pileOfTile(child.tilePair.second());
                    assert(secondMemberPile != 0);
                    assert(child.tilePair.first() != child.tilePair.second());
                    assert(firstMemberPile != secondMemberPile);

                    pileWriter_.writeMembership(secondMemberPile, id,
                        ParentTileLocator::fromTileToPair(child.tilePair.second(), tilePair),
                        child.typedId);
                }
                stats_.foreignMemberCount++;
            }
        }
    }
}

/**
 * - tempWriter must contain the body of the relation (typedMemberIds/roles and tags)
 */
void SorterWorker::deferSuperRelation(int64_t id, TilePair tentativeTilePair, int missingMembers)
{
    SuperRelation* rel = superRelationData_.create<SuperRelation>(
        id, tentativeTilePair,
        superRelationData_.allocCopy(Span<SortedChildFeature>(children_)),
        superRelationData_.allocCopy(tempWriter_.span()),
        missingMembers);
    superRelations_.addTail(rel);
}

void SorterWorker::resolveSuperRelations()
{
    SuperRelationResolver resolver(
        stats_.superRelationCount,
        builder_->tileCatalog(), builder_->stringCatalog(), indexes_[2]);

    for (SorterWorker& worker : reader()->workContexts())
    {
        SuperRelation* rel = worker.superRelations().first();
        while (rel)
        {
            SuperRelation* next = rel->next();  // addTail() will set next to null
            resolver.add(rel);
            rel = next;
        }
    }
    const std::vector<SuperRelation*>* levels = resolver.resolve();

    // Write the relations level by level (create a batch for each)
    // In each level, relations are sorted by ID
    for (int i = 0; i <= SuperRelationResolver::MAX_RELATION_LEVEL; i++)
    {
        const std::vector<SuperRelation*>& level = levels[i];
        if (!level.empty())
        {
            for (SuperRelation* rel : level)
            {
                int pilePair = rel->pilePair();
                assert(pilePair);
                // Console::msg("  Level %d: relation/%lld", i, rel->id());
                writeRelation(rel->id(), pilePair, rel->tilePair(),
                    rel->members(), rel->highestMemberZoom(),
                    rel->body(), rel->missingMemberCount(), rel->removedRefcyleCount());

                // Don't call indexFeature(), which only works for the
                // regular phases; instead, write directly to the relation index 
                indexes_[2].put(rel->id(), pilePair);
            }
            flushPiles();
            // don't call flushIndex() -- only suitable for regular phases
            indexes_[2].endBatch();
        }
    }
}


void SorterWorker::endBlock()	// CRTP override
{
    // At the end of each OSM block, we flush the index to ensure that
    // index writes don't overlap in a non-atomic way
    // However, we don't need to flush the piles, we can allow them to
    // accumulate
    GOL_DEBUG << "Finished block";
    flushIndex();
    stringTranslationTable_.clear();
}

void SorterWorker::harvestResults() const
{
    reader()->addCounts(stats_);
}


Sorter::Sorter(GolBuilder* builder) :
    OsmPbfReader(builder->threadCount()),
    builder_(builder),
    workPerByte_(0)
{
    for (int& phaseCountdown : phaseCountdowns_)
    {
        phaseCountdown = builder->threadCount();
    }
}


void Sorter::processTask(SorterOutputTask& task)
{
    task.piles_.writeTo(builder_->featurePiles());
    /*
    IndexFile& index = builder_->featureIndex(task.currentPhase_);
    printf("Writing %llu features into the index...\n", task.features_.size());
    for (const FeatureIndexEntry entry : task.features_)
    {
        index.put(entry.id(), entry.pile());
    }
    */
    builder_->progress(task.bytesProcessed_ * workPerByte_);
    reportOutputQueueSpace();
    // printf("-> Written\n");
}

static const char* PHASE_TASK_NAMES[] =
{
    "Sorting nodes...",
    "Sorting ways...",
    "Sorting relations...",
    "Sorting super-relations..."
};

void Sorter::advancePhase(int currentPhase, int newPhase)
{
    GOL_DEBUG << "Advancing phase from " << currentPhase << " to " << newPhase;
    assert(newPhase > currentPhase);
    assert(newPhase <= 3);
    std::unique_lock<std::mutex> lock(phaseMutex_);
    for (int i = currentPhase; i < newPhase; i++)
    {
        assert(phaseCountdowns_[i] > 0);
        phaseCountdowns_[i]--;
        GOL_DEBUG << "Completed phase " << i << ", countdown is now " << phaseCountdowns_[i];
        if (phaseCountdowns_[i] == 0)
        {
            builder_->console().setTask(PHASE_TASK_NAMES[newPhase]);
            phaseStarted_.notify_all();
        }
    }
    while (phaseCountdowns_[newPhase - 1] > 0)
    {
        //Console::debug("Waiting to proceed to phase %d...", newPhase);
        phaseStarted_.wait(lock);
    }
}

void Sorter::startFile(uint64_t size)		// CRTP override
{
    workContexts()[0].setMainWorker();
    workPerByte_ = builder_->phaseWork(GolBuilder::Phase::SORT) / size;
    builder_->console().setTask(PHASE_TASK_NAMES[Phase::NODES]);
}


void Sorter::sort(const char* fileName)
{
    GOL_DEBUG << "Starting sort with " << threadCount() << " workers...";
    read(fileName);
}
