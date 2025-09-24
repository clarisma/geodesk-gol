// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TileDumper.h"
#include <clarisma/text/Format.h>
#include <geodesk/feature/FeatureStore.h>
#include <geodesk/feature/MemberTableIterator.h>
#include <geodesk/feature/NodeTableIterator.h>
#include <geodesk/feature/RelationTableIterator.h>
#include <geodesk/feature/Tags.h>
#include <geodesk/feature/FeatureBase.h>

#include "tile/model/TileReader.h"
#include "tile/model/TExportTable.h"
#include "tile/model/TIndexLeaf.h"
#include "tile/model/TIndexTrunk.h"
#include "tile/model/TNode.h"
#include "tile/model/TWay.h"
#include "tile/model/TRelation.h"
#include "tile/model/TTagTable.h"
#include "tile/model/TRelationTable.h"
#include "tile/model/TString.h"



TileDumper::TileDumper(Buffer* buf, FeatureStore* store) :
	out_(buf),
    store_(store)
{
}


void TileDumper::dumpTags(TTagTable* tagTable)
{
    startElement(tagTable, "TAGS");
    users(tagTable);
    Tags tags(store_, tagTable->tags());
    for(Tag tag: tags)
    {
        out_ << "            " << tag.key() << "=" << tag.value() << '\n';
    }
}

void TileDumper::dumpString(TString* str)
{
    startElement(str, "STRING ");
    out_.writeByte('\"');
    const ShortVarString* s = str->string();
    out_.writeJsonEscapedString(s->data(), s->length());
    out_.writeByte('\"');
    users(str);
}

void TileDumper::dumpRelationTable(TRelationTable* rels)
{
    startElement(rels, "RELATIONS");
    users(rels);
    RelationTableIterator iter(rels->handle(), rels->data());
    Tip tip = FeatureConstants::START_TIP;
    Tex tex(Tex::RELATIONS_START_TEX);
    while (iter.next())
    {
        if (iter.isForeign())
        {
            if (iter.isInDifferentTile())
            {
                tip += iter.tipDelta();
            }
            tex += iter.texDelta();
            printForeignFeatureRef(tip, tex);
        }
        else
        {
            printLocalFeatureRef(iter.localHandle());
        }
        out_.writeByte('\n');
    }
}

void TileDumper::dumpFeature(TFeature* f)
{
    Feature feature(store_, f->feature());
    startElement(f, "FEATURE ")
        << feature.toString() << "  " << feature.label() << '\n';
}

void TileDumper::dumpWayBody(TWayBody* body)
{
    startElement(body, "BODY of way/");
    FeaturePtr f = body->feature()->feature();
    out_.formatInt(f.id());
    out_.writeConstString(" (");
    out_.formatInt(body->nodeCount());
    out_.writeConstString(" nodes)\n");

    DataPtr nodeTable = body->nodeTable();
    if (nodeTable)
    {
        NodeTableIterator iter(body->handle() - 
            (body->feature()->feature().flags() & FeatureFlags::RELATION_MEMBER), nodeTable);
        Tip tip = FeatureConstants::START_TIP;
        Tex tex(Tex::WAYNODES_START_TEX);
        while (iter.next())
        {
            if (iter.isForeign())
            {
                if (iter.isInDifferentTile())
                {
                    tip += iter.tipDelta();
                }
                tex += iter.texDelta();
                printForeignFeatureRef(tip, tex);
            }
            else
            {
                printLocalFeatureRef(iter.localHandle());
            }
            out_.writeByte('\n');
        }
    }
}

void TileDumper::dumpRelationBody(TRelationBody* body)
{
    startElement(body, "BODY of relation/");
    FeaturePtr f = body->feature()->feature();
    out_.formatInt(f.id());
    out_.writeByte('\n');
    MemberTableIterator iter(body->handle(), body->data());
    Tip tip = FeatureConstants::START_TIP;
    Tex tex(Tex::MEMBERS_START_TEX);
    int roleCode = 0;
    TString* role = nullptr;
    while (iter.next())
    {
        if (iter.isForeign())
        {
            if (iter.isInDifferentTile())
            {
                tip += iter.tipDelta();
            }
            tex += iter.texDelta();
            printForeignFeatureRef(tip, tex);
        }
        else
        {
            printLocalFeatureRef(iter.localHandle());
        }
        if (iter.hasDifferentRole())
        {
            if (iter.hasLocalRole())
            {
                role = tile_.getString(iter.localRoleHandleFast());
                roleCode = -1;
            }
            else
            {
                role = nullptr;
                roleCode = iter.globalRoleFast();
            }
        }
        printRole(roleCode, role);
    }
}

void TileDumper::dumpExports(TExportTable* exports)
{
    startElement(exports, "EXPORTS (");
    size_t count = exports->count();
    out_ << count << " features)\n";
    TFeature** features = exports->features();
    for(int i=0; i<count; i++)
    {
        out_ << "          #" << i << ": ";
        printLocalFeatureRef(features[i]->handle());
        out_.writeByte('\n');
    }
}

void TileDumper::printLocalFeatureRef(int handle)
{
    const TFeature* f = static_cast<const TFeature*>(
        tile_.getElement(handle));

    out_.writeConstString("          ");
    if (f)
    {
        out_.writeString(f->feature().toString());
    }
    else
    {
        out_ << "(illegal reference to ";
        char buf[32];
        Format::hexUpper(buf, handle, 8);
        out_ << buf;
        out_ << ")";
    }
}

void TileDumper::printForeignFeatureRef(Tip tip, Tex tex)
{
    char buf[32];
    out_.writeConstString("          ");
    tip.format(buf);
    out_.writeString(buf);
    out_.writeByte(' ');
    out_.writeByte('#');
    out_.formatInt(static_cast<int>(tex));
}

void TileDumper::printRole(int roleCode, const TString* role)
{
    if (roleCode >= 0)
    {
        assert(role == nullptr);
        if(roleCode)
        {
            out_ << " as " << *store_->strings().getGlobalString(roleCode);
        }
    }
    else
    {
        assert(role);
        out_ << " as \"" << *role->string() << '\"';
    }
    out_.writeByte('\n');
}


void TileDumper::dumpIndex(TIndex* index)
{
    std::cout << "Dumping index..." << std::endl;
}

void TileDumper::dumpIndexTrunk(TIndexTrunk* trunk)
{
    std::cout << "Dumping trunk..." << std::endl;
}

void TileDumper::dumpIndexLeaf(TIndexLeaf* leaf)
{
    std::cout << "Dumping leaf..." << std::endl;
}

void TileDumper::dump(TElement* e)
{
    switch (e->type())
    {
    case TElement::Type::TAGS:
        dumpTags(static_cast<TTagTable*>(e));
        break;
    case TElement::Type::STRING:
        dumpString(static_cast<TString*>(e));
        break;
    case TElement::Type::RELTABLE:
        dumpRelationTable(static_cast<TRelationTable*>(e));
        break;
    case TElement::Type::NODE:
        dumpFeature(static_cast<TFeature*>(e));
        break;
    case TElement::Type::FEATURE2D:
        dumpFeature(static_cast<TFeature*>(e));
        break;
    case TElement::Type::WAY_BODY:
        dumpWayBody(static_cast<TWayBody*>(e));
        break;
    case TElement::Type::RELATION_BODY:
        dumpRelationBody(static_cast<TRelationBody*>(e));
        break;
    case TElement::Type::INDEX:
        dumpIndex(static_cast<TIndex*>(e));
        break;
    case TElement::Type::TRUNK:
        dumpIndexTrunk(static_cast<TIndexTrunk*>(e));
        break;
    case TElement::Type::LEAF:
        dumpIndexLeaf(static_cast<TIndexLeaf*>(e));
        break;
    case TElement::Type::EXPORTS:
        dumpExports(static_cast<TExportTable*>(e));
        break;
    default:
        std::cout << "Unknown type" << std::endl;
        break;
    }
}


void TileDumper::dumpGap(int location, int len)
{
    startElement(location, "=== ");
    out_.formatInt(len);
    out_.writeConstString(" byte");
    if (len > 1) out_.writeByte('s');
    out_.writeByte('\n');
}

StreamWriter& TileDumper::startElement(int location, const char* s)
{
    char buf[16];
    Format::hexUpper(buf, location, 8);
    out_.writeBytes(buf, 8);
    out_.writeConstString("  ");
    out_.writeString(s);
    return out_;
}


StreamWriter& TileDumper::startElement(TElement* e, const char* s)
{
    startElement(e->location(), s);
    return out_;
}

void TileDumper::users(TSharedElement* e)
{
    int users = e->users();
    if (users != 1)
    {
        out_.writeByte(' ');
        out_.writeByte('(');
        out_.formatInt(users);
        out_.writeByte(')');
    }
    out_.writeByte('\n');
}


void TileDumper::dump(Tile tile, TilePtr pTile)
{
	TileReader reader(tile_);
	reader.readTile(tile, pTile);

    std::vector<TElement*> elements;
    std::vector<TReferencedElement*> referencedElements = tile_.getElements();
    for (TElement* e : referencedElements)
    {
        e->setLocation(e->handle() - e->anchor());
        elements.push_back(e);
        if (e->type() == TElement::Type::FEATURE2D)
        {
            TFeature2D* feature = static_cast<TFeature2D*>(e);
            TFeatureBody* body = feature->body();
            body->setLocation(body->handle() - body->anchor());
            elements.push_back(body);
        }
    }

    TExportTable* exports = tile_.exportTable();
    if(exports) elements.push_back(exports);
	std::sort(elements.begin(), elements.end(), TElement::compareByHandle);
    int pos = 0;
    for (TElement* e : elements)
    {
        // assert(e->location() >= pos);
        if (e->location() < pos)
        {
            out_.writeConstString("=== OVERLAP ===\n");
        }
        if (e->location() > pos)
        {
            dumpGap(pos, e->location() - pos);
        }
        dump(e);
        pos = e->location() + e->size();
    }
    out_.flush();
}


