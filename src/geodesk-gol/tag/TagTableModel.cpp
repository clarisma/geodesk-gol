// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TagTableModel.h"

#include <geodesk/feature/GlobalTagIterator.h>
#include <geodesk/feature/LocalTagIterator.h>

void TagTableModel::read(TagTablePtr p)
{
    assert(isEmpty());
    int32_t fakeHandle = static_cast<int32_t>(p.ptr()) & 2;
    // We don't need the handle; however, we need to respect the alignment
    // to ensure that pointer calculations work; hence, handle will be 0 or 2
    if(p.hasLocalKeys())
    {
        LocalTagIterator iterLocal(fakeHandle, p);
        while(iterLocal.next())
        {
            std::string_view key = iterLocal.keyString()->toStringView();
            if(iterLocal.hasLocalStringValue())
            {
                addLocalTag(key, iterLocal.localStringValue()->toStringView());
            }
            else
            {
                addLocalTag(key, iterLocal.valueType(), iterLocal.value());
            }
        }
    }
    GlobalTagIterator iterGlobal(fakeHandle, p);
    while(iterGlobal.next())
    {
        if(iterGlobal.hasLocalStringValue())
        {
            addGlobalTag(iterGlobal.key(), iterGlobal.localStringValue()->toStringView());
        }
        else
        {
            addGlobalTag(iterGlobal.key(), iterGlobal.valueType(), iterGlobal.value());
        }
    }
    // No need to normalize, table is already in canonical order
}
