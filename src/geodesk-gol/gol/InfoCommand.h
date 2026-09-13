// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "GolCommand.h"

namespace clarisma {
class ConsoleWriter;
}



class InfoCommand : public GolCommand
{
public:
    InfoCommand();

    int run(char* argv[]);

private:
    bool setParam(int number, std::string_view value) override;
    int setOption(std::string_view name, std::string_view value) override;
    void help() override;
    void showRevisionInfo(clarisma::ConsoleWriter& out);
    void printTileStatistics(clarisma::ConsoleWriter& out);

    std::string query_;
};

