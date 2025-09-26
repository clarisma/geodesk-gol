// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "GolChecker.h"

#include <clarisma/util/Crc32C.h>
#include <clarisma/util/DataPtr.h>
#include <geodesk/feature/FeatureStore.h>

#include "TileChecker.h"

void GolChecker::processTile(Tip tip, Tile tile)
{
    // LOGS << "Checking " << tip;
    TilePtr pTile = store().fetchTile(tip);
    if (pTile)
    {
        Crc32C checksum;
        uint32_t payloadSize = pTile.payloadSize();
        checksum.update(pTile.ptr(), payloadSize);
        if (checksum.get() != pTile.checksum())
        {
            ConsoleWriter out;
            out.blank() << tip << ": Invalid checksum";
        }
#ifdef GOL_DIAGNOSTICS
        if (Console::verbosity() >= Console::Verbosity::DEBUG)
        {
            TileChecker checker(tip, tile, TilePtr(pTile.ptr()));
            checker.check();
        }
#endif
    }
    postOutput(tip, ByteBlock());
}
