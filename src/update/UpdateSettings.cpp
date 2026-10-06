// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "UpdateSettings.h"

void UpdateSettings::setAreaRules(const char* rules)
{
    areaRules_ = AreaClassifier::Parser(rules).parseRules();
}

void UpdateSettings::complete()
{
    if(areaRules_.empty()) setAreaRules(AreaClassifier::DEFAULT);
}
