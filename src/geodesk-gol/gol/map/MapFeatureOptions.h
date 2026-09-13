// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/text/TextTemplate.h>

struct MapFeatureOptions
{
    bool hasEdit = false;
    bool hasLink = false;
    bool hasPopup = false;
    bool hasTooltip = false;
    clarisma::TextTemplate::Ptr editUrl;
    clarisma::TextTemplate::Ptr linkUrl;
    clarisma::TextTemplate::Ptr popup;
    clarisma::TextTemplate::Ptr tooltip;
};

