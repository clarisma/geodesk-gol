// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "AbstractQueryCommand.h"
#include <clarisma/cli/CliHelp.h>
#include <geodesk/query/Query.h>

using namespace clarisma;
using namespace geodesk;

AbstractQueryCommand::AbstractQueryCommand()
{
}

bool AbstractQueryCommand::setParam(int number, std::string_view value)
{
    if (number >= 2)
    {
        if (number > 2) query_ += " ";
        query_ += value;
        return true;
    }
    return GolCommand::setParam(number, value);
}


