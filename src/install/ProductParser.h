// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#pragma once
#include <clarisma/util/DateTime.h>
#include <clarisma/util/Parser.h>
#include <clarisma/util/SemanticVersion.h>

using namespace clarisma;

class ProductParser : Parser
{
public:
    explicit ProductParser(const char* pInput) : Parser(pInput) {}

    void parse(std::string_view platform);

private:
    SemanticVersion version_;
    DateTime timestamp_;
    std::string_view url_;
};

