// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <span>
#include <geodesk/feature/RelationTablePtr.h>
#include <clarisma/data/HashSet.h>

class CFeatureStub;


class CRelationTable
{
public:
    explicit CRelationTable(std::span<CFeatureStub*> rels);

    bool operator==(const CRelationTable& other) const noexcept;

    static size_t size(size_t count) noexcept
    {
        return sizeof(CRelationTable) + (count-1) * sizeof(CFeatureStub*);
    }

    std::span<CFeatureStub* const> relations() const noexcept
    {
        return {relations_, count_};
    }

//    bool remove(uint64_t relId) noexcept;

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