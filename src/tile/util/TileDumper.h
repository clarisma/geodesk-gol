// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <clarisma/util/StreamWriter.h>
#include <geodesk/feature/Tex.h>
#include <geodesk/feature/TilePtr.h>
#include "tile/model/TileModel.h"

class TElement;
class TTagTable;
class TString;
class TRelationTable;
class TFeature;
class TWayBody;
class TRelationBody;
class TIndex;
class TIndexTrunk;
class TIndexLeaf;

class TileDumper
{
public:
	TileDumper(Buffer* buf, FeatureStore* store);

	void dump(Tile tile, TilePtr pTile);

private:
	void dump(TElement* e);
    void dumpTags(TTagTable* tagTable);
    void dumpString(TString* str);
    void dumpRelationTable(TRelationTable* rels);
    void dumpFeature(TFeature* feature);
    void dumpWayBody(TWayBody* wayBody);
    void dumpRelationBody(TRelationBody* relationBody);
    void dumpIndex(TIndex* index);
    void dumpIndexTrunk(TIndexTrunk* trunk);
    void dumpIndexLeaf(TIndexLeaf* leaf);
	void dumpExports(TExportTable* exports);
    void dumpGap(int location, int len);

    StreamWriter& startElement(int location, const char* s);
    StreamWriter& startElement(TElement* e, const char* s);
    void printForeignFeatureRef(Tip tip, Tex tex);
    void printLocalFeatureRef(int handle);
    void printRole(int roleCode, const TString* role);
    void users(TSharedElement* e);

	StreamWriter out_;
	TileModel tile_;
	FeatureStore* store_;
};
