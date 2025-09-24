// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class AbstractQueryCommand : public GolCommand
{
public:
    AbstractQueryCommand();

private:
    bool setParam(int number, std::string_view value) override;

protected:
    std::string query_;
};

