// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <functional>
#include <memory>
#include <clarisma/data/HashMap.h>
#include <clarisma/util/DataPtr.h>
#include <clarisma/util/ShortVarString.h>
#include <geodesk/feature/GlobalStrings.h>
#include "AbstractTagsParser.h"
#include "TagTableModel.h"



class StringCatalog;
class TagTableModel;

namespace clarisma {
class BufferWriter;
}

using namespace clarisma;
using namespace geodesk;


class AreaClassifier
{
public:
	struct Entry
	{
		uint16_t code;
		unsigned int flags : 4;
		unsigned int isKey : 1;
		std::string_view string;

		explicit Entry(std::string_view s) :
			code(0), flags(0), isKey(false), string(s)
		{
		}

		bool operator<(const Entry& other) const
		{
			if (code == other.code)
			{
				// local strings sorted in reverse order
				return other.string < string;
			}
			return code < other.code;
		}
	};

	class Parser : public AbstractTagsParser
	{
	public:
		explicit Parser(const char *s) : AbstractTagsParser(s) {}
		std::vector<Entry> parseRules();
	};

    using GlobalStringLookupFunctor = std::function<int(std::string_view)>;

	AreaClassifier(std::vector<Entry>& entries, const GlobalStringLookupFunctor& lookup);

    static const char DEFAULT[];

    static constexpr int AREA_FOR_WAY = 1;
    static constexpr int AREA_FOR_RELATION = 2;

    int isArea(const TagTableModel& tags) const;
	// void dump(BufferWriter& out, const StringCatalog& strings) const;

private:
	// Rules are laid out like this:
  	// uint16 0
  	// "Banana"
	// uint16 length of Banana
	// "Apple"
	// uint16 length of Apple
  	// uint16 GlobalString 1	<-- pointer points here
  	// uint16 GlobalString 2
    // uint16 0xffff



	class RulePtr
	{
	public:
        enum
		{
            REJECT_SOME = 1,
			ACCEPT_ALL = 2,
			DEFINITE_FOR_WAY = 4,
            DEFINITE_FOR_RELATION = 8,
		};

		RulePtr(const std::byte* p, const std::byte* pBase, int flags) :
			data_((static_cast<uint32_t>(p - pBase) << 4) |
				static_cast<uint32_t>(flags)) {}

		int flags() const { return static_cast<int>(data_) & 0xf; }
		const std::byte* ptr(const std::byte* pBase) const
		{
			return pBase + (data_ >> 4);
		}

	private:
		uint32_t data_;
	};

	bool isArea(RulePtr rule, TagTableModel::Tag tag) const
	{
		if(tag.valueType() == TagValueType::GLOBAL_STRING) [[likely]]
		{
			return isArea(rule, tag.value());
		}
		if(tag.valueType() == TagValueType::LOCAL_STRING)
		{
			return isArea(rule, tag.stringValue());
		}
		return false;
	}

	bool isArea(RulePtr rule, uint_fast32_t value) const
	{
		if(value == GlobalStrings::NO) return false;
		if(rule.flags() & RulePtr::ACCEPT_ALL) return true;
		const uint16_t* p = reinterpret_cast<const uint16_t*>(rule.ptr(rules_.get()));
		for (;;)
		{
			if (*p >= value)
			{
				return (value == *p) ^ (rule.flags() & RulePtr::REJECT_SOME);
			}
			p++;
		}
	}

	bool isArea(RulePtr rule, std::string_view value) const
	{
		if(rule.flags() & RulePtr::ACCEPT_ALL) return true;
		const char* valueData = value.data();
		uint32_t valueLen = value.length();
		DataPtr p = rule.ptr(rules_.get()) - 2;

		for (;;)
		{
			uint16_t candidateLen = p.getUnsignedShort();
			if (candidateLen == 0)
			{
				// reached end of the list -> string not found
				return rule.flags() & RulePtr::REJECT_SOME;
			}
			p -= candidateLen;
			if (candidateLen == valueLen)
			{
				if (memcmp(p, valueData, valueLen) == 0)
				{
					// Matched string
					return (rule.flags() & RulePtr::REJECT_SOME) == 0;
				}
			}
			p -= 2 + (candidateLen & 1);  // skip padding byte for odd-length strings
		}
	}

	static uint32_t determineLastDefiniteGlobalKey(const GlobalStringLookupFunctor& lookup);
	void dumpRule(BufferWriter& out, RulePtr rule, const StringCatalog& strings) const;

	std::unique_ptr<const std::byte[]> rules_;
	HashMap<uint_fast32_t, RulePtr> globalKeyRules_;
	HashMap<std::string_view, RulePtr> localKeyRules_;
	uint32_t lastDefiniteGlobalKey_;

	friend class Parser;
};

