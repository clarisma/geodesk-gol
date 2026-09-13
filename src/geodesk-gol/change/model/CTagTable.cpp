// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "CTagTable.h"

#include <clarisma/cli/Console.h>
#include <clarisma/util/Hash.h>
#include <clarisma/util/log.h>
#include <geodesk/feature/GlobalTagIterator.h>
#include <geodesk/feature/LocalTagIterator.h>
#include "ChangeModel.h"

using namespace clarisma;

const CTagTable CTagTable::EMPTY;

CTagTable::CTagTable(const TagTableModel& tagModel, ChangeModel& changeModel) :
    tagCount_(static_cast<uint32_t>(tagModel.tags().size())),
    localTagCount_(static_cast<unsigned int>(tagModel.localTags().size())),
    flags_(0),
    hash_(0)
{
    assert(tagCount_ > 0);
    assert(tagCount_ > localTagCount_);
        // Even if tagtable only has local tags, there must be
        // an "empty" global tag
    Tag* pTag = tags_;
    for(TagTableModel::Tag localTag: tagModel.localTags())
    {
        *pTag++ = Tag(
            changeModel.getLocalString(localTag.localKey()),
            localTag.valueType(),
            getTagValue(changeModel, localTag));
    }
    for(TagTableModel::Tag globalTag: tagModel.globalTags())
    {
        *pTag++ = Tag(globalTag.globalKey(),
            globalTag.valueType(), getTagValue(changeModel, globalTag));
    }

    for(int i=0; i<tagCount_; i++)
    {
        hash_ = Hash::combine(hash_, static_cast<uint64_t>(tags_[i]));
    }
}

uint32_t CTagTable::getTagValue(ChangeModel& changeModel, const TagTableModel::Tag& tag)
{
    if(tag.valueType() == TagValueType::LOCAL_STRING)
    {
        return changeModel.getLocalString(tag.stringValue());
    }
    return tag.value();
}

bool CTagTable::equals(const ChangeModel& model, int32_t handle, TagTablePtr pTags) const noexcept
{
    LocalTagIterator iterLocal(handle, pTags);
    for(int i=0; i<localTagCount_; i++)
    {
        Tag tag = tags_[i];
        if(!iterLocal.next()) return false;
        if(*model.getString(tag.key()) != *iterLocal.keyString())
        {
            // We're using * to compare string contents, not pointers
            return false;
        }
        if(tag.type() != iterLocal.valueType()) return false;
        if(tag.type() == TagValueType::LOCAL_STRING)
        {
            if(*model.getString(tag.value()) != *iterLocal.localStringValue())
            {
                // We're using * to compare string contents, not pointers
                return false;
            }
        }
        else
        {
            if(tag.value() != iterLocal.value()) return false;
        }
    }
    if(iterLocal.next()) return false;

    GlobalTagIterator iterGlobal(handle, pTags);
    for(int i=localTagCount_; i<tagCount_; i++)
    {
        Tag tag = tags_[i];
        if(!iterGlobal.next()) return false;
        if(tag.type() != iterGlobal.valueType()) return false;
        if(tag.type() == TagValueType::LOCAL_STRING)
        {
            if(*model.getString(tag.value()) != *iterGlobal.localStringValue())
            {
                // We're using * to compare string contents, not pointers
                return false;
            }
        }
        else
        {
            if(tag.value() != iterGlobal.value()) return false;
        }
    }
    return !iterGlobal.next();
}


CTagTable::StorageSize CTagTable::calculateStorageSize() const
{
    uint32_t totalSize = 0;
    uint32_t localTagsSize = 0;

    if(tagCount_ <= localTagCount_)
    {
        LOGS << "Invalid tag counts: total=" << tagCount_ << ", local=" << localTagCount_;
    }

    assert(tagCount_ > localTagCount_);

    for(int i=0; i<tagCount_; i++)
    {
        uint32_t valueSize = 2 + (static_cast<uint32_t>(tags_[i].type()) & 2);
        totalSize += 2 + valueSize;
        if(i < localTagCount_)
        {
            totalSize += 2;
            localTagsSize += 4 + valueSize;
        }
    }
    return { totalSize, localTagsSize };
}