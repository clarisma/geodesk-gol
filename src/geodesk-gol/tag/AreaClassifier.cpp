// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "AreaClassifier.h"

#include <clarisma/cli/ConsoleWriter.h>

#include "build/util/StringCatalog.h"
#include <clarisma/util/MutableDataPtr.h>

const char AreaClassifier::DEFAULT[] =
	"aeroway (except taxiway), "
	"amenity, "
	"area, "
	"area:highway, "
	"barrier (city_wall, ditch, hedge, retaining_wall, wall, spikes), "
	"boundary, "
	"building, "
	"building:part, "
	"craft, "
	"golf, "
	"highway (services, rest_area, escape, elevator), "
	"historic, "
	"indoor, "
	"natural (except coastline, cliff, ridge, arete, tree_row), "
	"landuse, "
	"leisure, "
	"man_made (except cutline, embankment, pipeline), "
	"military, "
	"office, "
	"place, "
	"power (plant, substation, generator, transformer), "
	"public_transport, "
	"railway (station, turntable, roundhouse, platform), "
	"ruins, "
	"shop, "
	"tourism, "
	"type (multipolygon, boundary), "
	"waterway (riverbank, dock, boatyard, dam)";

uint32_t AreaClassifier::determineLastDefiniteGlobalKey(const GlobalStringLookupFunctor& lookup)
{
	uint32_t area = static_cast<uint32_t>(lookup("area"));
	uint32_t type = static_cast<uint32_t>(lookup("type"));
	return std::max(area, type);
}

AreaClassifier::AreaClassifier(std::vector<Entry>& entries, const GlobalStringLookupFunctor& lookup) :
	lastDefiniteGlobalKey_(determineLastDefiniteGlobalKey(lookup))
{
	size_t ruleTableSize = 0;
	Entry* pEntry = entries.data();
	const Entry* end = entries.data() + entries.size();
	while (pEntry < end)
	{
		Entry* pKey = pEntry++;
		assert(pKey->isKey);
		if ((pKey->flags & RulePtr::ACCEPT_ALL) == 0)
		{
			assert(pEntry < end);
			ruleTableSize += 4;		// end markers for global and local values (2 bytes each)
			assert(!pEntry->isKey);
			for(;;)
			{
				int code = lookup(pEntry->string);
				if (code >= 0)
				{
                  	pEntry->code = code;
					ruleTableSize += 2;		// 2-byte global string code
				}
				else
				{
					ruleTableSize += 2;		// string size
					ruleTableSize += pEntry->string.size();	// the string chars
					ruleTableSize += ruleTableSize & 1;
						// if string has an odd number of chars, add an extra byte
						// of padding so table entries are always 2-byte aligned
				}
				pEntry++;
				if(pEntry == end) break;
				if (pEntry->isKey) break;	// we've reached the next key
			}
			if(pKey + 1 < pEntry)
			{
				std::sort(pKey + 1, pEntry);
				// sort the values for the current keys in tag-table order
				// local keys first (reverse alphabetically), then global keys
				// (in ascending order of code)
			}
		}
	}

	// Now we can build the rules table
	MutableDataPtr p = new std::byte[ruleTableSize];
	rules_.reset(p.bytePtr());

	pEntry = entries.data();
	while (pEntry < end)
	{
		Entry* pKey = pEntry++;
		assert(pKey->isKey);
		const std::byte* pRule = nullptr;
		if ((pKey->flags & RulePtr::ACCEPT_ALL) == 0)
		{
			assert(pEntry < end);
			p.putUnsignedShort(0);	// end of local values
			p += 2;
			for (;;)
			{
				assert(!pEntry->isKey);
				if (pEntry->code)
				{
					if (pRule == nullptr) pRule = p.bytePtr();
					p.putUnsignedShort(pEntry->code);
					p += 2;
				}
				else
				{
					size_t stringSize = pEntry->string.size();
					p += (stringSize & 1); // padding for odd-length strings
					p.putBytes(pEntry->string.data(), stringSize);
					p += stringSize;
					p.putUnsignedShort(static_cast<uint16_t>(stringSize));
					p += 2;
				}
				pEntry++;
				if (pEntry == end) break;
				if (pEntry->isKey) break;	// we've reached the next key
			}
			if (pRule == nullptr) pRule = p.bytePtr();
			p.putUnsignedShort(0xffff);	// end of global values
			p += 2;
		}
		int code = lookup(pKey->string);
		if (code >= 0 && code <= TagValues::MAX_COMMON_KEY)
		{
			globalKeyRules_.insert({static_cast<uint_fast32_t>(code),
				RulePtr(pRule, rules_.get(), pKey->flags)});
		}
		else
		{
			localKeyRules_.insert({pKey->string,
				RulePtr(pRule, rules_.get(), pKey->flags)});
		}
	}
	assert(p.bytePtr() == rules_.get() + ruleTableSize);

	/*
	if(Console::verbosity() >= Console::Verbosity::DEBUG)
	{
		ConsoleWriter out;
		out.timestamp();
		dump(out, strings);
	}
	*/
}


std::vector<AreaClassifier::Entry> AreaClassifier::Parser::parseRules()
{
	std::vector<Entry> entries;

	for (;;)
	{
		std::string_view key = expectKey();
		size_t currentKey = entries.size();
		// Don't store a pointer, because it may become invalid when entries_ grows
		entries.emplace_back(key);
		entries[currentKey].isKey = true;
		if(key == "area")
		{
			entries[currentKey].flags |= RulePtr::DEFINITE_FOR_WAY;
		}
		else if (key == "type")
		{
			entries[currentKey].flags |= RulePtr::DEFINITE_FOR_RELATION;
		}
		if (accept('('))
		{
			int rawValueCount = 0;
			for (;;)
			{
				std::string_view value = identifier(VALID_NEXT_CHAR, VALID_NEXT_CHAR);
				if (value.empty())
				{
					error(rawValueCount == 0 ? "Expected tag value or \"except\"" :
						"Expected tag value");		// throws
				}
				rawValueCount++;
				skipWhitespace();
				if (value == "except")
				{
					entries[currentKey].flags = RulePtr::REJECT_SOME;
					continue;
				}
				entries.emplace_back(value);
				if (!accept(',')) break;
			}
			expect(')');
		}
		else
		{
			entries[currentKey].flags |= RulePtr::ACCEPT_ALL;
		}
		if (!accept(',')) break;
	}
	return entries;
}


int AreaClassifier::isArea(const TagTableModel& tags) const
{
	bool isGeneralArea = false;
	bool isDefiniteWayArea = false;
	bool isDefiniteRelationArea = false;
	bool seenDefiniteWayTag = false;
	bool seenDefiniteRelationTag = false;

	for(auto tag : tags.globalTags())
	{
		auto it = globalKeyRules_.find(tag.globalKey());
		if (it != globalKeyRules_.end())
		{
			RulePtr rule = it->second;
			bool isAreaTag = isArea(rule, tag);
			bool isDefiniteWayAreaTag = (rule.flags() & RulePtr::DEFINITE_FOR_WAY);
			seenDefiniteWayTag |= isDefiniteWayAreaTag;
			isDefiniteWayArea |= isDefiniteWayAreaTag & isAreaTag;
			bool isDefiniteRelAreaTag = (rule.flags() & RulePtr::DEFINITE_FOR_RELATION);
			seenDefiniteRelationTag |= isDefiniteRelAreaTag;
			isDefiniteRelationArea |= isDefiniteRelAreaTag & isAreaTag;
			isGeneralArea |= isAreaTag;
		}
		/*
		 // TODO: Can potentially quit early
		if(tag.globalKey() >= lastDefiniteGlobalKey_ && isGeneralArea)
		{
			return (seenDefiniteWayTag ? isDefiniteWayArea : isGeneralArea) ?
		}
		*/
	}

	for(auto tag : tags.localTags())
	{
		auto it = localKeyRules_.find(tag.localKey());
		if (it != localKeyRules_.end())
		{
			RulePtr rule = it->second;
			bool isAreaTag = isArea(rule, tag);
			bool isDefiniteWayAreaTag = (rule.flags() & RulePtr::DEFINITE_FOR_WAY);
			seenDefiniteWayTag |= isDefiniteWayAreaTag;
			isDefiniteWayArea |= isDefiniteWayAreaTag & isAreaTag;
			bool isDefiniteRelAreaTag = (rule.flags() & RulePtr::DEFINITE_FOR_RELATION);
			seenDefiniteRelationTag |= isDefiniteRelAreaTag;
			isDefiniteRelationArea |= isDefiniteRelAreaTag & isAreaTag;
			isGeneralArea |= isAreaTag;
		}
	}

	return ((seenDefiniteWayTag ? isDefiniteWayArea : isGeneralArea) ? AREA_FOR_WAY : 0) |
		((seenDefiniteRelationTag ? isDefiniteRelationArea : isGeneralArea) ? AREA_FOR_RELATION : 0);
}

/*
void AreaClassifier::dump(BufferWriter& out, const StringCatalog& strings) const
{
	out.writeString("Area rules: ");
	bool isFirst = true;
	for (const auto& [key, rule] : globalKeyRules_)
	{
		if(!isFirst)	out.writeString(", ");
		out.writeString(strings.getGlobalString(key)->toStringView());
		if((rule.flags() & RulePtr::ACCEPT_ALL) == 0)
		{
			dumpRule(out, rule, strings);
		}
		isFirst = false;
	}
	for (const auto& [key, rule] : localKeyRules_)
	{
		if(!isFirst)	out.writeString(", ");
		out.writeString(key);
		if((rule.flags() & RulePtr::ACCEPT_ALL) == 0)
		{
			dumpRule(out, rule, strings);
		}
		isFirst = false;
	}
	out.writeString("\n");
}

void AreaClassifier::dumpRule(BufferWriter& out, RulePtr rule, const StringCatalog& strings) const
{
	const uint16_t* p = reinterpret_cast<const uint16_t*>(rule.ptr(rules_.get()));
	out.writeString((rule.flags() & RulePtr::REJECT_SOME) ? "(except " : "(");
	bool isFirst = true;
	for (;;)
	{
		uint16_t value = *p++;
		if (value == 0xffff) break;
		if(!isFirst) out.writeString(", ");
		out.writeString(strings.getGlobalString(value)->toStringView());
		isFirst = false;
	}

	DataPtr p2 = rule.ptr(rules_.get()) - 2;
	for (;;)
	{
		uint16_t len = p2.getUnsignedShort();
		if (len == 0) break;
		p2 -= len;
		if(!isFirst) out.writeString(", ");
		out.writeString(std::string_view(reinterpret_cast<const char*>(p2.ptr()), len));
		isFirst = false;
		p2 -= 2 + (len & 1);  // skip padding byte for odd-length strings
	}
	out.writeString(")");
}
*/