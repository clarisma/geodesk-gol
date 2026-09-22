// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CRelationTable.h"

#include <clarisma/util/Hash.h>

#include "CFeature.h"

CRelationTable::CRelationTable(std::span<CFeatureStub*> rels) :
    count_(static_cast<uint32_t>(rels.size()))
{
    size_t hash = 0;
    for(int i=0; i<rels.size(); i++)
    {
        relations_[i] = rels[i];
        hash = clarisma::Hash::combine(hash,
            reinterpret_cast<size_t>(rels[i]));
    }
    hash_ = static_cast<uint32_t>(hash ^ (hash >> 32));
}

// TODO: Don't use this, we cannot modify a reltable once its
//  been indexed (sharing!)
/*
bool CRelationTable::remove(uint64_t relId) noexcept
{
    for(int i=0; i<count_; i++)
    {
        if(relations_[i]->id() == relId)
        {
            --count_;
            if(i < count_)
            {
                std::memmove(&relations_[i], &relations_[i+1],
                    (count_ - i)*sizeof(relations_[0]));
            }
            return true;
        }
    }
    return false;
}
*/