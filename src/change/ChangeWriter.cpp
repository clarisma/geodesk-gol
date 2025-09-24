// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeWriter.h"

#include <build/util/TileCatalog.h>
#include <clarisma/cli/Console.h>
#include <clarisma/util/log.h>

#include "change/model/ChangeModel.h"
#include "change/model/ChangedNode.h"
#include "change/model/ChangedTile.h"
#include "tile/tes/TesFlags.h"


void ChangeWriter::write(ChangedTile* tile, Buffer* buf)
{
    tile_ = tile;
    Tip tip = tile->tip();
    assert(!tip.isNull());
    // The starting coordinate is the minX/minY of the tile's bounds
    tileBottomLeft_ = tileCatalog_.tileOfTip(tip).bounds().bottomLeft();
    prevXY_ = tileBottomLeft_;

    out_.setBuffer(buf);
    gatherFeatures();
    int nodeCount = static_cast<int>(featureLists_[0].size());
    int wayCount = static_cast<int>(featureLists_[1].size());
    prepareFeatures(featureLists_[0], 0);
    prepareFeatures(featureLists_[1], nodeCount);
    prepareFeatures(featureLists_[2], nodeCount + wayCount);

    LOGS << tile->tip() << ": "
        << featureLists_[0].size() << " nodes, "
        << featureLists_[1].size() << " ways, "
        << featureLists_[2].size() << " relations, "
        << strings_.size() << " strings, "
        << tagTables_.size() << " tagtables, "
        << relationTables_.size() << " reltables, "
        << removedFeatures_.size() << " removed";

    // TODO: Write header
    writeFeatureIndex();
    writeStrings();
    writeTagTables();
    writeRelationTables();
    writeFeatures();
    writeRemovedFeatures();
    writeExports();
    out_.flush();

    features_.clear();
    featureLists_[0].clear();
    featureLists_[1].clear();
    featureLists_[2].clear();
    removedFeatures_.clear();
    strings_.clear();
    tagTables_.clear();
    relationTables_.clear();
}

void ChangeWriter::gatherFeatures()
{
    // LOGS << tile_->tip() <<": Gathering features";

    Tip tip = tile_->tip();
    const ChangedNode* node = tile_->changedNodes().first();
    while(node)
    {
        addChangedFeature(node);
        node = node->next();
    }

    LinkedStack ways(std::move(tile_->changedWays()));
    while(!ways.isEmpty())
    {
        ChangedFeature2D* way = ChangedFeature2D::cast(ways.pop()->get());
        if (way->id() == 30910986)
        {
            LOGS << tile_->tip() <<": Adding " << way->typedId() << " as changed feature";
        }
        bool newToTile = addChangedFeature(way);
        // TODO: If new to tile, no need to write empty member table
        if(newToTile || way->is(ChangeFlags::MEMBERS_CHANGED))
        {
            for(const CFeatureStub *wayNodeStub : way->members())
            {
                const CFeature* wayNode = wayNodeStub->get();
                if(wayNode->ref().tip() == tip)
                {
                    if(!features_.contains(wayNode))
                    {
                        featureLists_[0].push_back(wayNode);
                        features_[wayNode] = -1;
                    }
                }
            }
        }
    }

    LinkedStack relations(std::move(tile_->changedRelations()));
    while(!relations.isEmpty())
    {
        ChangedFeature2D* relation = ChangedFeature2D::cast(relations.pop()->get());
        // LOGS << tile_->tip() << ": Gathering " << relation->typedId();
        bool newToTile = addChangedFeature(relation);
        if(newToTile || relation->is(ChangeFlags::MEMBERS_CHANGED))
        {
            for(const CFeatureStub *memberStub : relation->members())
            {
                if(memberStub) [[likely]]
                {
                    const CFeature* member = memberStub->get();
                    if (member->typedId() == TypedFeatureId::ofWay(745980339))
                    {
                        LOGS << relation->typedId() << ": gathered " << member->typedId();
                    }
                    // LOGS << relation->typedId() << ": has " << member->typedId() << ", ref: " << member->ref();
                    if(member->isInTile(tip))
                    {
                        // LOGS << relation->typedId() << ": " << member->typedId() << " is in tile " << tip;
                        if(!features_.contains(member))
                        {
                            featureLists_[static_cast<int>(member->type())].push_back(member);
                            features_[member] = -1;

                            // LOGS << relation->typedId() << ": gathered " << member->typedId();
                        }
                    }
                }
            }
            for(auto role : relation->roles())
            {
                // TODO: Check what happens to roles of omitted features

                if(!role.isGlobal())
                {
                    strings_[role.value()]++;
                }
            }
        }
    }
}

/// Adds the given feature to featureLists_ and features_
/// For relations, checks if it has already been added to avoid
/// duplication. If the feature's tag table has changed (or the
/// feature is new to this tile), adds the tag table.
/// If the feature's relation table has changed (or the
/// feature is new to this tile), adds the relation table, and
/// also adds all local relations.
///
/// @return true if the feature is new to this tile
///
bool ChangeWriter::addChangedFeature(const ChangedFeatureBase* feature)
{
    if (!feature->isInTile(tile_->tip()))
    {
        LOGS << tile_->tip() << ": Attempt to add changed " << feature->typedId()
            << " even though it is not in tile.";
    }
    assert(feature->isInTile(tile_->tip()));

    // For relations, we need to check if the relation isn't already
    // in the inventory, because it may have already been added
    // as part of a member's reltable

    if(feature->type() != FeatureType::RELATION || !features_.contains(feature))
    {
        featureLists_[static_cast<int>(feature->type())].push_back(feature);
        features_[feature] = -1;
    }

    ChangeFlags flags = feature->flags();
    bool newToTile = test(flags, feature->ref().tip() == tile_->tip()
        ? ChangeFlags::NEW_TO_NORTHWEST : ChangeFlags::NEW_TO_SOUTHEAST);

    if(newToTile || test(flags, ChangeFlags::TAGS_CHANGED))
    {
        const CTagTable* tags = feature->tagTable();
        useTagTable(tags); // tags must always be non-null
    }

    // If the feature's parent relations changed, gather its local
    // parent relations (note: reltable change can mean added to or
    // removed from relation, but may also indicate that a parent
    // relation moved to a different tile
    if(newToTile || test(flags, ChangeFlags::RELTABLE_CHANGED))
    {
        const CRelationTable* rels = feature->parentRelations();
        if(rels) useRelationTable(rels);
    }
    return newToTile;
}

void ChangeWriter::useTagTable(const CTagTable* tagTable)
{
    assert(tagTable);
    auto [it, inserted] = tagTables_.try_emplace(tagTable, 0);
    it->second++;
    if(inserted)
    {
        auto tags = tagTable->tags();
        for(int i=0; i<tags.size(); i++)
        {
            auto tag = tags[i];
            if(i < tagTable->localTagCount())
            {
                strings_[tag.key()]++;
            }
            if(tag.type() == TagValueType::LOCAL_STRING)
            {
                strings_[tag.value()]++;
            }
        }
    }
}


void ChangeWriter::useRelationTable(const CRelationTable* relTable)
{
    assert(relTable);
    auto [it, inserted] = relationTables_.try_emplace(relTable, 0);
    it->second++;
    if(inserted)
    {
        for(const CFeatureStub *relStub : relTable->relations())
        {
            const CFeature* rel = relStub->get();
            if(rel->isInTile(tile_->tip()))
            {
                if(!features_.contains(rel))
                {
                    featureLists_[2].push_back(rel);
                    features_[rel] = -1;
                }
            }
        }
    }
}


void ChangeWriter::prepareFeatures(std::vector<const CFeature*>& featureList, int startingNumber)
{
    std::sort(featureList.begin(), featureList.end(),
        [](const CFeature* a, const CFeature* b)
        {
            return a->id() < b->id(); // Compare by id
        });

    for(int i=0; i<featureList.size(); i++)
    {
        features_[featureList[i]] = i + startingNumber;
    }
}


void ChangeWriter::writeStrings()
{
    //Console::log("  Writing %lld strings...", strings_.size());
    assert(sharedItems_.empty());
    for(const auto& entry : strings_)
    {
        sharedItems_.emplace_back(entry.second, entry.first,
            model_.getString(entry.first));
    }
    std::sort(sharedItems_.begin(), sharedItems_.end());
    // TODO: Sort strings alphabetically within groups

    out_.writeVarint(sharedItems_.size());
    for(int i=0; i<sharedItems_.size(); i++)
    {
        auto str = reinterpret_cast<const ShortVarString*>(sharedItems_[i].item);
        out_.writeBytes(str, str->totalSize());
        strings_[sharedItems_[i].code] = i;
    }
    sharedItems_.clear();
}

void ChangeWriter::writeFeatureIndex()
{
    // Console::log("  Writing index with %lld features...", features_.size());
    out_.writeVarint(features_.size());
    writeFeatureIndex(featureLists_[0]);
    if(!featureLists_[1].empty() || !featureLists_[2].empty())
    {
        out_.writeByte(0);
        writeFeatureIndex(featureLists_[1]);
        if(!featureLists_[2].empty())
        {
            out_.writeByte(0);
            writeFeatureIndex(featureLists_[2]);
        }
    }
}


void ChangeWriter::writeFeatureIndex(const std::vector<const CFeature*>& featureList)
{
    uint64_t prevId = 0;
    for(const CFeature* feature : featureList)
    {
        bool changed = feature->isChanged() &&
            ChangedFeatureBase::cast(feature)->hasActualChanges();
            // TODO: Is the check for actual changes needed?
            //  Why would feature be added to the tile if not changed?
        uint64_t id = feature->id();
        /*
        LOGS << "Writing indexed feature: " << feature->typedId()
            << ", ptr = " << reinterpret_cast<uintptr_t>(feature)
            << ", isChanged = " << changed;
        */
        assert(id > prevId);
        out_.writeVarint(((id - prevId) << 1) | changed);
        prevId = id;
    }
}


void ChangeWriter::writeFeatures()
{
    writeFeatures(FeatureType::NODE, &ChangeWriter::writeNode);
    writeFeatures(FeatureType::WAY, &ChangeWriter::writeWay);
    writeFeatures(FeatureType::RELATION, &ChangeWriter::writeRelation);
}

template <typename T>
void ChangeWriter::writeFeatures(FeatureType type, void (ChangeWriter::*write)(T*))
{
    for(const CFeature* feature : featureLists_[static_cast<int>(type)])
    {
        if(feature->isChanged() && ChangedFeatureBase::cast(feature)->hasActualChanges())
        {
            assert(feature->isInTile(tile_->tip()));
            (this->*write)(T::cast(feature));
        }
    }
}


void ChangeWriter::writeNode(const ChangedNode* node)
{
    assert(tileCatalog_.tileOfTip(tile_->tip()).bounds().contains(node->xy()));

    ChangeFlags changeFlags = node->flags();
    int flags = test(changeFlags, ChangeFlags::WILL_HAVE_WAYNODE_FLAG) ?
        TesFlags::NODE_BELONGS_TO_WAY : 0;
    flags |= test(changeFlags, ChangeFlags::NODE_WILL_SHARE_LOCATION) ?
        TesFlags::HAS_SHARED_LOCATION : 0;

    flags = writeStub(node, flags, 0);  // TODO: is_exception_node

    if(flags & TesFlags::GEOMETRY_CHANGED)
    {
        Coordinate xy = node->xy();
        out_.writeSignedVarint(static_cast<int64_t>(xy.x) - prevXY_.x);
        out_.writeSignedVarint(static_cast<int64_t>(xy.y) - prevXY_.y);
        prevXY_ = xy;
    }
}

/* // TODO: simpler test:
   ChangeFlags flags = feature->flags();
    bool newToTile = test(flags, feature->ref().tip() == tile_->tip()
        ? ChangeFlags::NEW_TO_NORTHWEST : ChangeFlags::NEW_TO_SOUTHEAST);

*/

/*
bool ChangeWriter::isNewToThisTile(const ChangedFeature2D* feature) const
{
    Tip tip = tile_->tip();
    ChangeFlags newFlags =
        feature->ref().tip() == tip ?
            ChangeFlags::NEW_TO_NORTHWEST : ChangeFlags::NONE;
    newFlags |=
        feature->refSE().tip() == tip ?
            ChangeFlags::NEW_TO_SOUTHEAST : ChangeFlags::NONE;
    return testAny(feature->flags(), newFlags);
}
*/

void ChangeWriter::writeWay(const ChangedFeature2D* way)
{
    if(way->id() == 1338636317)
    {
        LOGS << "Writing changed way/" << way->id();
    }

    ChangeFlags changeFlags = way->flags();
    int flags = test(changeFlags, ChangeFlags::MEMBERS_CHANGED) ?
        TesFlags::MEMBERS_CHANGED : 0;
    flags |= test(changeFlags, ChangeFlags::WAYNODE_IDS_CHANGED) ?
        (TesFlags::NODE_IDS_CHANGED | TesFlags::GEOMETRY_CHANGED) : 0;
        // If waynode IDs have changed, geometry is always assumed to have changed
        // See https://github.com/clarisma/gol-spec/blob/main/tes.md#changeflags
        // TODO: Does ChangeFlags have this rule as well?
    flags |= test(changeFlags, ChangeFlags::WILL_BE_AREA) ?
        TesFlags::IS_AREA : 0;

    int flagsIfNew = test(changeFlags,
        ChangeFlags::WAY_WILL_HAVE_FEATURE_NODES) ?
            (TesFlags::MEMBERS_CHANGED | TesFlags::NODE_IDS_CHANGED) :
            TesFlags::NODE_IDS_CHANGED;
    flags = writeStub(way, flags, flagsIfNew);

    if(flags & TesFlags::GEOMETRY_CHANGED)
    {
        assert(way->memberCount() >= 2);
        out_.writeVarint(way->memberCount());
        Coordinate prevNodeXY = prevXY_;
        for(const CFeatureStub* nodeStub : way->members())
        {
            const CFeature* node = nodeStub->get();
            if(way->id() == 1338636317)
            {
                LOGS << "  Writing waynode node/" << node->id();
            }
            Coordinate nodeXY = node->xy();
            assert(!nodeXY.isNull());
            out_.writeSignedVarint(nodeXY.x - prevNodeXY.x);
            out_.writeSignedVarint(nodeXY.y - prevNodeXY.y);
            prevNodeXY = nodeXY;
        }

        prevXY_ = way->members()[0]->get()->xy();

        if(flags & TesFlags::NODE_IDS_CHANGED)
        {
            // If WAYNODE_IDS_CHANGED is set, GEOMETRY_CHANGED will also be set

            uint64_t prevNodeId = 0;
            for(const CFeatureStub* nodeStub : way->members())
            {
                uint64_t nodeId = nodeStub->id();
                out_.writeSignedVarint(nodeId - prevNodeId);
                prevNodeId = nodeId;
            }
        }
    }

    if(flags & TesFlags::MEMBERS_CHANGED)
    {
        assert(table_.empty());
        WayNodeTableEncoder encoder
            (tile_->tip(), table_, features_, 0);
            // localBase = 0 (uses nodes only)

        for(CFeatureStub* nodeStub : way->members())
        {
            CFeature* node = nodeStub->get();
            if(!node->ref().tip().isNull())
            {
                encoder.add(node);
            }
        }
        encoder.write(out_);        // clears table_
        // LOGS << way->typedId() << ": wrote " << waynodeCount << " feature nodes";
    }
}

void ChangeWriter::writeRelation(const ChangedFeature2D* relation)
{
    // LOGS << tile_->tip() << ": Writing " << relation->typedId();

    ChangeFlags changeFlags = relation->flags();
    int flags = test(changeFlags, ChangeFlags::MEMBERS_CHANGED) ?
        TesFlags::MEMBERS_CHANGED : 0;
    flags |= test(changeFlags, ChangeFlags::BOUNDS_CHANGED) ?
        TesFlags::BBOX_CHANGED : 0;
    flags |= test(changeFlags, ChangeFlags::WILL_BE_AREA) ?
        TesFlags::IS_AREA : 0;
    flags = writeStub(relation, flags, TesFlags::MEMBERS_CHANGED |
        TesFlags::BBOX_CHANGED);

    if(flags & TesFlags::BBOX_CHANGED)
    {
        assert(!relation->bounds().isEmpty());
        writeBounds(relation->bounds());
    }

    if(flags & TesFlags::MEMBERS_CHANGED)
    {
        writeRelationMembers(relation);
    }
}


void ChangeWriter::writeRelationMembers(const ChangedFeature2D* relation)
{
    assert(table_.empty());

    MemberTableEncoder encoder
        (tile_->tip(), table_, features_, 0);
        // localBase = 0 (uses full range of features)

    auto members = relation->members().data();
    auto roles = relation->roles().data();

    for(int i=0; i<relation->memberCount(); i++)
    {
        if(members[i]) [[likely]]
        {
            CFeature* member = members[i]->get();
            CFeature::Role role = roles[i];
            if (!role.isGlobal())   [[unlikely]]
            {
                role = CFeature::Role(false, strings_[role.value()]);
            }
            encoder.add(member, role, nullptr);  // TODO: next
        }
    }
    encoder.write(out_);        // clears table_
}

/// Writes a [FeatureChange](https://github.com/clarisma/gol-spec/blob/main/tes.md#featurechange)
/// structure to the TES. This method computes the TesFlags TAGS_CHANGED,
/// SHARED_TAGS, RELATIONS_CHANGED and GEOMETRY_CHANGED based on the
/// ChangeFlags of the feature (and whether the feature is new to this tile),
/// then combines them with the given `flags`, as well as `flagsIfNew` if thr
/// feature is new to this tile.
/// The method then writes the flag byte, followed optionally by the feature's
/// tag table and relation table (or refs to them, if they are shared).
///
/// @return the effective TesFlags
///
int ChangeWriter::writeStub(const ChangedFeatureBase* feature, int flags, int flagsIfNew)
{
    if (feature->typedId() == TypedFeatureId::ofWay(1338504162))
    {
        LOGS << "Writing stub of " << feature->typedId();
    }
    ChangeFlags changeFlags = feature->flags();
    bool isNew = test(changeFlags,
        feature->ref().tip() == tile_->tip() ?
            ChangeFlags::NEW_TO_NORTHWEST : ChangeFlags::NEW_TO_SOUTHEAST);
    flags |= test(changeFlags, ChangeFlags::TAGS_CHANGED) ?
        TesFlags::TAGS_CHANGED : 0;
    flags |= test(changeFlags, ChangeFlags::GEOMETRY_CHANGED) ?
        TesFlags::GEOMETRY_CHANGED : 0;
    flags |= test(changeFlags, ChangeFlags::RELTABLE_CHANGED) ?
        TesFlags::RELATIONS_CHANGED : 0;
    int relsFlag = feature->parentRelations() ? TesFlags::RELATIONS_CHANGED : 0;
    flags |= isNew ? (flagsIfNew | relsFlag |
        TesFlags::GEOMETRY_CHANGED | TesFlags::TAGS_CHANGED) : 0;

    int tagTableNumber = -1;
    const CTagTable* tags = feature->tagTable();
    if(flags & TesFlags::TAGS_CHANGED)
    {
        if(!tags)
        {
            LOGS << "Tags not fixed for " << feature->typedId();
        }
        assert(tags);
            // At this point, we must always have a valid tag-table,
            // even if it is empty
        tagTableNumber = tagTables_[tags];
        flags |= (tagTableNumber < 2) ? TesFlags::TAGS_CHANGED :
            (TesFlags::TAGS_CHANGED | TesFlags::SHARED_TAGS);
    }

    out_.writeByte(flags);

    if(flags & TesFlags::TAGS_CHANGED)
    {
        if(tagTableNumber < 2)
        {
            writeTagTable(tags);
        }
        else
        {
            out_.writeVarint(tagTableNumber - 2);
        }
    }

    if (flags & TesFlags::RELATIONS_CHANGED)
    {
        const CRelationTable* rels = feature->parentRelations();
        if(rels == nullptr)
        {
            // Feature no longer belongs to any relations
            out_.writeByte(0);
        }
        else
        {
            int relTableNumber = relationTables_[rels];
            if (relTableNumber < 2)
            {
                writeRelationTable(rels);
            }
            else
            {
                // number of a shared reltable, with marker flag
                out_.writeVarint(((relTableNumber - 2) << 1) | 1);
            }
        }
    }
    return flags;
}

// TODO: Move to AbstractTesWriter
void ChangeWriter::writeBounds(const Box& bounds)
{
    out_.writeSignedVarint(static_cast<int64_t>(bounds.minX()) - prevXY_.x);
    out_.writeSignedVarint(static_cast<int64_t>(bounds.minY()) - prevXY_.y);
    out_.writeVarint(static_cast<uint64_t>(
        static_cast<int64_t>(bounds.maxX()) - bounds.minX()));
    out_.writeVarint(static_cast<uint64_t>(
        static_cast<int64_t>(bounds.maxY()) - bounds.minY()));
    prevXY_ = bounds.bottomLeft();
}

template<typename T>
void ChangeWriter::writeSharedItems(HashMap<T*,int>& items, void (ChangeWriter::*write)(T*))
{
    assert(sharedItems_.empty());
    for(const auto& entry : items)
    {
        if(entry.second > 1)
        {
            sharedItems_.emplace_back(entry.second, 0, entry.first);
        }
    }
    std::sort(sharedItems_.begin(), sharedItems_.end());
    out_.writeVarint(sharedItems_.size());
    for(int i=0; i<sharedItems_.size(); i++)
    {
        T* item = static_cast<T*>(sharedItems_[i].item);
        (this->*write)(item);
        items[item] = i + 2;
    }
    sharedItems_.clear();
}


void ChangeWriter::writeTagTables()
{
    // Console::log("  Writing %lld tag tables...", tagTables_.size());
    writeSharedItems<const CTagTable>(tagTables_, &ChangeWriter::writeTagTable);
}

void ChangeWriter::writeTagTable(const CTagTable* tags)
{
    CTagTable::StorageSize storageSize = tags->calculateStorageSize();
    assert(storageSize.totalSize >= 4);
    assert(storageSize.totalSize >= storageSize.localTagsSize + 4);
        // Even if tagtable only has local keys, it must always have an empty-tag
        // marker, so total table size must be 4 bytes more than the local part
    bool hasLocalTags = storageSize.localTagsSize > 0;
    out_.writeVarint(storageSize.totalSize | hasLocalTags);
    if(hasLocalTags)
    {
        out_.writeVarint(storageSize.localTagsSize >> 1);
        for(CTagTable::Tag tag : tags->localTags())
        {
            TagValueType typeCode = tag.type();
            writeTag((strings_[tag.key()] << 2) | typeCode, tag);
        }
    }
    uint32_t prevGlobalKey = 0;
    for(CTagTable::Tag tag : tags->globalTags())
    {
        TagValueType typeCode = tag.type();
        uint32_t key = tag.key();
        assert(key <= FeatureConstants::MAX_COMMON_KEY);
        assert((prevGlobalKey == 0) ? (key >= prevGlobalKey) : (key > prevGlobalKey));
            // Global keys must be unique and ascending
        writeTag(((key - prevGlobalKey) << 2) | typeCode, tag);
        prevGlobalKey = key;
    }
}

void ChangeWriter::writeTag(uint32_t keyAndFlags, CTagTable::Tag tag)
{
    out_.writeVarint(keyAndFlags);
    uint32_t value = tag.value();
    if((keyAndFlags & 3) == TagValueType::LOCAL_STRING)
    {
        value = strings_[value];
    }
    out_.writeVarint(value);
}

void ChangeWriter::writeRelationTables()
{
    writeSharedItems<const CRelationTable>(relationTables_, &ChangeWriter::writeRelationTable);
}


void ChangeWriter::writeRelationTable(const CRelationTable* relTable)
{
    assert(relTable != nullptr);
    Tip localTip = tile_->tip();

    assert(parentRelations_.empty());
    for(CFeatureStub* relStub : relTable->relations())
    {
        CFeature* rel = relStub->get();
        if (rel->isInTile(localTip))
        {
            parentRelations_.emplace_back(rel, Tip()); // null tip = local
        }
        else
        {
            Tip tip = rel->ref().tip();
            Tip tipSE = rel->refSE().tip();
            if (!tipSE.isNull())      [[unlikely]]
            {
                // If the parent relation is dual-tile
                if (tileCatalog_.tileOfTip(tipSE).bounds().contains(
                    tileBottomLeft_))
                {
                    tip = tipSE;
                }
            }
            parentRelations_.emplace_back(rel, tip);
        }
    }
    std::sort(parentRelations_.begin(), parentRelations_.end());

    // TODO: Ensure RelationTableEncoder respects the choice of TIP for
    //  dual-tile relations
    assert(table_.empty());
    RelationTableEncoder encoder
        (localTip, table_, features_,
            featureLists_[0].size() + featureLists_[1].size());
        // localBase = nodecount + wayCount (uses relations only)

    //LOGS << localTip << ": Encoding " << parentRelations_.size() << " relations:";
    for(ParentRelation rel : parentRelations_)
    {
        //LOGS << localTip << ": Encoding " << rel.relation->typedId() << " in " << rel.tip;
        encoder.add(rel.relation);
    }
    encoder.write(out_);        // clears table_
    parentRelations_.clear();
}

int ChangeWriter::gatherRemovedFeatures(const LinkedStack<ChangedFeatureStub>& removed)
{
    size_t first = removedFeatures_.size();
    ChangedFeatureStub* featureStub = removed.first();
    while(featureStub)
    {
        removedFeatures_.emplace_back(featureStub->get());
        featureStub = featureStub->next();
    }
    std::sort(removedFeatures_.begin() + first, removedFeatures_.end(),
        [](const ChangedFeatureBase* a, const ChangedFeatureBase* b)
        {
            return a->id() < b->id(); // Compare by id
        });
    return static_cast<int>(removedFeatures_.size() - first);
}


void ChangeWriter::writeRemovedFeatures()
{
    int removedNodeCount = gatherRemovedFeatures(tile_->deletedNodes());
    int removedWayCount = gatherRemovedFeatures(tile_->deletedWays());
    int removedRelationCount = gatherRemovedFeatures(tile_->deletedRelations());
    assert(removedFeatures_.size() == removedNodeCount + removedWayCount + removedRelationCount);
    out_.writeVarint(removedFeatures_.size());
    writeRemovedFeatures(0, removedNodeCount);
    if(removedWayCount || removedRelationCount)
    {
        out_.writeByte(0);
        writeRemovedFeatures(removedNodeCount, removedWayCount);
        if(removedRelationCount)
        {
            out_.writeByte(0);
            writeRemovedFeatures(removedNodeCount + removedWayCount,
                removedRelationCount);
        }
    }
}

void ChangeWriter::writeRemovedFeatures(int start, int count)
{
    int end = start + count;
    assert(end <= removedFeatures_.size());
    uint64_t prevId = 0;
    for(int i=start; i<end; i++)
    {
        auto feature = removedFeatures_[i];
        bool deleted = test(feature->flags(), ChangeFlags::DELETED);
        uint64_t id = feature->id();
        assert(id != prevId);
        out_.writeVarint(((id - prevId) << 1) | deleted);
        prevId = id;
    }
}

void ChangeWriter::writeExports()
{
    // TODO
    out_.writeByte(0);
}
