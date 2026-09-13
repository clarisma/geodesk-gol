// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ParallelQueryPrinter.h"
#include <clarisma/text/Format.h>

using namespace geodesk;

class ListQueryPrinter : public ParallelQueryPrinter<ListQueryPrinter>
{
public:
    using ParallelQueryPrinter::ParallelQueryPrinter;

    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) // CRTP override
    {
        char buf[32];
        buf[31] = '\n';
        char* end = buf + sizeof(buf);
        char* start = clarisma::Format::unsignedIntegerReverse(feature.id(), end-1) - 1;
        if(feature.isWay())
        {
            *start = 'W';
        }
        else if(feature.isNode())
        {
            *start = 'N';
        }
        else
        {
            assert(feature.isRelation());
            *start = 'R';
        }
        out.write(start, end - start);
    }
};
