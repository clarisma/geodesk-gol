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
            rels[i]->id());
        // We can't compare pointers, because stubs can
        // be replaced by a changed feature
    }
    hash_ = static_cast<uint32_t>(hash ^ (hash >> 32));
}


bool CRelationTable::operator==(const CRelationTable& other) const noexcept
{
    if(hash_ != other.hash_) return false;
    if(count_ != other.count_) return false;
    for (int i=0; i<count_; i++)
    {
        if(relations_[i]->id() != other.relations_[i]->id()) return false;
    }
    return true;
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