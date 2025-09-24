// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TesArchive.h"

using namespace clarisma;

void TesArchive::open(const char* fileName)
{
    MappedFile::open(fileName, OpenMode::READ);
    fileSize_ = size();
    data_ = reinterpret_cast<uint8_t*>(map(0, fileSize_, MappingMode::READ));
}

void TesArchive::close()
{
    if(data_)
    {
        unmap(data_, fileSize_);
        data_ = nullptr;
        fileSize_ = 0;
    }
    MappedFile::close();
}


std::unique_ptr<uint64_t[]> TesArchive::computeOffsets() const
{
    uint32_t count = header().entryCount;
    std::unique_ptr<uint64_t[]> offsets(new uint64_t[count]);
    uint64_t currentOfs = sizeof(TesArchiveHeader) + sizeof(TesArchiveEntry) * count;
    for(int i=0; i<count; i++)
    {
        const TesArchiveEntry& entry = (*this)[i];
        offsets[i] = currentOfs;
        currentOfs += entry.size;
    }
    assert(currentOfs == fileSize_);
    return offsets;
}