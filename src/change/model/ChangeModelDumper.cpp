// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeModelDumper.h"

#include <ranges>
#include <geodesk/geom/LonLat.h>
#include "ChangedFeature2D.h"
#include "ChangedTile.h"
#include "CTagTable.h"
#include "clarisma/io/FileBuffer3.h"


void ChangeModelDumper::dump(const char* fileName)
{
    out_.open(fileName);
    dumpFeatures(FeatureType::NODE, &ChangeModelDumper::dumpNode);
    dumpFeatures(FeatureType::WAY, &ChangeModelDumper::dumpWay);
    dumpFeatures(FeatureType::RELATION, &ChangeModelDumper::dumpRelation);
    dumpTexChanges();
    out_.flush();
    out_.close();
}

template<typename T>
void ChangeModelDumper::dumpFeatures(FeatureType type, void (ChangeModelDumper::*dump)(const T*))
{
    for(auto f: std::views::values(model_.features()))
    {
        CFeature* feature = f->get();
        if(feature->isChanged() && feature->type() == type)
        {
            features_.push_back(ChangedFeatureBase::cast(feature));
        }
    }

    std::sort(features_.begin(), features_.end(),
        [](ChangedFeatureBase* a, const ChangedFeatureBase* b)
        {
            return a->id() < b->id();
        });

    for (ChangedFeatureBase* f : features_)
    {
        (this->*dump)(reinterpret_cast<const T*>(f));
        out_ << "\n";
    }
    features_.clear();
}


void ChangeModelDumper::dumpFeatureStub(const ChangedFeatureBase* feature)
{
    out_ << feature->typedId() << "\n"
        << "  Version: " << feature->version() << "\n"
        << "  Ref:     " << feature->ref();
    if(feature->type() != FeatureType::NODE)
    {
        out_ << " / " << feature->refSE() << "\n";
        dumpBounds(ChangedFeature2D::cast(feature)->bounds());
    }
    else
    {
        out_ << "\n  LonLat:  " << LonLat(feature->xy()) << "\n";
    }
    dumpFlags(feature->flags());
    //if(feature->is(ChangeFlags::TAGS_CHANGED))
    //{
        dumpTags(feature->tagTable());
    //}
    if(feature->is(ChangeFlags::RELTABLE_LOADED))
    {
        dumpParentRelations(feature->parentRelations());
    }

}

void ChangeModelDumper::dumpFlags(ChangeFlags flags)
{
    if (test(flags, ChangeFlags::DELETED)) tempStrings_.push_back("deleted");
    if (test(flags, ChangeFlags::ADDED_TO_RELATION)) tempStrings_.push_back("added_to_relation");
    if (test(flags, ChangeFlags::REMOVED_FROM_RELATION)) tempStrings_.push_back("removed_from_relation");
    if (test(flags, ChangeFlags::RELTABLE_LOADED)) tempStrings_.push_back("reltable_loaded");
    if (test(flags, ChangeFlags::RELTABLE_CHANGED)) tempStrings_.push_back("reltable_changed");
    if (test(flags, ChangeFlags::NODE_WILL_SHARE_LOCATION)) tempStrings_.push_back("node_will_share_location");
    if (test(flags, ChangeFlags::TAGS_CHANGED)) tempStrings_.push_back("tags_changed");
    if (test(flags, ChangeFlags::GEOMETRY_CHANGED)) tempStrings_.push_back("geometry_changed");
    if (test(flags, ChangeFlags::MEMBERS_CHANGED)) tempStrings_.push_back("members_changed");
    if (test(flags, ChangeFlags::WAYNODE_IDS_CHANGED)) tempStrings_.push_back("waynode_ids_changed");
    if (test(flags, ChangeFlags::WILL_BE_AREA)) tempStrings_.push_back("will_be_area");
    if (test(flags, ChangeFlags::AREA_STATUS_CHANGED)) tempStrings_.push_back("area_status_changed");
    if (test(flags, ChangeFlags::PROCESSED)) tempStrings_.push_back("processed");
    if (test(flags, ChangeFlags::BOUNDS_CHANGED)) tempStrings_.push_back("bounds_changed");
    if (test(flags, ChangeFlags::TILES_CHANGED)) tempStrings_.push_back("tiles_changed");
    if (test(flags, ChangeFlags::WILL_HAVE_WAYNODE_FLAG)) tempStrings_.push_back("will_have_waynode_flag");
    if (test(flags, ChangeFlags::WAYNODE_STATUS_CHANGED)) tempStrings_.push_back("waynode_status_changed");
    if (test(flags, ChangeFlags::SHARED_LOCATION_STATUS_CHANGED)) tempStrings_.push_back("shared_location_status_changed");
    if (test(flags, ChangeFlags::REMOVED_FROM_WAY)) tempStrings_.push_back("removed_from_way");
    if (test(flags, ChangeFlags::RELATION_DEFERRED)) tempStrings_.push_back("relation_deferred");
    if (test(flags, ChangeFlags::RELATION_ATTEMPTED)) tempStrings_.push_back("relation_attempted");
    if (test(flags, ChangeFlags::NEW_TO_NORTHWEST)) tempStrings_.push_back("new_to_northwest");
    if (test(flags, ChangeFlags::NEW_TO_SOUTHEAST)) tempStrings_.push_back("new_to_southeast");
    if (test(flags, ChangeFlags::MEMBER_TILES_CHANGED)) tempStrings_.push_back("member_tiles_changed");
    if (test(flags, ChangeFlags::WILL_BE_SUPER_RELATION)) tempStrings_.push_back("will_be_super_relation");

    out_ << "  Flags:\n";
    for(const char* s : tempStrings_)
    {
        out_ << "    " << s << "\n";
    }
    tempStrings_.clear();
}

void ChangeModelDumper::dumpTags(const CTagTable* tags)
{
    if(tags == nullptr)
    {
        out_ << "  Tags:    (null)\n";
        return;
    }
    if(tags == &CTagTable::EMPTY)
    {
        out_ << "  Tags:    (empty)\n";
        return;
    }
    out_ << "  Tags:\n";
    for(auto tag : tags->localTags())
    {
        out_ << "    ";
        dumpLocalString(tag.key());
        dumpTagValue(tag);
    }
    for(auto tag : tags->globalTags())
    {
        out_ << "    ";
        dumpGlobalString(tag.key());
        dumpTagValue(tag);
    }
}

void ChangeModelDumper::dumpTagValue(CTagTable::Tag tag)
{
    uint32_t v = tag.value();
    out_ << '=';
    switch(tag.type())
    {
    case TagValueType::GLOBAL_STRING:
        dumpGlobalString(v);
        break;
    case TagValueType::LOCAL_STRING:
        dumpLocalString(v);
        break;
    case TagValueType::NARROW_NUMBER:
        out_ << TagValues::intFromNarrowNumber(v);
        break;
    case TagValueType::WIDE_NUMBER:
        out_ << TagValues::decimalFromWideNumber(v);
        break;
    }
    out_ << "\n";
}

void ChangeModelDumper::dumpParentRelations(const CRelationTable* rels)
{
    if(rels == nullptr)
    {
        out_ << "  Parents: (null)\n";
        return;
    }
    out_ << "  Parents:\n";
    for(auto relStub : rels->relations())
    {
        const CFeature* rel = relStub->get();
        out_ << "    " << rel->typedId() << ": "
            << rel->ref() << " / " << rel->refSE() << "\n";
    }
}

void ChangeModelDumper::dumpGlobalString(uint32_t code)
{
    out_ << *model_.store()->strings().getGlobalString(code);
}

void ChangeModelDumper::dumpLocalString(uint32_t code)
{
    out_ << "\"" << *model_.getString(code) << "\"";
}

void ChangeModelDumper::dumpBounds(const Box& bounds)
{
    out_ << "  Bounds:  ";
    if(bounds.isEmpty())
    {
        out_ <<" (empty)\n";
        return;
    }
    out_ << LonLat(bounds.bottomLeft()) << " -> "
        << LonLat(bounds.topRight()) << "\n";
}

void ChangeModelDumper::dumpNode(const ChangedNode* node)
{
    dumpFeatureStub(node);
}

void ChangeModelDumper::dumpWay(const ChangedFeature2D* way)
{
    dumpFeatureStub(way);
    out_ << "  Nodes:\n";
    for(CFeatureStub* nodeStub : way->members())
    {
        CFeature* node = nodeStub->get();
        out_ << "    node/" << node->id() << ": " << node->ref() << "\n";
    }
}

void ChangeModelDumper::dumpRelation(const ChangedFeature2D* rel)
{
    dumpFeatureStub(rel);
    out_ << "  Members:\n";
    for(int i=0; i < rel->memberCount(); i++)
    {
        const CFeatureStub* memberStub = rel->members()[i];
        if(memberStub == nullptr)
        {
            out_ << "    (omitted)\n";
            continue;
        }
        const CFeature* member = memberStub->get();
        out_ << "    " << member->typedId();
        CFeature::Role role = rel->roles()[i];
        if(role.isGlobal())
        {
            if(role.value() != 0)
            {
                out_ << " as ";
                dumpGlobalString(role.value());
            }
        }
        else
        {
            out_ << " as ";
            dumpLocalString(role.value());
        }
        out_ << ": " << member->ref();
        if(member->type() != FeatureType::NODE)
        {
            out_ << " / " << member->refSE();
        }
        out_ << "\n";
    }
}

void ChangeModelDumper::dumpTexChanges()
{
    for(const auto& [tip,changedTile] : model_.changedTiles())
    {
        if (changedTile->hasTexChanges())
        {
            out_ << "  " << tip << ":\n";
            out_ << "  Potential TEX Gainers:\n";
            for (const CFeatureStub* f : changedTile->mayGainTex())
            {
                // LOGS << tip << ": " << f->typedId() << " may gain TEX";
                out_ << "    " << f->typedId() << "\n";
            }
            out_ << "\n";
        }
    }

    out_ << "Potential TEX Losers:\n";
    for (CFeatureStub* f : model_.mayLoseTex())
    {
        if (!f->get()->isFutureForeign())
        {
            out_ << "  " << f->typedId() << "\n";
        }
    }
}