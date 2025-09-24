// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: LGPL-3.0-only

#include "FeaturePileLookup.h"
#include <clarisma/io/FilePath.h>
#include <clarisma/util/Bits.h>
#include <clarisma/util/Strings.h>

using namespace clarisma;

bool FeaturePileLookup::open(std::string_view golPath, int tileCount)
{
    int bits = 32 - Bits::countLeadingZerosInNonZero32(tileCount);
        // The above is correct; if we have 512 tiles, we need to store 513
        // distinct values (pile number starts at 1, 0 = "missing")
        // Hence, it's not enough to have 9 bits, but we will need 10
        // 0x200 (decimal 512) has 22 leading zeroes --> 10 bits
    std::string_view folderPrefix = FilePath::withoutExtension(golPath);
    std::string path = Strings::combine(folderPrefix, "-indexes/nodes.idx");
    if (!File::exists(path.c_str())) return false;
    indexes_[0].open(path.c_str(), File::OpenMode::READ | File::OpenMode::WRITE, bits);
    path = Strings::combine(folderPrefix, "-indexes/ways.idx");
    indexes_[1].open(path.c_str(), File::OpenMode::READ | File::OpenMode::WRITE, bits + 2);
    path = Strings::combine(folderPrefix, "-indexes/relations.idx");
    indexes_[2].open(path.c_str(), File::OpenMode::READ | File::OpenMode::WRITE, bits + 2);
    return true;
}
