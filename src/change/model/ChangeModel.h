// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <vector>
#include <clarisma/alloc/Arena.h>
#include <clarisma/data/HashMap.h>
#include <clarisma/data/LinkedStack.h>
#include <clarisma/util/ShortVarString.h>
#include <geodesk/feature/TypedFeatureId.h>
#include <geodesk/feature/WayPtr.h>
#include <geodesk/feature/RelationPtr.h>

#include "tag/AreaClassifier.h"
#include "ChangedFeature2D.h"
#include "ChangedNode.h"
#include "CRelationTable.h"
#include "CTagTable.h"
#include "change/UpdateSettings.h"

using namespace clarisma;

namespace geodesk
{
    class FeatureStore;
}
using namespace geodesk;

class ChangeModel
{
public:
    explicit ChangeModel(FeatureStore* store, UpdateSettings& settings);

    void clear();

    FeatureStore* store() const { return store_; }
    Arena& arena() { return arena_; }
    uint32_t getLocalString(std::string_view s);
    const CTagTable* getTagTable(const TagTableModel& tags, bool determineIfArea);
    const CTagTable* getTagTable(CRef ref);
    void setLocalTag(ChangedFeatureBase* feature,
        std::string_view key, TagValueType type, uint32_t value);
    const CRelationTable* getRelationTable(CRef ref,
        const MembershipChange* changes = nullptr);
    const CRelationTableSet& relationTables() const { return relationTables_; }

    /// Retrieves the relation table of the given feature,
    /// loading it if needed and processing any membership changes
    ///
    const CRelationTable* getParentRelations(ChangedFeatureBase* feature);

    CFeatureStub* getFeatureStub(TypedFeatureId typedId);
    ChangedNode* getChangedNode(uint64_t id)
    {
        TypedFeatureId typedId = TypedFeatureId::ofNode(id);
        auto it = features_.find(typedId);
        return getChangedNode(id,
            it == features_.end() ? nullptr : it->second);
    }
    ChangedNode* getChangedNode(CFeatureStub* feature)
    {
        assert(feature);
        return getChangedNode(feature->id(), feature);
    }

    ChangedFeature2D* getChangedFeature2D(TypedFeatureId typedId)
    {
        if (typedId == TypedFeatureId::ofWay(1154460013))
        {
            LOGS << "getChangedFeature2D !!!";
        }

        auto it = features_.find(typedId);
        return getChangedFeature2D(typedId,
            it == features_.end() ? nullptr : it->second);
    }

    ChangedFeature2D* getChangedFeature2D(FeatureType type, uint64_t id)
    {
        return getChangedFeature2D(TypedFeatureId::ofTypeAndId(type, id));
    }

    ChangedFeature2D* getChangedFeature2D(CFeatureStub* feature)
    {
        if (feature->typedId() == TypedFeatureId::ofWay(1154460013))
        {
            LOGS << "getChangedFeature2D !!!";
        }
        assert(feature);
        return getChangedFeature2D(feature->typedId(), feature);
    }

    ChangedFeatureBase* getChanged(TypedFeatureId typedId);
    ChangedFeatureBase* getChanged(CFeatureStub* feature);

    // ChangedFeatureBase* changeImplicitly(FeaturePtr feature, CRef ref, bool isRefSE);
    void setMembers(ChangedFeature2D* changed, CFeatureStub** members,
        int memberCount, CFeature::Role* roles);

    /// Applies the given flags to each parent relation
    /// - Flags must be one or more of GEOMETRY_CHANGED,
    ///   BOUNDS_CHANGED or MEMBERS_CHANGED
    /// - BOUNDS_CHANGED is only applied to the parent relation
    ///   if the change in the member bounds could actually
    ///   affect the relation's bounds (the true bounds of
    ///   the relation will be computed in a later step,
    ///   which may clear the relation's BOUNDS_CHANGED flag)
    /// - GEOMETRY_CHANGED (and possibly BOUNDS_CHANGED) are
    ///   recursively cascaded upwards to any of the relation's
    ///   parents, if the relation's flags have changed
    /// - This method is immune to cyclical relations
    ///
    void memberChanged(const CRelationTable* parents,
        const Box& pastMemberBounds, const Box& futureMemberBounds,
        ChangeFlags cascadeFlags);

    void memberChanged(ChangedFeatureBase* member,
        const Box& pastMemberBounds, const Box& futureMemberBounds,
        ChangeFlags cascadeFlags)
    {
        const CRelationTable* parents = getParentRelations(member);
        if (parents)    [[unlikely]]
        {
            memberChanged(parents, pastMemberBounds,
                futureMemberBounds, cascadeFlags);
        }
    }

    void addMembership(ChangedFeatureBase* member, ChangedFeature2D* rel);

    std::span<CFeatureStub*> loadWayNodes(Tip tip, DataPtr pTile, WayPtr way);

    void ensureNodesLoaded(ChangedFeature2D* way)
    {
        if (way->memberCount() == 0)   [[unlikely]]
        {
            CRef ref = way->ref();
            if (!ref.canGetFeature())   [[unlikely]]
            {
                ref = way->refSE();
            }
            Tip tip = ref.tip();
            assert(!tip.isNull());
            TilePtr pTile = store()->fetchTile(tip);
            WayPtr pastWay(ref.getFeature(pTile));
            way->setMembers(loadWayNodes(tip, pTile, pastWay));
        }
    }

    // TODO: can member count be negative?
    //  what about empty relations?
    void ensureMembersLoaded(ChangedFeature2D* rel)
    {
        assert(rel->type() == FeatureType::RELATION);
        if (rel->memberCount() > 0) [[likely]]
        {
            return;     // already loaded
        }
        loadMembers(rel);
    }

    void ensureBounds(ChangedFeature2D* feature) const;

    ChangedFeatureStub* copy(ChangedFeatureBase* feature)
    {
        /*
        if (feature->type() == FeatureType::NODE)
        {
            LOGS << "Creating a copy of " << feature->typedId()
                << ": NW ref = " << feature->ref();
        }
        */
        return arena_.create<ChangedFeatureStub>(feature);
    }

    /*
    ChangedNode* createChangedNode(uint64_t id, ChangeFlags flags,
        uint32_t version, Coordinate xy);
    ChangedFeature2D* createChangedFeature2D(FeatureType type, uint64_t id,
        ChangeFlags flags, uint32_t version, int memberCount);
    */
    CFeature::Role getRole(std::string_view s);
    std::string_view getRoleString(CFeature::Role role) const;
    CFeature* peekFeature(TypedFeatureId typedId) const;

    const ShortVarString* getString(uint32_t code) const
    {
        assert(code < strings_.size());
        return strings_[code];
    }

    std::string_view getStringView(uint32_t code) const
    {
        return getString(code)->toStringView();
    }

    ChangedNode* nodeAtFutureLocation(Coordinate xy) const
    {
        auto iter = futureNodeLocations_.find(xy);
        if(iter == futureNodeLocations_.end()) return nullptr;
        return iter->second;
    }

    const HashMap<TypedFeatureId,CFeatureStub*>& features() const
    {
        return features_;
    }

    size_t stringCount() const { return strings_.size(); }
    size_t tagTableCount() const { return tagTables_.size(); }

    LinkedStack<ChangedNode>& changedNodes() { return changedNodes_; };
    LinkedStack<ChangedFeature2D>& changedWays() { return changedWays_; };
    LinkedStack<ChangedFeature2D>& changedRelations() { return changedRelations_; };

    void prepareNodes();
    void prepareWays();
    // void addNewRelationMemberships();
    // void cascadeMemberChange(NodePtr past, ChangedNode* future);
    // void cascadeMemberChange(FeaturePtr past, ChangedFeature2D* future);
    void mayGainTex(CFeature* f);
    void mayLoseTex(CFeature* f)
    {
        assert(!f->getFeature(store_).isNull());
            // If past feature cannot be retrieved, there's no TEX to lose
            // and this feature should not be added to mayLoseTex_
            // If removed from a tile, it will have already been explicitly marked)
            // TODO: No! May lose TEX in one tile, but keep in another
            //  But TEX assignment can check if no longer in tile
            //  just need to flag the tile that it has TEX changes

        mayLoseTex_.insert(f);
    }
    const HashSet<CFeatureStub*>& mayLoseTex() const { return mayLoseTex_; }
    void dump();
    void checkMissing();
    void determineTexLosers();
    bool willMemberKeepTex(CFeature* member) const;

    const CTagTable* createExceptionNodeTags(bool duplicate, bool orphan);

    void dumpChangedRelationCount()
    {
        size_t relCount = 0;
        ChangedFeature2D* rel = changedRelations().first();
        while (rel)
        {
            relCount++;
            rel = rel->next();
        }
        LOGS << relCount << " changed relations in model";
    }

    // static bool willBeRelationMember(FeaturePtr past, ChangedFeatureBase* future);

private:
    ChangedNode* getChangedNode(uint64_t id, CFeatureStub* existing);
    ChangedFeature2D* getChangedFeature2D(TypedFeatureId typedId, CFeatureStub* existing);
    uint32_t getTagValue(const TagTableModel::Tag& tag);
    void gatherTag(bool isLocalKey, const CTagTable::Tag tag);
    template<typename Iter>
    CFeature* readFeature(Iter& iter, Tip tip, TilePtr pTile);
    void loadMembers(ChangedFeature2D* rel);

    // void readParentRelations(FeaturePtr feature, Tip tip);
    static bool parentBoundsMayChange(const Box& parent,
        const Box& pastMember, const Box& futureMember) noexcept;


    FeatureStore* store_;
    Arena arena_;
    std::vector<ShortVarString*> strings_;
    HashMap<std::string_view,uint32_t> stringToNumber_;
    CTagTableSet tagTables_;
    CRelationTableSet relationTables_;
    HashMap<TypedFeatureId,CFeatureStub*> features_;
    HashMap<Coordinate,ChangedNode*> futureNodeLocations_;
    LinkedStack<ChangedNode> changedNodes_;
    LinkedStack<ChangedFeature2D> changedWays_;
    LinkedStack<ChangedFeature2D> changedRelations_;
    HashSet<CFeatureStub*> mayLoseTex_;
        // TODO: make vector, use MAY_LOSE_TEX flag
        // TODO: move to ChangeManager
    TagTableModel tags_;
    AreaClassifier areaClassifier_;
    std::vector<CFeatureStub*> tempRelations_;
    std::vector<std::pair<CFeatureStub*,CFeature::Role>> tempMembers_;
};
