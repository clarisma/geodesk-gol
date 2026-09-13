// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "CFeatureStub.h"

class CFeature : public CFeatureStub
{
public:
    using CFeatureStub::CFeatureStub;

    static CFeature* cast(CFeatureStub* f)
    {
        assert(!f || f->isBasic());
        return reinterpret_cast<CFeature*>(f);
    }

    Coordinate xy() const noexcept
    {
        assert(type() == FeatureType::NODE);
        assert(!isReplaced());
        return xy_;
    }

    void setXY(Coordinate xy)
    {
        assert(type() == FeatureType::NODE);
        assert(!isReplaced());
        xy_ = xy;
    }

    CRef ref() const noexcept
    {
        assert(!isReplaced());
        return ref_;
    }

    void setRef(CRef ref) noexcept
    {
        assert(!isReplaced());
        assert(ref != CRef::SINGLE_TILE);  // only SE ref can be single-tile
        ref_ = ref;
    }

    void offerRef(CRef ref) noexcept
    {
        assert(!isReplaced());
        assert(!ref.tip().isNull() || ref==CRef::ANONYMOUS_NODE);
        assert(ref != CRef::SINGLE_TILE);  // only SE ref can be single-tile
        if(ref_. isVague()) ref_ = ref;
    }

    /*
    void setRef(const WayNodeIterator::WayNode& node)
    {
        assert(type() == FeatureType::NODE);
        assert(!isReplaced());

        if(node.feature.isNull())
        {
            ref_ = CRef::ANONYMOUS_NODE;
        }
        else
        {
            if(node.foreign.isNull())
            {
                f->setRef(refOfLocal(node.feature));
            }
            else
            {
                f->setRef(CRef::ofForeign(node.foreign));
            }
        }
    }
    */

    // TODO: Could simply return CRef::SINGLE_TILE for nodes, simplifies
    //  a lot of logic -- could also face refSEfast() for cases when we
    //  know this feature is not a node
    CRef refSE() const noexcept
    {
        assert(type() != FeatureType::NODE);
        assert(!isReplaced());
        return refSE_;
    }

    void setRefSE(CRef ref) noexcept
    {
        assert(type() != FeatureType::NODE);
        assert(!isReplaced());
        refSE_ = ref;
    }

    void offerRefSE(CRef ref) noexcept
    {
        assert(type() != FeatureType::NODE);
        assert(!isReplaced());
        assert(!ref.tip().isNull());
        if(refSE_.isVague()) refSE_ = ref;
    }


    bool isInTile(Tip tip) const noexcept
    {
        if(ref_.tip() == tip) return true;
        return type() != FeatureType::NODE && refSE_.tip() == tip;
    }

    // TODO: can we assume all unknown refs are resolved?
    bool isDualTile() const noexcept
    {
        return type() != FeatureType::NODE && refSE_ != CRef::SINGLE_TILE;
    }

    FeaturePtr getFeature(FeatureStore* store) const
    {
        FeaturePtr feature = ref_.getFeature(store);
        if(feature.isNull() && type() != FeatureType::NODE)    [[unlikely]]
        {
            feature = refSE_.getFeature(store);
        }
        return feature;
    }

    // TODO: This flag needs to be copied over if we replace the CFeature
    //  with a changed version
    // TODO: remove??
    void markAsFutureWaynode()
    {
        idAndFlags_ |= FUTURE_WAYNODE;
    }

    bool isFutureWaynode() const
    {
        return idAndFlags_ & FUTURE_WAYNODE;
    }

    void markAsFutureForeign()
    {
        idAndFlags_ |= FUTURE_FOREIGN;
    }

    bool isFutureForeign() const
    {
        return idAndFlags_ & FUTURE_FOREIGN;
    }

    bool willBeForeignMember(FeatureStore* store) const;

    bool isForeignMemberOf(const CFeature* parent) const
    {
        assert(parent->type() != FeatureType::NODE);
        if (ref().tip() != parent->ref().tip()) return true;
        if (type() == FeatureType::NODE) return false;
        return refSE().tip() != parent->refSE().tip();
    }
};
