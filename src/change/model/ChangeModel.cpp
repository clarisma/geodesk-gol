// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeModel.h"

#include <clarisma/cli/ConsoleWriter.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/MemberIterator.h>
#include <geodesk/feature/MemberTableIterator.h>
#include <geodesk/feature/ParentRelationIterator.h>
#include <geodesk/feature/WayNodeIterator.h>

#include "build/compile/Role.h"
#include "ChangedNode.h"
#include "ChangedTile.h"
#include "CRelationTable.h"
#include "CTagTable.h"

ChangeModel::ChangeModel(FeatureStore* store, UpdateSettings& settings) :
    store_(store),
    arena_(1 * 1024 * 1024), // Arena::GrowthPolicy::SAME_SIZE)
    areaClassifier_(settings.areaRules(),
    [store](std::string_view str)
    {
        return store->strings().getCode(str);
    })
{
    // assert(_CrtCheckMemory());
}


uint32_t ChangeModel::getLocalString(std::string_view s)
{
    // TODO: This is bad, the string_view refers to text in the parsed xml,
    //  which will be thrown away as soon as the file is read
    /*
    auto res = stringToNumber_.insert({s, static_cast<uint32_t>(strings_.size())});
    if(res.second)
    {
        uint32_t totalSize = ShortVarString::totalSize(s.size());
        ShortVarString* str = reinterpret_cast<ShortVarString*>(arena_.alloc(totalSize, 1));
        str->init(s.data(), s.size());
        strings_.push_back(str);
    }
    return res.first->second;
    */

    // TODO: Make more efficient, avoid hashing twice when inserting

    auto it = stringToNumber_.find(s);
    if(it != stringToNumber_.end()) return it->second;
    uint32_t number = static_cast<uint32_t>(strings_.size());
    uint32_t totalSize = ShortVarString::totalSize(s.size());
    ShortVarString* str = reinterpret_cast<ShortVarString*>(arena_.alloc(totalSize, 1));
    str->init(s.data(), s.size());
    strings_.push_back(str);
    stringToNumber_[str->toStringView()] = number;
    return number;
}


uint32_t ChangeModel::getTagValue(const TagTableModel::Tag& tag)
{
    if(tag.valueType() == TagValueType::LOCAL_STRING)
    {
        return getLocalString(tag.stringValue());
    }
    return tag.value();
}

const CTagTable* ChangeModel::getTagTable(const TagTableModel& tagModel, bool determineIfArea)
{
    CTagTable* tags =
        arena_.createVariableLength<CTagTable>(
            tagModel.tags().size(), tagModel, *this);

    auto [iter, inserted] = tagTables_.insert(tags);
    if(!inserted)
    {
        // This is ok, we will never roll back creation if
        // we've added strings, since this means that the
        // tag-table does not already exist
        arena_.freeLastAlloc(tags);
    }

    tags = *iter;
    if(determineIfArea && !tags->areaTagsClassified())
    {
        int areaFlags = areaClassifier_.isArea(tagModel);
        tags->setAreaFlags(
            ((areaFlags & AreaClassifier::AREA_FOR_WAY) ? CTagTable::WAY_AREA_TAGS : 0) |
            ((areaFlags & AreaClassifier::AREA_FOR_RELATION) ? CTagTable::RELATION_AREA_TAGS : 0));
    }
    return tags;
}

const CTagTable* ChangeModel::getTagTable(CRef ref)
{
    assert(ref.canGetFeature());
    FeaturePtr feature = ref.getFeature(store_);
    assert(!feature.isNull());
    assert(tags_.isEmpty());
    tags_.read(feature.tags());
    const CTagTable* tags = getTagTable(tags_, false);
    tags_.clear();
    return tags;
}


const CRelationTable* ChangeModel::getRelationTable(CRef ref, const MembershipChange* changes)
{
    if (ref.canGetFeature())
    {
        Tip tip = ref.tip();
        DataPtr pTile = store()->fetchTile(tip);
        FeaturePtr feature = ref.getFeature(pTile);
        assert(!feature.isNull());
        if(feature.isRelationMember())
        {
            assert(tempRelations_.empty());
            ParentRelationIterator iter(store_, feature.relationTableFast(),
                store_->borrowAllMatcher(), nullptr);
            for(;;)
            {
                CFeature* rel = readFeature(iter, tip, pTile);
                if(!rel) break;
                tempRelations_.emplace_back(rel);
            }
        }
    }

    while(changes)
    {
        /*
        if (changes->typedId().isRelation())
        {
            LOGS << changes->typedId() <<
                (changes->action() == ChangeAction::RELATION_MEMBER_ADDED ?
                    " ADDED to " : " REMOVED from ") << changes->parentRelation()->typedId();
        }
        */
        if(changes->action() == ChangeAction::RELATION_MEMBER_ADDED)    [[likely]]
        {
            tempRelations_.emplace_back(changes->parentRelation());
        }
        else if(changes->action() == ChangeAction::RELATION_MEMBER_REMOVED)
        {
            if (changes->parentRelation()->typedId() == TypedFeatureId::ofRelation(169101) &&
                changes->typedId().isRelation())
            {
                LOGS << "  Removing " << changes->typedId() << " from " << changes->parentRelation()->typedId();
            }
            auto it = std::ranges::find(tempRelations_, changes->parentRelation());
            assert(it != tempRelations_.end());
            std::swap(*it, tempRelations_.back());
            tempRelations_.pop_back();
        }
        else
        {
            assert(false);
        }
        changes = changes->next();
    }

    const CRelationTable* rels = nullptr;
    if(!tempRelations_.empty())
    {
        std::ranges::sort(tempRelations_,
            [](const CFeatureStub* a, const CFeatureStub* b)
            {
                return a->id() < b->id(); // Sort ascending by id
            });
        // TODO: could just sort by pointer, avoids de-ref of data;
        //  we only care about a stable representation
        //  the proper sorting will be performed within ChangeWriter
        //   but will the pointer stay the same if a relation is changed??
        //    No, getChangedFeature2D will update mapping
        //    Safest to stick with ID
        //    But then hash/compare won't work either!!!!

        CRelationTable* newRels =
            arena_.createVariableLength<CRelationTable>(
                tempRelations_.size(), tempRelations_);
        tempRelations_.clear();
        auto [it, inserted] = relationTables_.insert(newRels);
        if(!inserted)
        {
            // This is ok, we will never roll back creation if
            // we've added strings, since this means that the
            // tag-table does not already exist
            arena_.freeLastAlloc(newRels);
        }
        rels = *it;
    }
    return rels;
}


CFeature::Role ChangeModel::getRole(std::string_view s)
{
    int roleCode = store_->strings().getCode(s);
    if(roleCode >= 0 && roleCode <= FeatureConstants::MAX_COMMON_ROLE)
    {
        return CFeature::Role(true, roleCode);
    }
    return CFeature::Role(false, getLocalString(s));
}

std::string_view ChangeModel::getRoleString(CFeature::Role role) const
{
    if(role.isGlobal())
    {
        return store_->strings().getGlobalString(role.value())->toStringView();
    }
    return strings_[role.value()]->toStringView();
}

CFeatureStub* ChangeModel::getFeatureStub(TypedFeatureId typedId)
{
    auto it = features_.find(typedId);
    if(it != features_.end()) return it->second;
    CFeature* f = arena_.create<CFeature>(0, typedId.type(), typedId.id());
    features_.insert({typedId, f});
    return f;
}

CFeature* ChangeModel::peekFeature(TypedFeatureId typedId) const
{
    auto it = features_.find(typedId);
    if(it == features_.end()) return nullptr;
    return it->second->get();
}


// TODO: replace previous change?

/*
ChangedNode* ChangeModel::createChangedNode(
    uint64_t id, ChangeFlags flags, uint32_t version, Coordinate xy)
{
    ChangedNode* changed = arena_.create<ChangedNode>(id, flags, version, xy);
    changedNodes_.push(changed);
    CFeatureStub* current = changed;
    TypedFeatureId typedId = TypedFeatureId::ofNode(id);
    auto it = features_.find(typedId);
    if(it != features_.end())
    {
        // Node exists already
        current = it->second;
        if(!it->second->isBasic())
        {
            ChangedNode* prevChanged;
            if(current->isReplaced())
            {
                // If node has been replaced, this means it has changed
                prevChanged = reinterpret_cast<ChangedNode*>(current->getReplaced());
                assert(prevChanged->isChanged());
                assert(prevChanged->typedId() == typedId);
            }
            else
            {
                assert(current->isChanged());
                prevChanged = reinterpret_cast<ChangedNode*>(current);
            }
            if(prevChanged->version() < version ||
                (prevChanged->version() == version &&
                    (flags & ChangeFlags::DELETED) == ChangeFlags::DELETED))
            {
                changed->addFlags(prevChanged->flags() & ChangeFlags::CREATED);
                current->replaceWith(changed);
            }
        }
    }
    features_[typedId] = current;
    return changed;
}
*/

ChangedNode* ChangeModel::getChangedNode(uint64_t id)
{
    ChangedNode* changed = arena_.create<ChangedNode>(id);
    TypedFeatureId typedId = TypedFeatureId::ofNode(id);
    auto it = features_.find(typedId);
    if(it != features_.end())
    {
        // Node exists already
        CFeatureStub* existing = it->second;
        assert(existing->type() == FeatureType::NODE);
        if(!existing->isBasic())
        {
            // If node has been replaced, this means it has changed
            arena_.freeLastAlloc(changed);
            if(existing->isReplaced()) existing = existing->getReplaced();
            return ChangedNode::cast(existing);
        }
        auto existingNode = CFeature::cast(existing);
        changed->setRef(existingNode->ref());
        changed->setXY(existingNode->xy());
        existing->replaceWith(changed);
            // (copies flags to changed)
        assert(changed->isFutureWaynode() == existingNode->isFutureWaynode());
        assert(changed->isFutureForeign() == existingNode->isFutureForeign());
    }
    features_[typedId] = changed;
    changedNodes_.push(changed);
    return changed;
}

// TODO: unify with above
ChangedNode* ChangeModel::getChangedNode(CFeatureStub* nodeStub)
{
    assert(nodeStub->type() == FeatureType::NODE);
    if(!nodeStub->isBasic())
    {
        // If node has been replaced, this means it has changed
        if(nodeStub->isReplaced()) nodeStub = nodeStub->getReplaced();
        return ChangedNode::cast(nodeStub);
    }
    auto node = CFeature::cast(nodeStub);
    ChangedNode* changed = arena_.create<ChangedNode>(node->id());
    changed->setRef(node->ref());
    changed->setXY(node->xy());
    node->replaceWith(changed);
    return changed;
}


ChangedFeature2D* ChangeModel::getChangedFeature2D(CFeatureStub* stub)
{
    if(!stub->isBasic())
    {
        // If feature has been replaced, this means it has changed
        if(stub->isReplaced()) stub = stub->getReplaced();
        return ChangedFeature2D::cast(stub);
    }
    ChangedFeature2D* changed = arena_.create<ChangedFeature2D>(stub->type(), stub->id());
    auto feature = CFeature::cast(stub);
    changed->setRef(feature->ref());
    changed->setRefSE(feature->refSE());
    feature->replaceWith(changed);
    return changed;
}

// TODO: unify with above
ChangedFeature2D* ChangeModel::getChangedFeature2D(FeatureType type, uint64_t id)
{
    ChangedFeature2D* changed = arena_.create<ChangedFeature2D>(type, id);
    // assert(_CrtCheckMemory());
    TypedFeatureId typedId = TypedFeatureId::ofTypeAndId(type, id);
    auto it = features_.find(typedId);
    if(it != features_.end())
    {
        // Feature exists already
        CFeatureStub* existing = it->second;
        assert(existing->type() == type);
        if(!existing->isBasic())
        {
            // If node has been replaced, this means it has changed
            arena_.freeLastAlloc(changed);
            if(existing->isReplaced()) existing = existing->getReplaced();
            return ChangedFeature2D::cast(existing);
        }
        auto existingFeature = CFeature::cast(existing);
        changed->setRef(existingFeature->ref());
        changed->setRefSE(existingFeature->refSE());
        existing->replaceWith(changed);
            // (copies flags to changed)
        assert(!changed->isFutureWaynode());
        assert(!existingFeature->isFutureWaynode());
        assert(changed->isFutureForeign() == existingFeature->isFutureForeign());
    }
    features_[typedId] = changed;
        // TODO: This messes up CRelationTable, which relies on stable
        //  pointers. Insert new changes only, otherwise keep the stub
        //  This would save a lookup here, at the cost of an extra
        //  indirection whenever the feature is looked up via index
    (type == FeatureType::WAY ? changedWays_ : changedRelations_).push(changed);
    return changed;
}

ChangedFeatureBase* ChangeModel::getChanged(TypedFeatureId typedId)
{
    FeatureType type = typedId.type();
    if(type == FeatureType::NODE) return getChangedNode(typedId.id());
    return getChangedFeature2D(type, typedId.id());
}

// TODO: "offer" refs instead of setting them, because
//  the ChangedFeature may already have a "better" ref
ChangedFeatureBase* ChangeModel::changeImplicitly(
    FeaturePtr feature, CRef ref, bool isRefSE)
{
    if(feature.isNode())
    {
        NodePtr node(feature);
        ChangedNode* changed = getChangedNode(node.id());
        if(!changed->isChangedExplicitly())
        {
            // Only set ref and xy if not changed explicitly
            changed->setXY(node.xy());
            assert(!isRefSE);
            changed->offerRef(ref);
        }
        return changed;
    }
    ChangedFeature2D* changed = getChangedFeature2D(feature.type(), feature.id());
    if(!changed->isChangedExplicitly())
    {
        // Only set ref and bounds if not changed explicitly
        changed->setBounds(feature.bounds());
        assert(!isRefSE);
        if(isRefSE) [[unlikely]]
        {
            changed->offerRefSE(ref);
        }
        else
        {
            changed->offerRef(ref);
        }
    }
    return changed;
}


void ChangeModel::setMembers(ChangedFeature2D* changed, CFeatureStub** members,
    int memberCount, CFeature::Role* roles)
{
    CFeatureStub** dest = changed->members().data();
    if(changed->memberCount() < memberCount)
    {
        dest = reinterpret_cast<CFeatureStub**>(arena_.alloc(
            (sizeof(CFeatureStub*) + (roles ? sizeof(CFeature::Role) : 0))
            * memberCount, alignof(CFeatureStub*)));
    }
    std::memcpy(dest, members, memberCount * sizeof(CFeature*));
    if(roles)
    {
        std::memcpy(dest + memberCount, roles, memberCount * sizeof(CFeature::Role));
    }
    changed->setMembers(std::span(dest, memberCount));
}

/*
// TODO: replace previous change?

ChangedFeature2D* ChangeModel::createChangedFeature2D(
    FeatureType type, uint64_t id, ChangeFlags flags,
    uint32_t version, int memberCount)
{
    assert(type == FeatureType::WAY || type == FeatureType::RELATION);
    ChangedFeature2D* changed = reinterpret_cast<ChangedFeature2D*>(
        arena_.alloc(ChangedFeature2D::size(
            type, memberCount), alignof(ChangedFeature2D)));
    new(changed)ChangedFeature2D(type, id, flags, version, memberCount);
    (type == FeatureType::WAY ? changedWays_ : changedRelations_).push(changed);
    CFeatureStub* current = changed;
    TypedFeatureId typedId = TypedFeatureId::ofTypeAndId(type, id);
    auto it = features_.find(typedId);
    if(it != features_.end())
    {
        // Feature exists already
        current = it->second;
        if(!current->isBasic())
        {
            ChangedFeature2D* prevChanged;
            if(current->isReplaced())
            {
                // If feature has been replaced, this means it has changed
                prevChanged = reinterpret_cast<ChangedFeature2D*>(current->getReplaced());
                assert(prevChanged->isChanged());
                assert(prevChanged->typedId() == typedId);
            }
            else
            {
                assert(current->isChanged());
                prevChanged = reinterpret_cast<ChangedFeature2D*>(current);
            }
            if(prevChanged->version() < version ||
                (prevChanged->version() == version &&
                    (flags & ChangeFlags::DELETED) == ChangeFlags::DELETED))
            {
                changed->addFlags(prevChanged->flags() & ChangeFlags::CREATED);
                current->replaceWith(changed);
            }
        }
    }
    features_[typedId] = current;
    return changed;
}
*/

ChangedTile* ChangeModel::getChangedTile(Tip tip)
{
    assert(!tip.isNull());
    auto it = changedTiles_.find(tip);
    if(it != changedTiles_.end()) return it->second;
    ChangedTile* changedTile = arena_.create<ChangedTile>(arena_, tip);
    changedTiles_[tip] = changedTile;
    return changedTile;
}

void ChangeModel::dump()
{
    int64_t counts[3] = {0};
    int64_t changedCounts[3] = {0};
    int64_t tagsChangedCounts[3] = {0};
    int64_t geomChangedCounts[3] = {0};
    int64_t createdCounts[3] = {0};

    Console::log("Completed read.");

    for(const auto& [id, stub] : features_)
    {
        CFeature* f = stub->get();
        FeatureType type = f->type();
        counts[static_cast<int>(type)]++;
        if(f->isChanged())
        {
            changedCounts[static_cast<int>(type)]++;
            if(test(ChangedFeatureBase::cast(f)->flags(), ChangeFlags::TAGS_CHANGED))
            {
                tagsChangedCounts[static_cast<int>(type)]++;
            }
            if(test(ChangedFeatureBase::cast(f)->flags(), ChangeFlags::GEOMETRY_CHANGED))
            {
                geomChangedCounts[static_cast<int>(type)]++;
            }
            /*
            if((ChangedFeatureBase::cast(f)->flags() & ChangeFlags::CREATED)
                == ChangeFlags::CREATED)
            {
                createdCounts[static_cast<int>(type)]++;
            }
            */
        }
    }
    Console::log("Total nodes:      %lld", counts[0]);
    Console::log("  Changed:        %lld", changedCounts[0]);
    Console::log("    Created:      %lld", createdCounts[0]);
    Console::log("    Geom changed: %lld", geomChangedCounts[0]);
    Console::log("    Tags changed: %lld", tagsChangedCounts[0]);
    Console::log("Total ways:       %lld", counts[1]);
    Console::log("  Changed:        %lld", changedCounts[1]);
    Console::log("    Created:      %lld", createdCounts[1]);
    Console::log("    Geom changed: %lld", geomChangedCounts[1]);
    Console::log("    Tags changed: %lld", tagsChangedCounts[1]);
    Console::log("Total relations:  %lld", counts[2]);
    Console::log("  Changed:        %lld", changedCounts[2]);
    Console::log("    Created:      %lld", createdCounts[2]);
    Console::log("    Geom changed: %lld", geomChangedCounts[2]);
    Console::log("    Tags changed: %lld", tagsChangedCounts[2]);
}


void ChangeModel::checkMissing()
{
    size_t missingCount = 0;
    for (const auto it : features_)
    {
        CFeature* f = it.second->get();
        if(!f->ref().tip().isNull()) continue;
        if(f->isChanged())
        {
            if(ChangedFeatureBase::cast(f)->version() == 1) continue;
        }
        missingCount++;
        ConsoleWriter out;
        // out.timestamp() << "Missing: " << it.first;
    }
    Console::log("%lld features missing.", missingCount);
}


void ChangeModel::prepareNodes()
{
    // TODO: remove superseded versions here?

    ChangedNode* node = changedNodes_.first();
    while(node)
    {
        assert(node->version() > 0);
        if(!node->isDeleted())
        {
            Coordinate xy = node->xy();
            auto it = futureNodeLocations_.find(xy);
            if(it != futureNodeLocations_.end())
            {
                it->second->addFlags(ChangeFlags::NODE_WILL_SHARE_LOCATION);
                node->addFlags(ChangeFlags::NODE_WILL_SHARE_LOCATION);
            }
            else
            {
                futureNodeLocations_[xy] = node;
            }
        }
        node = node->next();
    }
}


void ChangeModel::prepareWays()
{
    ChangedFeature2D* way = changedWays_.first();
    while(way)
    {
        assert(way->version() > 0);
        if(way->isDeleted())
        {
            assert(way->memberCount() == 0);
        }
        for(CFeatureStub* stub: way->members())
        {
            CFeature* node = stub->get();
            assert(node->type() == FeatureType::NODE);
            node->markAsFutureWaynode();
        }
        way = way->next();
    }
}

/*
bool ChangeModel::willBeRelationMember(FeaturePtr past, ChangedFeatureBase* future)
{
    ChangeFlags changeFlags = future->flags();
    if(test(changeFlags, ChangeFlags::ADDED_TO_RELATION)) return true;
    if(past.isNull() || !past.isRelationMember()) return false;
    if(!test(changeFlags, ChangeFlags::REMOVED_FROM_RELATION)) return true;
    int removeCount = 0;
    const ChangeAction* membershipChange = future->membershipChanges();
    while(membershipChange)
    {
        assert(membershipChange->action == ChangeAction::RELATION_MEMBER_DROPPED);
        removeCount++;
        membershipChange = membershipChange->next;
    }
    assert(removeCount > 0);
    MemberTableIterator iter(0, past.relationTableFast());
        // It's ok to use a dummy handle here since we are not going
        // to fetch the parent relations, we're only counting them
        // to see if the feature will be removed from all relations
    while(iter.next())
    {
        if(--removeCount < 0) return true;
    }
    return false;
}
*/

std::span<CFeatureStub*> ChangeModel::loadWayNodes(Tip tip, DataPtr pTile, WayPtr way)
{
    WayNodeIterator iter(store_, way, false, true);
    size_t nodeCount = iter.storedRemaining();
    // LOGS << "Loading nodes of way/" << way.id() << " from tile " << tip << " (" << nodeCount << " nodes)";
    CFeatureStub** nodes = arena_.allocArray<CFeatureStub*>(nodeCount);
    CFeatureStub** pNode = nodes;
    for(;;)
    {
        WayNodeIterator::WayNode wayNode = iter.next();
        if(wayNode.id == 0) break;

        CFeature* node = getFeatureStub(TypedFeatureId::ofNode(wayNode.id))->get();
        if (wayNode.id == 7857097273)
        {
            LOGS << "- Loading node/" << wayNode.id;
            if (node->isChanged())
            {
                LOGS << "    version = " << ChangedNode::cast(node)->version();
                LOGS << "    flags =   " << static_cast<uint32_t>(ChangedNode::cast(node)->flags());
            }
        }
        if(!node->isChanged())
        {
            node->setXY(wayNode.xy);
            if (wayNode.feature.isNull())   [[likely]]
            {
                node->setRef(CRef::ANONYMOUS_NODE);
            }
            else
            {
                if (wayNode.foreign.isNull())   [[likely]]
                {
                    node->offerRef(CRef::ofMaybeExported(tip, wayNode.feature.ptr() - pTile));
                }
                else
                {
                    node->setRef(CRef::ofForeign(wayNode.foreign));
                }
            }
        }
        // LOGS << "- node/" << wayNode.id << ": " << node->ref();
        assert(pNode < nodes + nodeCount);
        *pNode++ = node;
    }
    // ReSharper disable once CppDFALocalValueEscapesFunction
    //  nodes does not escape, the table is allocated in the arena
    return { nodes, nodeCount };
}


template<typename Iter>
CFeature* ChangeModel::readFeature(Iter& iter, Tip tip, DataPtr pTile)
{
    FeaturePtr pastFeature = iter.next();
    if (pastFeature.isNull()) return nullptr;
    CFeatureStub* stub = getFeatureStub(pastFeature.typedId());
    assert(stub);
    CFeature* f = stub->get();

    CRef ref = iter.isForeign() ?
        CRef::ofExported(iter.tip(), iter.tex()) :
        CRef::ofMaybeExported(tip, pastFeature.ptr() - pTile);

    if (f->type() == FeatureType::NODE)
    {
        if(!f->isChanged())
        {
            f->offerRef(ref);
            f->setXY(NodePtr(pastFeature).xy());
        }
    }
    else
    {
        if(!f->isChanged() || !ChangedFeatureBase::cast(f)->is(
            ChangeFlags::PROCESSED))
        {
            // Differs from nodes, because NW and SE tiles may swap
            // position if a dual-tile feature moves to an adjacent tile,
            // so we cannot safely offer

            if (pastFeature.hasNorthwestTwin()) [[unlikely]]
            {
                f->offerRefSE(ref);
            }
            else
            {
                f->offerRef(ref);
            }
        }
    }
    return f;
}

void ChangeModel::addNewRelationMemberships()
{
    HashSet<TypedFeatureId> memberSet;
    ChangedFeature2D* rel = changedRelations().first();
    while (rel)
    {
        if(rel->isChangedExplicitly()) [[likely]]
        {
            if (rel->ref() == CRef::UNKNOWN)
            {
                // If a relation is changed explicitly and
                // it has not been found, this means it has
                // been newly created; we need to add memberships
                // for all its members (for existing relations,
                // TileChangeAnalyzer will perform this step)
                bool hasChildRelations = false;
                for (CFeatureStub* memberStub : rel->members())
                {
                    // TODO: We could avoid the lookup by typedId
                    TypedFeatureId memberId = memberStub->typedId();
                    auto result = memberSet.insert(memberId);
                    if (result.second)  // actually inserted
                    {
                        // Only add a single membership, even if member
                        // appears multiple times in same relation

                        ChangedFeatureBase* member = getChanged(memberId);
                        member->addMembershipChange(
                            arena_.create<MembershipChange::Added>(
                                memberId, rel));
                        member->addFlags(ChangeFlags::ADDED_TO_RELATION |
                            ChangeFlags::RELTABLE_CHANGED);
                    }
                    hasChildRelations |= memberId.isRelation();
                }
                memberSet.clear();
                rel->addFlags(hasChildRelations ?
                    ChangeFlags::WILL_BE_SUPER_RELATION : ChangeFlags::NONE);
                    // TODO: Move super-relation detection to ChangeReader?
                    //  Currently duplicated in TileChangeAnalyzer::checkMembers()
            }
        }
        rel = rel->next();
    }
}


void ChangeModel::cascadeMemberChange(NodePtr past, ChangedNode* future)
{
    Box futureBounds(future->xy());
    cascadeMemberChange(past, future, futureBounds);
}

void ChangeModel::cascadeMemberChange(FeaturePtr past, ChangedFeature2D* future)
{
    cascadeMemberChange(past, future, future->bounds());
}


void ChangeModel::cascadeMemberChange(FeaturePtr past,
    ChangedFeatureBase* future, const Box& futureBounds)
{
    if (past.isNull())
    {
        // If the feature didn't exist in the past, there's nothing to do
        // (If a newly created feature has parent relations, that means it
        // has been added to all of these relations, which means these
        // relations will already be explicitly geometrically changed)
        return;
    }

    if (future->is(ChangeFlags::RELTABLE_LOADED))
    {
        // RELTABLE_LOADED does not necessarily mean that the feature
        // actually has a reltable, just that it has been processed

        const CRelationTable* rels = future->parentRelations();
        if (rels)
        {
            for (CFeatureStub* rel : rels->relations())
            {
                memberBoundsChanged(rel->get(),past, futureBounds);
            }
        }
    }
    else if (past.isRelationMember())
    {
        assert(!future->is(ChangeFlags::TILES_CHANGED));
            // If the member feature moved tiles, its ref will reflect
            // its future location, not its past (which we need in
            // order to retrieve its parent relations)
            // But if the feature moved, it should have already
            // loaded its reltable (because it will need to be
            // written to the new tile), which means we would
            // have executed the code above, instead
        Tip tip = future->ref().tip();
        DataPtr pTile = store()->fetchTile(tip);
        ParentRelationIterator iter(store_, past.relationTableFast(),
            store_->borrowAllMatcher(), nullptr);
        for(;;)
        {
            CFeature* rel = readFeature(iter, tip, pTile);
            if(!rel) break;
            memberBoundsChanged(rel,past, futureBounds);
        }
    }
}


// TODO: We must also cascade if a member's tiles changed (even if
//  the change cannot affect the relation's bounds), so processRelation()
//  can check whether members (or the relation itself) will gain/lose TEXes

void ChangeModel::memberBoundsChanged(CFeature* relation,
    FeaturePtr pastMember, const Box& futureMemberBounds)
{
    if (relation->isChanged())
    {
        ChangedFeature2D* changedRel = ChangedFeature2D::cast(relation);
        if (changedRel->is(ChangeFlags::PROCESSED))
        {
            LOGS << relation->typedId() << " has already been processed. Has geom changes = "
                << changedRel->is(ChangeFlags::GEOMETRY_CHANGED)
                << ", member = " << pastMember.typedId();

            // TODO: This is a problem!
            //  Assert avoided temporarily
            // return;
        }
        assert(!changedRel->is(ChangeFlags::PROCESSED));
        if (changedRel->is(ChangeFlags::GEOMETRY_CHANGED))
        {
            // If relation already has geometric changes,
            // there's nothing to do
            return;
        }
    }
    RelationPtr pastRelation(relation->getFeature(store_));
    assert(!pastRelation.isNull());

    Coordinate pastMemberBottomLeft = pastMember.bottomLeft();
    Coordinate pastMemberTopRight = pastMember.topRight();
    Box pastRelationBounds = pastRelation.bounds();
    if(!pastRelationBounds.containsSimple(futureMemberBounds) ||
        pastMemberBottomLeft.x == pastRelationBounds.minX() ||
        pastMemberBottomLeft.y == pastRelationBounds.minY() ||
        pastMemberTopRight.x == pastRelationBounds.maxX() ||
        pastMemberTopRight.y == pastRelationBounds.maxY())
    {
        // Unless the member's future bounds lie entirely within the
        // parent's past bounds, and the member's past bounds did not
        // lie on the parent's bounds, the member's bounds change may
        // cause the parent's bounds to change as well

        /*
        LOGS << "Bounds of " << relation->typedId()
            << " may change due to bounds change of member";
        */

        ChangedFeature2D* changed = getChangedFeature2D(relation);
        assert(!changed->is(ChangeFlags::PROCESSED));
        changed->setBounds(pastRelationBounds);

        // ensureMembersLoaded() is called by processRelation()
        // ensureMembersLoaded(changed);

        changed->addFlags(ChangeFlags::GEOMETRY_CHANGED);
    }
}


void ChangeModel::ensureMembersLoaded(ChangedFeature2D* rel)
{
    if (rel->memberCount() > 0) return;     // already loaded

    assert(!rel->is(ChangeFlags::PROCESSED));
    assert(tempMembers_.empty());
    CRef ref = rel->ref();
    if (!ref.canGetFeature())   [[unlikely]]
    {
        ref = rel->refSE();
    }
    Tip tip = ref.tip();
    assert(!tip.isNull());
    DataPtr pTile = store()->fetchTile(tip);
    RelationPtr pastRel(ref.getFeature(pTile));
    assert(!pastRel.isNull());

    bool hasChildRelations = false;
    MemberIterator iter(store_, pastRel.bodyptr(),
        FeatureTypes::ALL, store_->borrowAllMatcher(), nullptr);
    for(;;)
    {
        CFeature* member = readFeature(iter, tip, pTile);
        if(!member) break;
        CFeature::Role role;
        if (iter.hasLocalRole())    [[unlikely]]
        {
            role = CFeature::Role(false, getLocalString(iter.currentRole()));
        }
        else
        {
            role = CFeature::Role(true, iter.currentRoleCode());
        }
        hasChildRelations |= member->type() == FeatureType::RELATION;
        tempMembers_.emplace_back(member, role);
    }
    assert(tempMembers_.size() > 0);

    CFeatureStub** pMembers = reinterpret_cast<CFeatureStub**>(arena_.alloc(
        (sizeof(CFeatureStub*) + sizeof(CFeature::Role)) * tempMembers_.size(),
        alignof(CFeatureStub*)));

    CFeatureStub** pMember = pMembers;
    CFeature::Role* pRole = reinterpret_cast<CFeature::Role*>(pMembers + tempMembers_.size());

    for (auto& entry : tempMembers_)
    {
        *pMember++ = entry.first;
        *pRole++ = entry.second;
    }
    rel->setMembers(std::span(pMembers, tempMembers_.size()));
    rel->addFlags(hasChildRelations ?
        ChangeFlags::WILL_BE_SUPER_RELATION : ChangeFlags::NONE);

    tempMembers_.clear();
}

void ChangeModel::mayGainTex(CFeature* feature)
{
    Tip tip = feature->ref().tip();
    assert(!tip.isNull());
    getChangedTile(tip)->mayGainTex(feature);
    if (feature->type() != FeatureType::NODE)
    {
        tip = feature->refSE().tip();
        assert(!tip.isNull() || feature->refSE() == CRef::SINGLE_TILE);
        if (!tip.isNull())  [[unlikely]]
        {
            getChangedTile(tip)->mayGainTex(feature);
        }
    }
}


void ChangeModel::determineTexLosers()
{
    // TODO: Determine which features will actually lose their TEX

    for (CFeatureStub* stub : mayLoseTex_)
    {
        CFeature* feature = stub->get();
        if (feature->isFutureForeign()) continue;
            // If a feature has been marked as foreign by
            // any parent, it will keep its TEX
        FeaturePtr past = feature->getFeature(store_);
        assert (!past.isNull());
            // If the feature's past version cannot be retrieved,
            // that means it moved away from all of its tiles
            // or is new, in which case it should not have
            // been added to mayLoseTex_

        if (willMemberKeepTex(feature)) continue;

        // TODO: should we pass FeaturePtr to this method?
        //  Otherwise may retrieve more than once
    }
}


bool ChangeModel::willMemberKeepTex(CFeature* member) const
{
    if(member->isChanged())
    {
        auto changed = ChangedFeatureBase::cast(member);
        if (changed->is(ChangeFlags::RELTABLE_LOADED))
        {
            const CRelationTable* rels = changed->parentRelations();
            if (!rels) return false;
            for (const CFeatureStub* relStub : rels->relations())
            {
                if (changed->isForeignMemberOf(relStub->get()))
                {
                    return true;
                }
            }
            return false;
        }
    }

    FeaturePtr past = member->getFeature(store_);
    assert(!past.isNull());
    if (!past.isRelationMember()) return false;
    ParentRelationIterator iter(store_, past.relationTableFast());
    for (;;)
    {
        RelationPtr pastParent = iter.next();
        if (pastParent.isNull()) break;
        const CFeature* rel = peekFeature(
            TypedFeatureId::ofRelation(pastParent.id()));
        if (rel)
        {
            if (member->isForeignMemberOf(rel)) return true;
        }

        // TODO: obtain the parent's TIPs based on its bbox
    }
    return false;
}


void ChangeModel::clear()
{
    arena_.clear();
    strings_.clear();
    stringToNumber_.clear();
    tagTables_.clear();
    relationTables_.clear();
    features_.clear();
    futureNodeLocations_.clear();
    changedNodes_.clear();
    changedWays_.clear();
    changedRelations_.clear();
    changedTiles_.clear();
    mayLoseTex_.clear();

    // The following must always be cleared after each use:
    assert(tags_.isEmpty());
    assert(tempRelations_.empty());
    assert(tempMembers_.empty());
}