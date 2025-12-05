// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "Installer.h"
#include <clarisma/cli/Console.h>
#include <clarisma/net/HttpClient.h>
#include "ProductParser.h"

using namespace clarisma;

void Installer::upgrade()
{
    Console::get()->start("Checking for updates...");
    HttpClient client("https://www.geodesk.com");
    std::vector<std::byte> data;
    client.get("/downloads/gol.json", data);
    data[data.size()-1] = static_cast<std::byte>(0); // TODO
    ProductParser parser(reinterpret_cast<const char*>(data.data()));
    parser.parse(PLATFORM);

}
