// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/ForeignFeatureRef.h>
#include "tile/model/TRelation.h"


class Membership : public Linked<Membership>
{
public:
	Membership(TRelation* rel) :
 		taggedId_(rel->id() << 1),
		localRelation_(rel)
	{
	}

	Membership(uint64_t id, ForeignFeatureRef rel) :
		taggedId_((id << 1) | 1),
		foreignRelation_(rel)
	{
	}

	int compareTo(const Membership& other) const
	{
		if(isForeign())
		{
			if(!other.isForeign()) return 1;
			int res =
				(foreignRelation_.tip > other.foreignRelation_.tip) -
				(foreignRelation_.tip < other.foreignRelation_.tip);
			if(res != 0) return res;
		}
		else
		{
			if(other.isForeign()) return -1;
		}
		return (taggedId_ > other.taggedId_) - (taggedId_ < other.taggedId_);
	}

    uint64_t id() const { return taggedId_ >> 1; }
    bool isForeign() const { return taggedId_ & 1; }
	TRelation* localRelation() const
    {
        assert(!isForeign());
		return localRelation_;
    }

	ForeignFeatureRef foreignRelation() const
	{
		assert(isForeign());
		return foreignRelation_;
	}

    bool sortedInsert(Membership** pFirst)
	{
	    Membership** pPrev = pFirst;
	    while(*pPrev)
	    {
	        Membership* prev = *pPrev;
	        int res = compareTo(*prev);
	        if(res == 0) return false;
	        if(res < 0) break;
	        pPrev = &prev->next_;
	    }
	    next_ = *pPrev;
	    *pPrev = this;
	    return true;
	}

private:
	uint64_t taggedId_;
    union
    {
    	TRelation* localRelation_;
        ForeignFeatureRef foreignRelation_;
    };
};

static_assert(sizeof(Membership) == 24);


