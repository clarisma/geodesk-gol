// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <limits>


namespace geodesk {

template <typename R>
class CyclicalRelationResolver
{
public:
    void add(double parentScore, R* parent, R* child)
    {
        // TODO: Use ID as tie breaker?
        if (parentScore < lowestScore_)
        {
            lowestScore_ = parentScore;
            parent_ = parent;
            child_ = child;
        }
    }

    R* losingParent() const { return parent_; }
    R* childToRemove() const { return child_; }

    void reset()
    {
        lowestScore_ = std::numeric_limits<double>::max();
        parent_ = child_ = nullptr;
    }

private:
    double lowestScore_ = std::numeric_limits<double>::max();
    R* parent_ = nullptr;
    R* child_ = nullptr;
};

}  // namespace geodesk