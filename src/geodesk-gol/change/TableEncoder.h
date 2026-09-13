// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/HashMap.h>
#include <clarisma/util/BufferWriter.h>
#include "change/model/CFeature.h"

using clarisma::HashMap;

template<int ForeignFlag, int RoleFlag, int NarrowDeltaBitCount, int StartTex>
class TableEncoder
{
public:
    TableEncoder(Tip localTip, std::vector<uint32_t>& table,
        const HashMap<const CFeature*,int>& features, int localBase) :
        table_(table),
        localFeatures_(features),
        localTip_(localTip),
        storageSize_(0),
        localBase_(localBase),
        prevForeign_(Tip(0), StartTex)
    {
    }

    void add(const CFeature* member,
        CFeature::Role role = CFeature::Role(),
        const CFeature* next = nullptr)
    {
        int differentRoleFlag = (role != prevRole_) ? 2 : 0;
        if(member->isInTile(localTip_))
        {
            constexpr int LocalFlagCount = RoleFlag ? 2 : 1;
            auto it = localFeatures_.find(member);
            if (it == localFeatures_.end())
            {
                LOGS << member->typedId() << " not found in local-feature table, TIP=" << localTip_;
            }
            assert(it != localFeatures_.end());
            table_.push_back(((it->second - localBase_)
                << LocalFlagCount) | differentRoleFlag);
            storageSize_ += 4;
        }
        else
        {
            CRef ref;
            if(member->isDualTile())  [[unlikely]]
            {
                // TODO: use optimal selection
                ref = member->ref();
                if (ref.tip().isNull()) [[unlikely]]
                {
                    ref = member->refSE();
                }
            }
            else
            {
                ref = member->ref();
            }
            if (ref.tip().isNull())
            {
                LOGS << "TableEncoder: " << member->typedId() << " is unresolved.";
            }
            assert(!ref.tip().isNull());

            // TODO: dummy code since we're not assigning TEXes yet...
            //  All foreign members must have a TEX at this point
            if(!ref.isExported())
            {
                ref = CRef::ofExported(ref.tip(), 333);    // TODO: dummy code
            }

            constexpr int ForeignFlagCount = 1 + (ForeignFlag ? 1 : 0) + (RoleFlag ? 1 : 0);

            int differentTileFlag = 0;
            if(ref.tip() != prevForeign_.tip)
            {
                differentTileFlag = 1 << (ForeignFlagCount-1);
                if(prevForeign_.tip.isNull())
                {
                    prevForeign_.tip = FeatureConstants::START_TIP;
                }
            }

            TexDelta texDelta = ref.tex() - prevForeign_.tex;
            table_.push_back((clarisma::toZigzag(texDelta) << ForeignFlagCount)
                | ForeignFlag | differentRoleFlag | differentTileFlag);
            storageSize_ += texDelta.isWide(NarrowDeltaBitCount) ? 4 : 2;
            prevForeign_.tex += texDelta;

            if(differentTileFlag)
            {
                TipDelta tipDelta = ref.tip() - prevForeign_.tip;
                table_.push_back(clarisma::toZigzag(tipDelta));
                    // need to explicitly zigzag-encode the TIP delta
                    // (turning int into an unsigned int)
                    // because write simply emits unsigned ints
                storageSize_ += tipDelta.isWide() ? 4 : 2;
                prevForeign_.tip += tipDelta;
            }
        }

        if(differentRoleFlag)
        {
            table_.push_back(static_cast<uint32_t>(role));
            prevRole_ = role;
            storageSize_ += role.isGlobal() ? 2 : 4;
        }
    }

    void write(clarisma::BufferWriter& out) const
    {
        assert(storageSize_ == 0 || !table_.empty());
        out.writeVarint(storageSize_);
        for(uint32_t val : table_)
        {
            out.writeVarint(val);
        }
        table_.clear();
    }

private:
    const HashMap<const CFeature*,int>& localFeatures_;
    std::vector<uint32_t>& table_;
    Tip localTip_;
    uint32_t storageSize_;
    int localBase_;
    CFeature::Role prevRole_;
    ForeignFeatureRef prevForeign_;
};


// Foreign flag (in TES) is bit 0 (=1)
// Different-role flag (in TES) is bit 1 (=2)
// Narrow TEX of a member can be encoded in 11 bits (incl. sign)
using MemberTableEncoder = TableEncoder<1,2,11,Tex::MEMBERS_START_TEX>;

// Foreign flag (in TES) is bit 0 (=1)
// No different-role flag (=0)
// Narrow TEX of a member can be encoded in 12 bits (incl. sign)
using WayNodeTableEncoder = TableEncoder<1,0,12,Tex::WAYNODES_START_TEX>;

// No foreign flag in TES (locals always come before foreign, we can
// tell where foreign relations begin by looking at different_tile flag), so =0
// No different-role flag (=0)
// Narrow TEX of a member can be encoded in 12 bits (incl. sign)
// (in theory, could use 13 bits by dropping the foreign-flag in the Tile
// as well, but this encoding is simpler)
using RelationTableEncoder = TableEncoder<0,0,12,Tex::RELATIONS_START_TEX>;