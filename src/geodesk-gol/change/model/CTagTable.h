// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <span>
#include <geodesk/feature/TagTablePtr.h>
#include <clarisma/data/HashSet.h>
#include <clarisma/util/log.h>
#include "tag/TagTableModel.h"

namespace geodesk {
class AbstractTagIterator;
}

using namespace geodesk;

class ChangeModel;

class CTagTable
{
public:
    class Tag
    {
    public:
        constexpr Tag() : data_(1) {}     // empty tag (key 0, type string, value 0)
        Tag(uint32_t key, TagValueType type, uint32_t value) :
            data_(static_cast<uint64_t>(value) << 32 | (key << 2) | type) {}

        uint32_t key() const noexcept { return static_cast<uint32_t>(data_) >> 2; }
        uint32_t value() const noexcept { return static_cast<uint32_t>(data_ >> 32); }
        TagValueType type() const noexcept { return static_cast<TagValueType>(data_ & 3); }
        size_t hash() const noexcept { return data_; }
        explicit operator uint64_t() const noexcept { return data_; }

        bool operator==(const Tag& other) const noexcept
        {
            return data_ == other.data_;
        }

    private:
        uint64_t data_;
    };

    struct StorageSize
    {
        uint32_t totalSize;
        uint32_t localTagsSize;
    };

    constexpr CTagTable() :
        tagCount_(1),
        localTagCount_(0),
        flags_(AREA_TAGS_CLASSIFIED),
        hash_(0)
    {
        // sole tag is initialized as empty tag
    }

    CTagTable(const TagTableModel& tagModel, ChangeModel& changeModel);

    bool operator==(const CTagTable& other) const noexcept
    {
        if(hash_ != other.hash_) return false;
        if(tagCount_ != other.tagCount_) return false;
        if(localTagCount_ != other.localTagCount_) return false;
        return memcmp(tags_, other.tags_, tagCount_ * sizeof(Tag)) == 0;
    }

    bool equals(const ChangeModel& model, int32_t handle, TagTablePtr pTags) const noexcept;
    size_t hash() const noexcept;

    static size_t size(size_t tagCount) noexcept
    {
        return sizeof(CTagTable) + (tagCount-1) * sizeof(Tag);
    }

    unsigned localTagCount() const { return localTagCount_; }
    // Tag* tags() { return tags_; }
    std::span<Tag> tags() noexcept { return {tags_, tagCount_}; }
    std::span<const Tag> tags() const noexcept { return {tags_, tagCount_}; }
    std::span<const Tag> localTags() const noexcept { return {tags_, localTagCount_}; }
    std::span<const Tag> globalTags() const noexcept
    {
        return {&tags_[localTagCount_], tagCount_ - localTagCount_};
    }
    bool areaTagsClassified() const { return flags_ & AREA_TAGS_CLASSIFIED; }
    bool isArea(bool forRelation) const
    {
        assert(areaTagsClassified());
        return flags_ & (forRelation ? RELATION_AREA_TAGS : WAY_AREA_TAGS);
    }
    void setAreaFlags(int areaFlags)
    {
        flags_ = areaFlags | AREA_TAGS_CLASSIFIED;
    }

    StorageSize calculateStorageSize() const;

    struct PtrHash
    {
        size_t operator()(const CTagTable* table) const noexcept
        {
            return table->hash_;
        }
    };

    struct PtrEqual
    {
        bool operator()(const CTagTable* lhs, const CTagTable* rhs) const noexcept
        {
            return *lhs == *rhs; // Compare contents of CTagTable
        }
    };

    static constexpr int WAY_AREA_TAGS = 1;
    static constexpr int RELATION_AREA_TAGS = 2;
    static constexpr int AREA_TAGS_CLASSIFIED = 4;

    static const CTagTable EMPTY;

private:
    static uint32_t getTagValue(ChangeModel& changeModel, const TagTableModel::Tag& tag);

    // TODO: If we can limit tagcounts to 16K, we could reduce the
    //  object size by 8 bytes (can squeeze counts and flags into 4 bytes,
    //  plus 4 bytes for hash)
    uint32_t tagCount_;
    uint32_t localTagCount_;
    uint32_t flags_;
    uint32_t hash_;
    Tag tags_[1];
};


using CTagTableSet = clarisma::HashSet<CTagTable*, CTagTable::PtrHash, CTagTable::PtrEqual>;