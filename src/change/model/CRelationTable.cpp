// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CRelationTable.h"

#include <clarisma/util/Hash.h>

#include "CFeature.h"

CRelationTable::CRelationTable(std::span<CFeatureStub*> rels) :
    count_(static_cast<uint32_t>(rels.size()))
{
    hash_ = 0;
    for(int i=0; i<rels.size(); i++)
    {
        relations_[i] = rels[i];
        hash_ = clarisma::Hash::combine(hash_,
            reinterpret_cast<size_t>(rels[i]));
    }
}

bool CRelationTable::remove(uint64_t relId) noexcept
{
    for(int i=0; i<count_; i++)
    {
        if(relations_[i]->id() == relId)
        {
            --count_;
            if(i < count_)
            {
                std::memmove(relations_[i], relations_[i+1],
                    (count_ - i)*sizeof(relations_[0]));
            }
            return true;
        }
    }
    return false;
}
