// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "FeatureRow.h"
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/TagWalker.h>
#include <geodesk/format/KeySchema.h>
#include <geodesk/geom/Centroid.h>

using namespace clarisma;

FeatureRow::FeatureRow(const KeySchema& keys, FeatureStore* store,
    FeaturePtr feature, int precision) :
    SmallArray(keys.columnCount())
{
    char buf[32];

    size_t colCount = keys.columnCount();
    for (int i=0; i<colCount; i++)
    {
        (*this)[i] = StringHolder();
    }

    Coordinate xy;
    int idCol = keys.columnOfSpecial(KeySchema::ID);
    int lonCol = keys.columnOfSpecial(KeySchema::LON);
    int latCol = keys.columnOfSpecial(KeySchema::LAT);

    if (idCol)
    {
        char* end = &buf[sizeof(buf)];
        char* p = Format::unsignedIntegerReverse(feature.id(), end);
        p--;
        *p = "NWRX"[feature.typeCode()];
        (*this)[idCol-1] = StringHolder::inlineCopy(p, end-p);
    }
    if ((lonCol | latCol)  != 0)
    {
        xy = Centroid::ofFeature(store, feature);
        if (lonCol)  [[likely]]
        {
            char* p = Format::formatDouble(buf,
                Mercator::lonFromX(xy.x), precision);
            (*this)[lonCol-1] = StringHolder::inlineCopy(buf, p-buf);
        }
        if (latCol)  [[likely]]
        {
            char* p = Format::formatDouble(buf,
                Mercator::latFromY(xy.y), precision);
            (*this)[latCol-1] = StringHolder::inlineCopy(buf, p-buf);
        }
    }

    TagWalker tw(feature.tags(), store->strings());
    while (tw.next())
    {
        int col;
        if(tw.keyCode() >= 0) [[likely]]
        {
            col = keys.columnOfGlobal(tw.keyCode());
        }
        else
        {
            col = keys.columnOfLocal(tw.key()->toStringView());
        }
        if (col > 0)
        {
            assert(col <= colCount);
            if (tw.isStringValue()) [[likely]]
            {
                (*this)[col-1] = StringHolder(tw.stringValueFast());
            }
            else
            {
                (*this)[col-1] = StringHolder(tw.numberValueFast());
            }
        }
    }
}
