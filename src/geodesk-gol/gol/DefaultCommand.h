// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "BasicCommand.h"

class DefaultCommand : public BasicCommand
{
public:
    // DefaultCommand();
    int run(char* argv[]);

private:
    int setOption(std::string_view name, std::string_view value) override;
    void help();

    bool showVersion_ = false;
    bool showHelp_ = false;
};

