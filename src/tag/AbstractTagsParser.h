// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/Parser.h>

class AbstractTagsParser : public clarisma::Parser
{
public:
    explicit AbstractTagsParser(const char *s) : Parser(s) {}

    std::string_view expectKey();

protected:
    // TODO :These are the same as MatcherParser, maybe reuse?
    static const clarisma::CharSchema VALID_FIRST_CHAR;
    static const clarisma::CharSchema VALID_NEXT_CHAR;
};
