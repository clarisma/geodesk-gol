// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/cli/ConsoleWriter.h>

#include "SimpleQueryPrinter.h"

using namespace geodesk;

class BriefQueryPrinter : public SimpleQueryPrinter
{
public:
    explicit BriefQueryPrinter(QuerySpec* spec) :
        SimpleQueryPrinter(spec) {}

protected:
    void printFeature(FeaturePtr feature) override;
    void printFooter() override;

private:
    void addFeature(FeaturePtr feature);
    void printFeatures(const char* tail = "");
    void printFeature(clarisma::ConsoleWriter& out,
        FeaturePtr feature, int keyWidth, int valWidth);

    static constexpr size_t BATCH_SIZE = 64;

    int prevMaxKeyWidth_ = 0;
    int prevMaxValueWidth_ = 0;
    int maxKeyWidth_ = 0;
    int maxValueWidth_ = 0;
    std::vector<FeaturePtr> features_;
};
