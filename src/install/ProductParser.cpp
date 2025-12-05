// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "ProductParser.h"

void ProductParser::parse(std::string_view requiredPlatform)
{
    expect('{');
    for (;;)
    {
        std::string_view key = expectString();
        expect(':');
        if (key == "platforms")
        {
            expect('{');
            for (;;)
            {
                std::string_view platform = expectString();
                expect(':');
                std::string_view url = expectString();
                if (platform == requiredPlatform)
                {
                    url_ = url;
                }
                if (!accept(','))
                {
                    expect('}');
                    break;
                }
            }
        }
        else
        {
            std::string_view value = expectString();
            if (key == "version")
            {
                version_ = SemanticVersion(value);
            }
        }
        if (!accept(',')) break;
    }
}