// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#ifdef CLARISMA_WITH_ZLIB
#pragma once
#include <cstdint>

namespace clarisma {

class ZipArchive
{
public:
    #pragma pack(push, 1)

    ///
    /// @brief End Of Central Directory record (no ZIP64).
    /// @details Fixed 22-byte structure at end of file.
    ///
    struct Trailer
    {
        /// @brief Must be 0x06054b50.
        uint32_t signature;

        uint16_t diskNumber;
        uint16_t centralDirDisk;
        uint16_t entriesOnThisDisk;
        uint16_t totalEntries;
        uint32_t centralDirSize;
        uint32_t centralDirOffset;
        uint16_t commentLen;
    };

    static_assert(sizeof(Trailer) == 22, "EOCD size must be 22 bytes.");

    ///
    /// @brief Local file header.
    /// @details Fixed 30-byte header followed by name and extra.
    ///
    struct LocalFileHeader
    {
        /// @brief Must be 0x04034b50.
        uint32_t signature;

        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t method;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
    };

    static_assert(sizeof(LocalFileHeader) == 30,
        "LocalFileHeader size must be 30 bytes.");

    ///
    /// @brief Central directory file header.
    /// @details Fixed 46-byte header followed by name, extra, comment.
    ///
    struct CentralDirHeader
    {
        /// @brief Must be 0x02014b50.
        uint32_t signature;
        uint16_t versionMadeBy;
        uint16_t versionNeeded;
        uint16_t flags;
        uint16_t method;
        uint16_t modTime;
        uint16_t modDate;
        uint32_t crc32;
        uint32_t compressedSize;
        uint32_t uncompressedSize;
        uint16_t fileNameLen;
        uint16_t extraLen;
        uint16_t commentLen;
        uint16_t diskNumberStart;
        uint16_t internalAttrs;
        uint32_t externalAttrs;
        uint32_t localHeaderOffset;
    };

    static_assert(sizeof(CentralDirHeader) == 46,
        "CentralDirHeader size must be 46 bytes.");
#pragma pack(pop)

    /// @brief ZIP header signatures.
    enum : uint32_t
    {
        MAGIC_LOCAL_FILE_HEADER = 0x04034b50u,
        MAGIC_CENTRAL_DIR       = 0x02014b50u,
        MAGIC_EOCD              = 0x06054b50u
    };
};

} // namespace clarisma

#endif