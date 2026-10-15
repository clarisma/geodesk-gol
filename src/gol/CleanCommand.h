// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"
#include <vector>

class CleanCommand : public GolCommand
{
public:
    CleanCommand();

    int run(char* argv[]) override;

private:
    static Option CLEAN_OPTIONS[];

    bool setParam(int number, std::string_view value) override;
    void help() override;
};