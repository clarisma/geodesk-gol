// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <string_view>
#include <clarisma/math/Math.h>


namespace geodesk {

/// Calculates the score for a super-relation; useful for breaking
/// refcycles. The higher the score, the greater the likelihood
/// that this relation is at the top of a refcycle.
///
class SuperRelationScore
{
public:
    void addTag(std::string_view key, std::string_view value, double numValue)
    {
        if (key == "type")
        {
            if (value == "superroute" || value == "route_master")
            {
                score_ += 50'000'000;
            }
            else if (value == "network")
            {
                score_ += 100'000'000;
            }
            else if (value == "site")
            {
                score_ += 20'000'000;
            }
        }
        else if (key == "admin_level")
        {
            if (!value.empty())
            {
                if (!clarisma::Math::parseDouble(value, &numValue)) return;
            }
            if (numValue >= 1 && numValue <= 13)
            {
                score_ += (14 - numValue) * 1'000'000;
            }
        }
    }

    void addMemberCounts(int nodes, int ways, int /* relations */)
    {
        int nonRelationMemberCount = nodes + ways;
            // relation count is currently not used for scoring
        if (nonRelationMemberCount == 0)
        {
            // Relation only has other relations as members:
            // very high probability this relation is at the top of the hierarchy
            score_ += 1'000'000'000;
        }
        else
        {
            score_ += nonRelationMemberCount;	// 1 point for each node or way
        }
    }

    double score() const { return score_; }

private:
    double score_ = 0;
};

}  // namespace geodesk