// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ParallelQueryPrinter.h"
#include <clarisma/text/Csv.h>
#include <geodesk/format/FeatureRow.h>

using namespace geodesk;

class CsvQueryPrinter : public ParallelQueryPrinter<CsvQueryPrinter>
{
public:
    explicit CsvQueryPrinter(const QuerySpec* spec) :
        ParallelQueryPrinter(spec)
    {
    }

    void printHeader() override
    {
        clarisma::ConsoleWriter out;
        out.blank();
        bool isFirst = true;
        for (auto header : spec()->keys().columns())
        {
            if (!isFirst) out.writeByte(',');
            isFirst = false;
            out << header;
        }
        out.writeByte('\n');
    }

    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) // CRTP override
    {
        const KeySchema& keys = spec()->keys();
        FeatureRow row(keys, store, feature, spec()->precision());
        size_t colCount = keys.columnCount();
        for (int i=0; i<colCount; i++)
        {
            if (i > 0) out.writeByte(',');
            clarisma::Csv::writeEscaped(out, row[i].toStringView());
        }
        out.writeByte('\n');
    }
};
