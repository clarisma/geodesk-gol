// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "AbstractQueryCommand.h"
#include <geodesk/format/KeySchema.h>
#include "gol/query/OutputFormat.h"


class QueryCommand : public GolCommand
{
public:
    QueryCommand();

    int run(char* argv[]) override;

private:
    static Option QUERY_OPTIONS[];

    bool setParam(int number, std::string_view value) override;
    static OutputFormat format(std::string_view s);
    int setFormat(std::string_view s);
    int setKeys(std::string_view s);
    int setPrecision(std::string_view s);
    void help() override;
    void interactive();

    std::string query_;
    OutputFormat format_;
    int precision_;
    std::string_view keys_;
};

