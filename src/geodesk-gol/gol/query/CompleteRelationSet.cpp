// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CompleteRelationSet.h"
#include <geodesk/feature/StringTable.h>

void CompleteRelationSet::update(std::string_view types, const StringTable& strings)
{
    if (types == "all")
    {
        completeRelations_ = ALL;
    }
    else if (types == "none")
    {
        completeRelations_ = NONE;
    }
    else
    {
        completeRelations_ = SOME;
        acceptedTypes_.addStringList(types, &strings);
        typeGlobalKey_ = strings.getCode("type");
    }
}

bool CompleteRelationSet::contains(RelationPtr rel) const noexcept
{
    if (completeRelations_ == SOME)
    {
        TagTablePtr tags = rel.tags();
        TagBits value;
        if (typeGlobalKey_ > 0) [[likely]]
        {
            value = tags.getGlobalKeyValue(typeGlobalKey_);
        }
        else
        {
            value = tags.getLocalKeyValue("type", 4);
        }

        // If value not found, will be zero-value

        int valueType = TagTablePtr::valueType(value);
        if (valueType == TagValueType::GLOBAL_STRING)   [[likely]]
        {
            return acceptedTypes_.hasCode(TagTablePtr::rawNarrowValue(value));
        }
        if (valueType == TagValueType::LOCAL_STRING)
        {
            return acceptedTypes_.hasString(
                tags.localString(value)->toStringView());
        }
        return false;
    }
    return completeRelations_ == ALL;
}
