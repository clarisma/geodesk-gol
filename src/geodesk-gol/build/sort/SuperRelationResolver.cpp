// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "SuperRelationResolver.h"
#include <cassert>
#include <string_view>
#include <clarisma/cli/Console.h>
#include <clarisma/math/Math.h>
#include <clarisma/util/varint.h>
#include "build/util/ProtoGol.h"
#include "build/util/StringCatalog.h"
#include "build/util/TileCatalog.h"
#include "FastFeatureIndex.h"

const std::vector<SuperRelation*>* SuperRelationResolver::resolve()
{
    SuperRelation* rel = superRelations_.first();
    while (rel)
    {
        if (!rel->isResolved_)
        {
            resolve(rel);
        }
        if (rel->tilePair_.isNull())
        {
            // TODO: omitted empty relation
        }
        else if (rel->level_ > MAX_RELATION_LEVEL)
        {
            // TODO: omitted excessively nested relation
        }
        else
        {
            // Place relation into the vector for its level
            levels_[rel->level_].push_back(rel);
        }
        rel = rel->next();
    }

    for (int i = 0; i <= MAX_RELATION_LEVEL; i++)
    {
        // Sort relations in each level by ID
        std::sort(levels_[i].begin(), levels_[i].end(),
            [](const SuperRelation* a, const SuperRelation* b) -> bool
            {
                return a->id() < b->id();
            });
    }
    return levels_;
}

bool SuperRelationResolver::resolve (SuperRelation* rel)
{
    assert(!rel->isResolved_);
    rel->isPending_ = true;
    TilePair tilePair = rel->tilePair();
    int maxChildLevel = 0;

    for (SortedChildFeature& member : rel->members_)
    {
        if ((member.typedId & 3) == 2)
        {
            // The member is a relation
            uint64_t memberId = member.typedId >> 2;

            // first, look up the child relation in the relation index
            int memberPilePair = relationIndex_.get(memberId);
            TilePair memberTilePair;
            if (memberPilePair)
            {
                // Regular relation that has already been indexed
                memberTilePair = tileCatalog_.tilePairOfPilePair(memberPilePair);
            }
            else
            {
                // Child is a super-relation, or missing
                auto it = superRelationsById_.find(memberId);
                if (it == superRelationsById_.end())
                {
                    // Relation isn't in the super-relations index, either,
                    // so it's missing --> clear the ID
                    // In a later cycle, we count the missing relations
                    // and re-code the member table, if needed
                    member.typedId = 0;
                    continue;
                }
                
                SuperRelation* child = it->second;
                if (!child->isResolved_)
                {
                    // Child is an unresolved super-relation
                    if (child->isPending_)
                    {
                        // Refcycle detected: Put the child in the
                        //  list of cyclical relations
                        assert(cyclicalRelations_.empty());
                            // This child must be the first cyclical
                        cyclicalRelations_.emplace_back(rel, child);
                        rel->isPending_ = false;
                        return false;
                    }
                    while (!resolve(child))
                    {
                        assert(!cyclicalRelations_.empty());
                        cyclicalRelations_.emplace_back(rel, child);
                        if (cyclicalRelations_[0].child == rel)
                        {
                            // The current relation is the root of
                            // the reference cycle --> we're reasy to resolve it
                            SuperRelation* loser = breakReferenceCycle();

                            // If the child relation was removed from this relation,
                            // we don't try to resolve again
                            if (rel == loser) break;
                        }
                        else
                        {
                            rel->isPending_ = false;
                            return false;
                        }
                    }
                }
                if (member.typedId == 0) continue;
                memberPilePair = child->pilePair_;
                memberTilePair = child->tilePair_;
                if (memberTilePair.isNull())
                {
                    member.typedId = 0;
                    continue;
                }
                maxChildLevel = std::max(maxChildLevel, child->level_);
            }
            member.pilePair = memberPilePair;
            member.tilePair = memberTilePair;
            tilePair += memberTilePair;
        }
    }
    if (!tilePair.isNull())
    {
        tilePair = tileCatalog_.normalizedTilePair(tilePair);
        rel->tilePair_ = tilePair;
        rel->pilePair_ = tileCatalog_.pilePairOfTilePair(tilePair);
    }
    rel->isResolved_ = true;
    rel->isPending_ = false;
    rel->level_ = maxChildLevel + 1;
    rel->validate();
    return true;
}


double SuperRelationResolver::calculateScore(const SuperRelation* rel) const
{
    double score = 0;

    ByteSpan body = rel->body();
    const uint8_t* p = body.data();
    int nonRelationMemberCount = 0;
    for (int i = 0; i < rel->members().size(); i++)
    {
        uint64_t typedMemberId = readVarint64(p);
        if ((typedMemberId & 3) != 2) nonRelationMemberCount++;
        std::string_view role = ProtoGol::readStringView(p, ProtoStringPair::VALUE, strings_);
    }

    if (nonRelationMemberCount == 0)
    {
        // Relation only has other relations as members:
        // very high probability this relation is at the top of the hierarchy
        score += 1'000'000'000;
    }
    else
    {
        score += nonRelationMemberCount;	// 1 point for each node or way
    }

    while (p < body.end())
    {
        std::string_view key = ProtoGol::readStringView(p, ProtoStringPair::KEY, strings_);
        std::string_view value = ProtoGol::readStringView(p, ProtoStringPair::VALUE, strings_);
        if (key == "type")
        {
            if (value == "superroute" || value == "route_master")
            {
                score += 50'000'000;
            }
            else if (value == "network")
            {
                score += 100'000'000;
            }
            else if (value == "site")
            {
                score += 20'000'000;
            }
        }
        else if (key == "admin_level")
        {
            double level;
            if (Math::parseDouble(value, &level))
            {
                score += (14 - level) * 1'000'000;
            }
        }
    }
    return score;
}



SuperRelation* SuperRelationResolver::breakReferenceCycle()
{
    assert(cyclicalRelations_.size() >= 2);
        // There must be at least 2 cyclical relations 
        // (Simple self-references should have been discarded earlier)
    for (CyclicalRelation& cyclical : cyclicalRelations_)
    {
        cyclical.score = calculateScore(cyclical.relation);
    }
    std::sort(cyclicalRelations_.begin(), cyclicalRelations_.end());

    // Remove child from the relation with the lowest score
    SuperRelation* loser = cyclicalRelations_[0].relation;
    SuperRelation* child = cyclicalRelations_[0].child;
    if(Console::verbosity() >= Console::Verbosity::VERBOSE)	[[unlikely]]
    {
        Console::msg(
            "Removed relation/%lld from relation/%lld "
            "to break reference cycle", child->id(), loser->id());
    }
    loser->clearMember((child->id() << 2) | 2);
    loser->removedRefcyleCount_++;
    cyclicalRelations_.clear();
    return loser;
}
