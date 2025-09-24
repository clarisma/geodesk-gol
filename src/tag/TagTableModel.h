// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <span>
#include <vector>
#include <clarisma/util/ShortVarString.h>
#include <geodesk/feature/TagTablePtr.h>
#include <geodesk/feature/TagValues.h>

using namespace clarisma;
using namespace geodesk;

class TagTableModel
{
public:
	class Tag
	{
	public:
		Tag(uint32_t k, uint32_t globalStringValue) :
			keyStringLength_(0),
			valueStringLength_(0),
			valueType_(static_cast<uint16_t>(TagValueType::GLOBAL_STRING)),
			hasLocalKey_(false),
			globalKey_(k),
			value_(globalStringValue)
		{
		}

		Tag(uint32_t k, TagValueType type, uint32_t value) :
			keyStringLength_(0),
			valueStringLength_(0),
			valueType_(type),
			hasLocalKey_(false),
			globalKey_(k),
			value_(value)
		{
		}


		Tag(uint32_t k, std::string_view v) :	// NOLINT fields initialized in body
			keyStringLength_(0),
			hasLocalKey_(false),
			globalKey_(k)
		{
			setValue(v);
		}

		Tag(std::string_view k, uint32_t globalStringValue) :
			keyStringLength_(static_cast<uint16_t>(k.size())),
			valueStringLength_(0),
			valueType_(static_cast<uint16_t>(TagValueType::GLOBAL_STRING)),
			hasLocalKey_(true),
			localKey_(k.data()),
			value_(globalStringValue)
		{
		}

		Tag(std::string_view k, TagValueType type, uint32_t value) :
			keyStringLength_(static_cast<uint16_t>(k.size())),
			valueStringLength_(0),
			valueType_(type),
			hasLocalKey_(true),
			localKey_(k.data()),
			value_(value)
		{
		}

		Tag(std::string_view k, std::string_view v) :	// NOLINT fields initialized in body
			keyStringLength_(static_cast<uint16_t>(k.size())),
			hasLocalKey_(true),
			localKey_(k.data())
		{
			setValue(v);
		}

		uint32_t globalKey() const
		{
			assert (!hasLocalKey_);
			return globalKey_;
		}

		std::string_view localKey() const
		{
			assert (hasLocalKey_);
			return { localKey_, keyStringLength_ };
		}

		TagValueType valueType() const noexcept
		{
			return static_cast<TagValueType>(valueType_);
		}

		uint32_t value() const noexcept
		{
			assert(valueType() != TagValueType::LOCAL_STRING);
			return value_;
		}

		std::string_view stringValue() const noexcept
		{
			assert(valueType() == TagValueType::LOCAL_STRING);
			return { stringValue_, valueStringLength_ };
		}

	private:
		void setValue(std::string_view v)
		{
			Decimal num(v, true);
			if (TagValues::isNumericValue(num))
			{
				if (TagValues::isNarrowNumericValue(num))
				{
					value_ = TagValues::narrowNumber(num);
					valueType_ = TagValueType::NARROW_NUMBER;
				}
				else
				{
					value_ = TagValues::wideNumber(num);
					valueType_ = TagValueType::WIDE_NUMBER;
				}
				valueStringLength_ = 0;
			}
			else
			{
				valueStringLength_ = static_cast<uint16_t>(v.size());
				stringValue_ = v.data();
				valueType_ = TagValueType::LOCAL_STRING;
			}
		}

		uint16_t keyStringLength_;
		uint16_t valueStringLength_;
		uint16_t valueType_;
		bool hasLocalKey_;

		union
		{
			uint32_t globalKey_;
			const char* localKey_;
		};
		union
		{
			uint32_t value_;
			const char* stringValue_;
		};
	};

	static_assert(sizeof(Tag) == 24);

	TagTableModel() :
		globalTagsSize_(0),
		localTagsSize_(0),
		localTagsCount_(0) {}

	void addGlobalTag(uint32_t k, uint32_t v)
	{
		tags_.emplace_back(k, v);
		globalTagsSize_ += 4;
	}

	void addGlobalTag(uint32_t k, TagValueType type, uint32_t v)
	{
		tags_.emplace_back(k, type, v);
		globalTagsSize_ += 4;
	}

	void addGlobalTag(uint32_t k, std::string_view v)
	{
		Tag tag(k,v);
		tags_.emplace_back(tag);
		globalTagsSize_ += 4 + (tag.valueType() & 2);
	}

	void addLocalTag(std::string_view k, uint32_t v)
	{
		addLocalTag(Tag(k,v));
		localTagsSize_ += 6;
	}

	void addLocalTag(std::string_view k, TagValueType type, uint32_t v)
	{
		addLocalTag(Tag(k,type,v));
		localTagsSize_ += 6;
	}

	void addLocalTag(std::string_view k, std::string_view v)
	{
		Tag tag(k,v);
		addLocalTag(tag);
		localTagsSize_ += 6 + (tag.valueType() & 2);
	}

	bool isEmpty() const { return tags_.empty(); }

	std::span<const Tag> tags() const
	{
		return { tags_ };
	}

	std::span<const Tag> globalTags() const
	{
		return { tags_.begin() + localTagsCount_, tags_.end() };
	}

	std::span<Tag> globalTags()
	{
		return { tags_.begin() + localTagsCount_, tags_.end() };
	}

	std::span<const Tag> localTags() const
	{
		return { tags_.begin(), tags_.begin() + localTagsCount_ };
	}

	std::span<Tag> localTags()
	{
		return { tags_.begin(), tags_.begin() + localTagsCount_ };
	}

	bool hasGlobalTags() const { return tags_.size() - localTagsCount_; }
	bool hasLocalTags() const { return localTagsCount_; }

	uint32_t globalTagsSize() const { return globalTagsSize_; }
	uint32_t localTagsSize() const { return localTagsSize_; }

	void read(TagTablePtr p);

	void normalize()
	{
		std::ranges::sort(localTags(), compareLocal);
		std::ranges::sort(globalTags(), compareGlobal);
		if(!hasGlobalTags()) addGlobalTag(0,0);
		// TODO: Review gol-spec#3 on value of empty tag
	}

	void clear()
	{
		tags_.clear();
		globalTagsSize_ = 0;
		localTagsSize_ = 0;
		localTagsCount_ = 0;
	}

protected:
	void addLocalTag(const Tag& tag)
	{
		int prevCount = tags_.size();
		int prevLocalTagsCount = localTagsCount_;
		localTagsCount_++;
		tags_.emplace_back(tag);
		int globalTagsCount = prevCount - prevLocalTagsCount;
		if(globalTagsCount > 0)
		{
			std::swap(tags_[prevLocalTagsCount], tags_[prevCount]);
		}
	}

	static bool compareGlobal(const Tag& a, const Tag& b)
	{
		return a.globalKey() < b.globalKey();
	}

	static bool compareLocal(const Tag& a, const Tag& b)
	{
		return a.localKey() < b.localKey();
	}

	std::vector<Tag> tags_;
	int globalTagsSize_;
	int localTagsSize_;
	int localTagsCount_;
};
