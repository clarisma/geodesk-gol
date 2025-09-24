// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "BriefQueryPrinter.h"

#include <clarisma/text/TextMetrics.h>
#include <geodesk/feature/Tags.h>

using namespace clarisma;

void BriefQueryPrinter::printFeature(FeaturePtr feature)
{
    addFeature(feature);
    if (features_.size() == BATCH_SIZE) printFeatures();
}


void BriefQueryPrinter::printFooter()
{
    printFeatures("\n");
}


void BriefQueryPrinter::addFeature(FeaturePtr feature)
{
    Tags tags(store_, feature);
    for (Tag tag : tags)
    {
        maxKeyWidth_ = std::max(maxKeyWidth_,
            static_cast<int>(TextMetrics::countCharsUtf8(tag.key())));
        maxValueWidth_ = std::max(maxValueWidth_, tag.value().charCount());
    }
    features_.push_back(feature);
}


void BriefQueryPrinter::printFeatures(const char* tail)
{
    ConsoleWriter out;
    out.blank();
    for (FeaturePtr feature : features_)
    {
        printFeature(out, feature, maxKeyWidth_, maxValueWidth_);
    }
    out << tail;
    features_.clear();
    maxKeyWidth_ = 0;
    maxValueWidth_ = 0;
}

void BriefQueryPrinter::printFeature(clarisma::ConsoleWriter& out,
    FeaturePtr feature, int keyWidth, int valWidth)
{
    constexpr AnsiColor KEY_COLOR{"\033[38;5;137m"};
    constexpr AnsiColor GRAY{"\033[38;5;239m"};
    constexpr AnsiColor LIGHTGRAY{"\033[38;5;245m"};
    constexpr AnsiColor NODE_COLOR{"\033[38;5;147m"};
    constexpr AnsiColor WAY_COLOR{"\033[38;5;121m"};
    constexpr AnsiColor RELATION_COLOR{"\033[38;5;135m"};
    constexpr AnsiColor TYPE_COLORS[3] = { NODE_COLOR, WAY_COLOR, RELATION_COLOR };

    out << TYPE_COLORS[feature.typeCode()] << feature.typeName()
        << GRAY << '/' << LIGHTGRAY << feature.id() << "\n";
    Tags tags(store_, feature);
    for (Tag tag : tags)
    {
        int w = static_cast<int>(TextMetrics::countCharsUtf8(tag.key()));
        out << "  " << KEY_COLOR << tag.key();
        out.writeRepeatedChar(' ', keyWidth - w);
        out << GRAY << " = " << Console::DEFAULT;
        out << tag.value() << "\n";
    }

}