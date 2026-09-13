// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "TagTableBuilder.h"
#include "build/util/ProtoGol.h"
#include <geodesk/feature/TagValues.h>
#include <tag/AreaClassifier.h>

#include "tile/compiler/TagTableWriter.h"
#include "tile/model/TileModel.h"

//// TODO: BAD !!!!!!!!!!!!!!
///   Must allocate all strings *before* building the tagtable
///   Root problem: strings are created speculatively and then rolled back
///   We cannot rollback once we start creating the tagtable
///  Fixed now -- if strings were created, creation of the the tag-table
///  is never rolled back (the tag-table by definition cannot be a duplicate)
///  We only create strigns now if they don't already exist in the model

TTagTable* TagTableBuilder::getTagTable(ByteSpan protoTags, bool determineIfArea)
{
	assert(tags_.empty());
	assert(globalTagsSize_ == 0);
	assert(localTagsSize_ == 0);

	const uint8_t* p = protoTags.data();
	while (p < protoTags.end())
	{
		std::pair<int, std::string_view> key = ProtoGol::readKeyString(p, strings_);
		std::pair<int, std::string_view> val = ProtoGol::readValueString(p, strings_);
		if(key.first >= 0)
		{
			if(val.first >= 0)
			{
				addGlobalTag(key.first, val.first);
			}
			else
			{
				addGlobalTag(key.first, val.second);
			}
		}
		else
		{
			if(val.first >= 0)
			{
				addLocalTag(key.second, val.first);
			}
			else
			{
				addLocalTag(key.second, val.second);
			}
		}
	}
	return getTagTable(determineIfArea);
}


TTagTable* TagTableBuilder::getTagTable(bool determineIfArea)
{
	normalize();
	TTagTable* tags = tile_.beginTagTable(globalTagsSize_ + localTagsSize_, localTagsSize_);
	TagTableWriter writer(tags->handle(), tags->data());
	bool needsFixup = hasLocalTags();
	
	for (Tag& tag : localTags())
	{
		TString* key = tile_.addString(tag.localKey());
		key->setAlignment(TElement::Alignment::DWORD);
			// strings used as local keys must be 4-byte aligned

		if (tag.valueType() == TagValueType::LOCAL_STRING)
		{
			writer.writeLocalTag(key, tile_.addString(tag.stringValue()));
		}
		else
		{
			writer.writeLocalTag(tag.valueType(), key, tag.value());
		}
	}
	assert(writer.ptr() == tags->data() - localTagsSize_);
	writer.endLocalTags();

	for (Tag& tag : globalTags())
	{
		if (tag.valueType() == TagValueType::LOCAL_STRING)
		{
			writer.writeGlobalTag(tag.globalKey(), tile_.addString(tag.stringValue()));
			needsFixup = true;
		}
		else
		{
			writer.writeGlobalTag(tag.valueType(),
				tag.globalKey(), tag.value());
		}
	}
	assert(writer.ptr() == tags->data() + globalTagsSize_);
	writer.endGlobalTags();

	tags = tile_.completeTagTable(tags, static_cast<uint32_t>(writer.hash()), needsFixup);
	if(determineIfArea)
	{
		if(!tags->isBuilt())
		{
			int areaType = areaClassifier_.isArea(*this);
			tags->setFlag(TTagTable::Flags::WAY_AREA_TAGS,
				(areaType & AreaClassifier::AREA_FOR_WAY) != 0);
			tags->setFlag(TTagTable::Flags::RELATION_AREA_TAGS,
				(areaType & AreaClassifier::AREA_FOR_RELATION) != 0);
			tags->setFlag(TTagTable::Flags::BUILT, true);
		}
	}

	clear();

	return tags;
}





