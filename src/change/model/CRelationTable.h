// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <span>
#include <geodesk/feature/RelationTablePtr.h>
#include <clarisma/data/HashSet.h>

class CFeatureStub;

// TODO: Fix!!!
//  pointers may change if a referenced relation is later changed,
//  which causes it to have a different pointer; need to either
//  change getChangedFeature2D() or base hash/equal on IDs, not pointers
class CRelationTable
{
public:
    explicit CRelationTable(std::span<CFeatureStub*> rels);

    bool operator==(const CRelationTable& other) const noexcept
    {
        if(hash_ != other.hash_) return false;
        if(count_ != other.count_) return false;
        return memcmp(relations_, other.relations_, count_ * sizeof(CFeatureStub*)) == 0;
    }

    static size_t size(size_t count) noexcept
    {
        return sizeof(CRelationTable) + (count-1) * sizeof(CFeatureStub*);
    }

    std::span<CFeatureStub* const> relations() const noexcept
    {
        return {relations_, count_};
    }

    bool remove(uint64_t relId) noexcept;

    struct PtrHash
    {
        size_t operator()(const CRelationTable* table) const noexcept
        {
            return table->hash_;
        }
    };

    struct PtrEqual
    {
        bool operator()(const CRelationTable* a, const CRelationTable* b) const noexcept
        {
            return *a == *b; // Compare contents
        }
    };


private:
    uint32_t count_;
    uint32_t hash_;
    CFeatureStub* relations_[1];
};

using CRelationTableSet = clarisma::HashSet<const CRelationTable*, CRelationTable::PtrHash, CRelationTable::PtrEqual>;