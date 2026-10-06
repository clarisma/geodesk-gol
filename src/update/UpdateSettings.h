// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "tag/AreaClassifier.h"

class UpdateSettings 
{
public:
    std::vector<AreaClassifier::Entry>& areaRules() { return areaRules_; };
    void setAreaRules(const char* rules);
    int threadCount() const { return threadCount_; }
    void setThreadCount(int count) { threadCount_ = count; }
    size_t bufferSize() const { return bufferSize_; }
    void setBufferSize(size_t size) { bufferSize_ = size; }
    void complete();

private:
    std::vector<AreaClassifier::Entry> areaRules_;
    size_t bufferSize_ = 0;
    int threadCount_ = 0;
};
