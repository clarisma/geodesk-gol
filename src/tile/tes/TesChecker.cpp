// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TesChecker.h"

#include <clarisma/cli/Console.h>
#include <clarisma/util/log.h>
#include <clarisma/util/varint.h>
#include <geodesk/feature/TagTablePtr.h>
#include <geodesk/feature/Tex.h>
#include <geodesk/feature/types.h>
#include <geodesk/geom/LonLat.h>

#include "TesFlags.h"

using namespace clarisma;
using namespace geodesk;

/*
bool TesChecker::check()
{
    if(p_ == end_)
    {
        fatal(0, "Empty buffer");
        return false;
    }
    read();
    if(!errors().empty())
    {
        for(const Error& error : errors())
        {
            Console::log("%08X  %s", static_cast<uint32_t>(error.location()), error.message().c_str());
        }
    }
    return errors().empty();
}
*/

void TesChecker::dump(const std::filesystem::path& root)
{
    char subFolderName[16];
    char fileName[16];
    Format::hexUpper(subFolderName, tip() >> 12, 3);
    Format::hexUpper(fileName, tip(), 3);
    strcpy(&fileName[3], ".txt");
    // file.open((folder_ / subFolderName / fileName).toString().c_str(),
    //	File::CREATE | File::REPLACE_EXISTING | File::WRITE);
    std::string filePath = (root / subFolderName / fileName).string();
    FILE* file = fopen(filePath.c_str(), "wb");
    FileBuffer buf(file, 64 * 1024);
    out_.setBuffer(&buf);
    read();
    dumpErrors();
    out_.flush();
    fclose(file);
}

void TesChecker::read()
{
    tileBounds_ = tile_.bounds();
    prevXY_ = tileBounds_.bottomLeft();
    LOGS << "Checking " << tip_ << " (" << tile_ << ")";
    readFeatureIndex();
    readStrings();
    readTagTables();
    readRelationTables();
    readChangedFeatures();
    readRemovedFeatures();
    readExports();
}

void TesChecker::readFeatureIndex()
{
    size_t count = readVarint32();
    features_.reserve(count);
    out_ << "FEATURES:\n";
    int typeGroup = 0;
    uint64_t id = 0;
    int typeCount = 0;
    // LOGS << "Reading " << count << " features...";
    while(features_.size() < count)
    {
        uint64_t taggedDelta = readVarint64();
        if(taggedDelta == 0)
        {
            featureCounts_[typeGroup] = typeCount;
            typeGroup++;
            typeCount = 0;
            id = 0;
            continue;
        }
        id += taggedDelta >> 1;
        FeatureType type = static_cast<FeatureType>(typeGroup);
        out_ << "  FEATURE #" << features_.size() << ": " << typeName(type)
            << '/' << id << std::string_view(taggedDelta & 1 ? " *\n" : "\n");
        features_.emplace_back(TypedFeatureId::ofTypeAndId(type, id),
            (taggedDelta & 1) ? start() : nullptr);
            // If feature is changed, we temporarily set its data pointer
            // to the start of the TES file -- readChangedFeatures() will
            // fill in the true pointer later. If only referenced, we
            // set the pointer to null
        changedFeatureCount_ += static_cast<int>(taggedDelta) & 1;
        typeCount++;
    }
    featureCounts_[typeGroup] = typeCount;
    out_ << featureCounts_[0] << " nodes, "
        << featureCounts_[1] << " ways, "
        << featureCounts_[2] << " relations\n";
    out_ << "\n";
}

void TesChecker::readStrings()
{
    uint32_t count = readVarint32();
    out_ << "STRINGS:\n";
    // LOGS << "Reading " << count << " strings...";
    strings_.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const ShortVarString* str = readString();
        out_ << "  STRING #" << i << ": \""
            << (str ? str->toStringView() : "(invalid)") << "\"\n";
        strings_.emplace_back(str);
    }
    out_ << "\n";
}


void TesChecker::readTagTables()
{
    uint32_t count = readVarint32();
    if(count==0) return;
    out_ << "SHARED_TAGS:\n";
    // LOGS << "Reading " << count << " tag tables...";
    tagTables_.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        tagTables_.emplace_back(readTagTable(i));
    }
    out_ << '\n';
}

void TesChecker::readRelationTables()
{
    uint32_t count = readVarint32();
    if(count==0) return;
    out_ << "SHARED_RELATIONS:\n";
    // LOGS << "Reading " << count << " relation tables...";
    relationTables_.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        relationTables_.emplace_back(readRelationTable(i));
    }
    out_ << '\n';
}

const uint8_t* TesChecker::readTagTable(int number)
{
    const uint8_t* tags = p_;
    mark();
    uint32_t taggedSize = readVarint32();
    uint32_t size = taggedSize & 0xffff'fffe;
    uint32_t computedSize = 0;
    if (size < 4)
    {
        error(tags, "Invalid tag-table size: %d", size);
        return tags;
    }
    if(number >= 0)
    {
        out_ << "  TAGS #" << number;
    }
    else
    {
        out_ << "  TAGS";
    }
    out_ << " (" << size << " bytes):\n";

    if (taggedSize & 1) // has local keys
    {
        uint32_t localTagsSize = readVarint32() << 1;
        if (localTagsSize > size - 4)
        {
            error("Size of locals (%d) too large for table size %d",
                localTagsSize, size);
        }
        while(computedSize < localTagsSize)
        {
            computedSize += readLocalTag();
        }
        if(computedSize > localTagsSize)
        {
            error(tags, "Local tags size should be %d, not %d",
                computedSize, localTagsSize);
        }
    }
    uint32_t prevGlobalTag = 0;
    while(computedSize < size)
    {
        computedSize += readGlobalTag(prevGlobalTag);
    }
    if(computedSize > size)
    {
        error(tags, "Tags size should be %d, not %d",
            computedSize, size);
    }
    return tags;
}


uint32_t TesChecker::readGlobalTag(uint32_t prevGlobalTag)
{
    mark();
    uint32_t keyAndFlags = readVarint32();
    TagValueType type = static_cast<TagValueType>(keyAndFlags & 3);
    uint32_t globalTag = prevGlobalTag + (keyAndFlags >> 2);
    checkRange("global-tag key", globalTag, FeatureConstants::MAX_COMMON_KEY+1);
    // TODO: check global string range
    out_ << "    #" << globalTag;
    uint32_t value = readTagValue(type);
    return 4 + (static_cast<uint32_t>(type) & 2);
}


uint32_t TesChecker::readLocalTag()
{
    mark();
    uint32_t keyAndFlags = readVarint32();
    TagValueType type = static_cast<TagValueType>(keyAndFlags & 3);
    checkLocalString("local key", keyAndFlags >> 2);
    out_ << "    ";
    writeLocalString(keyAndFlags >> 2);
    uint32_t value = readTagValue(type);
    return 6 + (static_cast<uint32_t>(type) & 2);
}


void TesChecker::checkLocalString(const char* type, uint32_t code)
{
    checkRange(type, code, strings_.size());
}

uint32_t TesChecker::readTagValue(TagValueType type)
{
    out_ << '=';
    mark();
    uint32_t value = readVarint32();
    if(type == TagValueType::LOCAL_STRING)
    {
        checkLocalString("tag value", value);
        writeLocalString(value);
        out_ << '\n';
    }
    else
    {
        // TODO: check global string range
        out_ << value << '\n';
    }
    return value;
}

const uint8_t* TesChecker::readRelationTable(int number)
{
    const uint8_t* relTable = p_;
    uint32_t size = readVarint32();
    readRelationTableContents(number, size);
    return relTable;
}

void TesChecker::readRelationTableContents(int number, uint32_t size)
{
    if(number >= 0)
    {
        if(size == 0)
        {
            error("Size of shared reltable must not be 0");
        }
        out_ << "  RELATIONS #" << number;
    }
    else if(size == 0)
    {
        out_ << "  NO RELATIONS";
        return;
    }
    else
    {
        out_ << "  RELATIONS";
    }
    out_ << " (" << size << " bytes):\n";

    uint32_t computedSize = 0;
    bool foreign = false;
    Tip tip = FeatureConstants::START_TIP;
    Tex tex = Tex::RELATIONS_START_TEX;
    while(computedSize < size)
    {
        uint32_t rel = readVarint32();
        bool differentTile = rel & 1;
        foreign |= differentTile;
        if(foreign)
        {
            TexDelta texDelta(fromZigzag(rel >> 1));
            tex += texDelta;
            computedSize += texDelta.isWide(Tex::RELATIONS_TEX_BITS) ? 4 : 2;
            if(differentTile)
            {
                TipDelta tipDelta = fromZigzag(readVarint32());
                computedSize += tipDelta.isWide() ? 4 : 2;
                tip += tipDelta;
            }
            writeForeignFeatureRef(tip, tex);
        }
        else
        {
            /*
            LOGS << "Raw rel ref = " << rel << ", nodeCount=" << featureCounts_[0]
                << ", wayCount=" << featureCounts_[1];
            */
            uint32_t local = (rel >> 1) + featureCounts_[0] + featureCounts_[1];
            checkRange("relation", local, features_.size());
            computedSize += 4;
            writeLocalFeatureRef(local);
        }
    }

    if(computedSize > size)
    {
        error("Relation table size should be %d, not %d",
            computedSize, size);
    }
}

int TesChecker::readFeatureStub()
{
    int flags = *p_++;
    if (flags & TesFlags::TAGS_CHANGED)
    {
        if (flags & TesFlags::SHARED_TAGS)
        {
            mark();
            uint32_t tagTableNumber = readVarint32();
            checkRange("tag table", tagTableNumber, tagTables_.size());
            out_ << "  TAGS #" << tagTableNumber << '\n';
        }
        else
        {
            readTagTable(-1);   // private tag table
        }
    }

    if (flags & TesFlags::RELATIONS_CHANGED)
    {
        mark();
        uint32_t relsSizeOrRef = readVarint32();
        if (relsSizeOrRef != 0)	// 0 means feature no longer has a reltable
        {
            if (relsSizeOrRef & 1)
            {
                checkRange("relation table", relsSizeOrRef >> 1,
                    relationTables_.size());
                out_ << "    RELATIONS #" << (relsSizeOrRef >> 1) << '\n';
            }
            else
            {
                readRelationTableContents(-1, relsSizeOrRef);
                // No need to shift, size is always a multiple of 2
                // and Bit 0 is cleared to signal that this is a private table
            }
        }
        else
        {
            // TODO: only valid if this is a delta TES
        }
    }
    return flags;

}


void TesChecker::readNode()
{
    int flags = readFeatureStub();
    // TODO: flag check
    if(flags & TesFlags::GEOMETRY_CHANGED)
    {
        prevXY_.x += fromZigzag(readVarint64());
        prevXY_.y += fromZigzag(readVarint64());
        out_ << "  LONLAT: " << LonLat(prevXY_) << '\n';
    }
}

void TesChecker::readWay()
{
    int flags = readFeatureStub();
    // TODO: flag check
    if(flags & TesFlags::GEOMETRY_CHANGED)
    {
        mark();
        uint32_t nodeCount = readVarint32();
        if(nodeCount < 2)
        {
            error("Invalid node count (%d)", nodeCount);
        }
        out_ << "  NODES (" << nodeCount << "):\n";
        Coordinate xy = prevXY_;
        Box bounds;
        for(int i=0; i<nodeCount; ++i)
        {
            xy.x += fromZigzag(readVarint32());
            xy.y +=fromZigzag(readVarint32());
            coords_.emplace_back(xy);
            bounds.expandToInclude(xy);
        }
        if (nodeCount > 0) prevXY_ = coords_[0];

        uint64_t nodeId = 0;
        if(flags & TesFlags::NODE_IDS_CHANGED)
        {
            for(int i=0; i<nodeCount; ++i)
            {
                nodeId += fromZigzag(readVarint64());
                out_ << "    node/" << nodeId << ": " << LonLat(coords_[i]) << '\n';
            }
        }
        else
        {
            for(int i=0; i<nodeCount; ++i)
            {
                out_ << "    " << LonLat(coords_[i]) << '\n';
            }
        }
        coords_.clear();
    }
    else
    {
        if(flags & TesFlags::NODE_IDS_CHANGED)
        {
            error("Flagged node_ids_changed, but not geometry_changed");
        }
    }

    if(flags & TesFlags::MEMBERS_CHANGED)
    {
        uint32_t nodeTableSize = readVarint32();
        if(nodeTableSize == 0)
        {
            out_ << "  NO MEMBERS\n";
        }
        else
        {
            out_ << "  MEMBERS (" << nodeTableSize << " bytes):\n";
            uint32_t computedSize = 0;
            Tip tip = FeatureConstants::START_TIP;
            Tex tex = Tex::WAYNODES_START_TEX;
            while(computedSize < nodeTableSize)
            {
                uint32_t nodeRef = readVarint32();
                if(nodeRef & 1)
                {
                    // foreign node
                    TexDelta texDelta(fromZigzag(nodeRef >> 2));
                    tex += texDelta;
                    computedSize += texDelta.isWide(Tex::WAYNODES_TEX_BITS) ? 4 : 2;
                    if(nodeRef & 2)
                    {
                        // different tile
                        TipDelta tipDelta(fromZigzag(readVarint32()));
                        tip += tipDelta;
                        computedSize += tipDelta.isWide() ? 4 : 2;
                    }
                    writeForeignFeatureRef(tip, tex);
                }
                else
                {
                    // local node
                    checkRange("local-node ref", nodeRef >> 1,
                        featureCounts_[0]);
                    writeLocalFeatureRef(nodeRef >> 1);
                    computedSize += 4;
                }
            }
        }
    }
}



void TesChecker::readRelation()
{
    int flags = readFeatureStub();
    // TODO: flag check
    if(flags & TesFlags::BBOX_CHANGED)
    {
        int64_t minX = static_cast<int64_t>(prevXY_.x) + fromZigzag(readVarint64());
        int64_t minY = static_cast<int64_t>(prevXY_.y) + fromZigzag(readVarint64());
        int64_t maxX = minX + readVarint64();
        int64_t maxY = minY + readVarint64();
        Box bounds(minX, minY, maxX, maxY);
        out_ << "  BOUNDS: " << LonLat(bounds.bottomLeft()) << " -> " << LonLat(bounds.topRight()) << '\n';
        prevXY_ = bounds.bottomLeft();
    }
    if(flags & TesFlags::MEMBERS_CHANGED)
    {
        uint32_t size = readVarint32();
        out_ << "  MEMBERS (" << size << " bytes):\n";
        Tip tip = FeatureConstants::START_TIP;
        Tex tex = Tex::MEMBERS_START_TEX;
        uint32_t computedSize = 0;
        while(computedSize < size)
        {
            uint32_t member = readVarint32();
            if(member & 1)
            {
                // foreign member
                TexDelta texDelta(fromZigzag(member >> 3));
                tex += texDelta;
                computedSize += texDelta.isWide(Tex::MEMBERS_TEX_BITS) ? 4 : 2;
                if(member & 4)
                {
                    // different tile
                    TipDelta tipDelta(fromZigzag(readVarint32()));
                    tip += tipDelta;
                    computedSize += tipDelta.isWide() ? 4 : 2;
                }
                writeForeignFeatureRef(tip, tex);
            }
            else
            {
                checkRange("member", member >> 2, features_.size());
                writeLocalFeatureRef(member >> 2);
                computedSize += 4;
            }
            if(member & 2)
            {
                // different role
                uint32_t role = readVarint32();
                if(role & 1)
                {
                    // TODO: global string -- check range
                    computedSize += 2;
                }
                else
                {
                    checkLocalString("role", role >> 1);
                    computedSize += 4;
                }
            }
        }

        if(computedSize > size)
        {
            error("Member table size should be %d, not %d",
                computedSize, size);
        }
    }
}

void TesChecker::readChangedFeatures()
{
    for(Feature& f : features_)
    {
        if(!f.data) continue;       // not changed, only referenced

        out_ << "CHANGED " << f.typedId << ":\n";
        const uint8_t* featureData = p_;

        switch(f.typedId.type())
        {
        case FeatureType::NODE:
            readNode();
            break;
        case FeatureType::WAY:
            readWay();
            break;
        case FeatureType::RELATION:
            readRelation();
            break;
        }
        f.data = featureData;
        out_ << '\n';
    }
}

void TesChecker::readRemovedFeatures()
{
    uint32_t count = readVarint32();
    // LOGS << "Reading " << count << " removed features...";
    if(count == 0) return;
    out_ << "REMOVED:\n";

    int typeGroup = 0;
    uint64_t id = 0;
    while(count)
    {
        uint64_t ref = readVarint64();
        if (ref == 0)
        {
            typeGroup++;
            id = 0;
            continue;
        }
        id += ref >> 1;
        int deletedFlag = ref & 1;
        out_ << "  " << TypedFeatureId::ofTypeAndId(
            static_cast<FeatureType>(typeGroup), id)
            << (deletedFlag ? " DELETED\n" : "\n");
        count--;
    }
}

void TesChecker::readExports()
{
    uint32_t count = readVarint32();
    // TODO
}


void TesChecker::writeLocalString(uint32_t code)
{
    if(code >= strings_.size())
    {
        out_ << "(invalid)";
    }
    else
    {
        out_ << '\"' << *strings_[code] << '\"';
    }
}


void TesChecker::writeLocalFeatureRef(uint32_t local)
{
    out_ << "    Local  #" << local << ": ";
    if(local < features_.size())
    {
        out_ << features_[local].typedId << '\n';
    }
    else
    {
        out_ << "invalid\n";
    }
}

void TesChecker::writeForeignFeatureRef(Tip tip, Tex tex)
{
    out_ << "    " << tip << " #" << static_cast<int>(tex) << '\n';
}

void TesChecker::dumpErrors()
{
    const std::vector<Error>& errorList = errors();
    if (errorList.empty()) return;

    for (const Error& error : errorList)
    {
        out_ << error.location() << ": " << error.message() << '\n';
    }
}