// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "CRef.h"
#include <geodesk/feature/TypedFeatureId.h>
#include <geodesk/geom/Coordinate.h>

class ChangedFeatureBase;
using namespace geodesk;
class CFeature;
class ChangedNode;
class ChangedFeature2D;
class ChangeModel;


// TODO: Could make this more compact by storing the action code
//  in same 64-bit word as typedId (sames 8 bytes per action)

// TODO: Problem: For actions that have a ref, how do we know whether
//  the ref is NW or SE?
//  Encode as a flag of ID?
//  Idea: ChangeAction has a field uint64_t idAndFlags:
//    Bit 0-4: Action code
//    Bit 5:   NW/SE flag
//    Bit 6-7: type
//    Bit 8-63: id


// TODO: Alternative: Could pass FeaturePtr instead of CRef;
//  we can always determine NW vs. SE from via a FeaturePtr
//  however, we lose the TEX, and may have to look it up again later

// TODO: make ref part of base class? most subtypes need it

// TODO: base class should create a changed feature
//  (This is common to all actions), then call apply with the changed?

class ChangeAction
{
public:
    enum
    {
        RELATION_MEMBER_ADDED,
        RELATION_MEMBER_REMOVED,
        NODE_BECOMES_COINCIDENT,
        NODE_REMOVED_FROM_WAY,
        NODE_BECOMES_WAYNODE,
        IMPLICIT_WAY_GEOMETRY_CHANGE
    };

    void apply(ChangeModel& model);
    int action() const { return action_; }
    ChangeAction* next() const { return next_; }
    void setNext(ChangeAction* next) { next_ = next; };
    TypedFeatureId typedId() const
    {
        return TypedFeatureId::ofTypeAndId(static_cast<FeatureType>(type_), id_);
    }
    uint64_t id() const { return id_; }
    CRef ref() const { return ref_; }
    bool isRefSE() const { return isRefSE_; }

protected:
    explicit ChangeAction(int action, FeatureType type, uint64_t id, CRef ref, bool isRefSE) :
        action_(action),
        isRefSE_(isRefSE),
        type_(static_cast<unsigned>(type)),
        id_(id),
        ref_(ref),
        next_(nullptr) {}

    explicit ChangeAction(int action, TypedFeatureId typedId, CRef ref, bool isRefSE) :
        ChangeAction(action, typedId.type(), typedId.id(), ref, isRefSE) {}

    unsigned action_  : 5;
    unsigned isRefSE_ : 1;
    unsigned type_    : 2;
    uint64_t id_      : 56;
    CRef ref_;
    ChangeAction* next_;
};


class MembershipChange : public ChangeAction
{
public:
    class Added;
    class Removed;

    ChangedFeature2D* parentRelation() const { return parentRelation_; }
    MembershipChange* next() const
    {
        return reinterpret_cast<MembershipChange*>(next_);
    }

protected:
    MembershipChange(int action, TypedFeatureId memberId,
        CRef memberRef, bool isMemberRefSE, ChangedFeature2D* parentRelation) :
        ChangeAction(action, memberId, memberRef, isMemberRefSE),
        parentRelation_(parentRelation) {}

    ChangedFeature2D* parentRelation_;
};


class MembershipChange::Added : public MembershipChange
{
public:
    Added(TypedFeatureId memberId, ChangedFeature2D* parentRelation) :
        MembershipChange(RELATION_MEMBER_ADDED, memberId,
            CRef::UNKNOWN, false, parentRelation) {}

    void apply(ChangedFeatureBase* changed);
};


class MembershipChange::Removed : public MembershipChange
{
public:
    Removed(TypedFeatureId memberId, CRef memberRef,
        bool isMemberRefSE, ChangedFeature2D* parentRelation) :
        MembershipChange(RELATION_MEMBER_REMOVED, memberId,
            memberRef, isMemberRefSE, parentRelation) {}

    void apply(ChangedFeatureBase* changed);
};


class ImplicitWayGeometryChange : public ChangeAction
{
public:
    ImplicitWayGeometryChange(uint64_t id, CRef ref, bool isRefSE) :
        ChangeAction(IMPLICIT_WAY_GEOMETRY_CHANGE, FeatureType::WAY, id,
            ref, isRefSE) {}

    void apply(ChangeModel& model, ChangedFeatureBase* changed) const;
};


class NodeBecomesCoincident : public ChangeAction
{
public:
    NodeBecomesCoincident(uint64_t id, Coordinate xy, CRef ref) :
        ChangeAction(NODE_BECOMES_COINCIDENT, FeatureType::NODE, id, ref, false),
        xy_(xy) {}

    void apply(ChangedFeatureBase* changed);

private:
    Coordinate xy_;
};


class NodeRemovedFromWay : public ChangeAction
{
public:
    NodeRemovedFromWay(uint64_t id, Coordinate xy, CRef ref) :
        ChangeAction(NODE_REMOVED_FROM_WAY, FeatureType::NODE,
            id, ref, false),
        xy_(xy) {}

    void apply(ChangedFeatureBase* changed);

private:
    Coordinate xy_;
};


class NodeBecomesWaynode : public ChangeAction
{
public:
    NodeBecomesWaynode(uint64_t id, CRef ref) :
        ChangeAction(NODE_BECOMES_WAYNODE, FeatureType::NODE,
            id, ref, false) {}

    // TODO: We don't need the ref (set by TCA), and ideally
    //  we'd provide a pointer to the CFeatureStub so we save
    //  a hashtable lookup

    void apply(ChangedFeatureBase* changed);

private:
    Coordinate xy_;
};


