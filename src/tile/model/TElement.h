// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cassert>
#include <cstdint>
#include <clarisma/cli/Console.h>
#include <clarisma/data/Linked.h>
#include <clarisma/data/Lookup.h>

// TODO: In order to save 8 bytes per element, we use next_ (from Linked) to 
// chain items in the same bucket in an ElementDeduplicator
// However, we also use next_ for the chain of placed elements; this means
// that once we start placing elements, we can no longer look up elements in
// the ElementDeduplicator, because the hashmap chains are now invalid
// This should not be a problem, but need to document
// When placing elements, cannot assume that next_ is null!

using namespace clarisma;

class TileModel;
template <typename T> class ElementDeduplicator;

/**
	TElement                             24 bytes
	├── TDataElement                     32 bytes
	│	├── TReferencedElement           40 bytes 
	│	│   ├── TFeature                 56 bytes  (could reduce to 48)
	│	│   │   ├── TNode				   
	│	│   │   └── TFeature2D
	│	│   │       ├── TWay
	│	│   │       └── TRelation
	│	│   └── TSharedElement           48 bytes 
	│	│       ├── TString
	│	│       ├── TTagTable
    │	│       └── TRelationTable
	│	└── TFeatureBody                 32 bytes
	│		├── TWayBody
	│       └── TRelationBody
	├── TIndex
	├── TIndexBranch
	│   ├── TIndexLeaf
	│   └── TIndexTrunk
	└── TExportTable

 */
class TElement : public Linked<TElement>
{
public:
	using Handle = int32_t;		
		// handle has to be signed, or else pointer calculation 
		// may introduce bugs (pointer deltas are signed)

	enum class Alignment : uint8_t
	{
		BYTE, WORD, DWORD, QWORD
	};

	enum class Type : unsigned int
	{
		UNKNOWN,
		STRING,
		TAGS,
		RELTABLE,
		NODE,
		FEATURE2D,
		WAY_BODY,
		RELATION_BODY,
		INDEX,
		TRUNK,
		LEAF,
		HEADER,
		EXPORTS
	};

	enum Flags
	{
		LAST        = (1 << 0),
		DELETED     = (1 << 1),
		ORIGINAL    = (1 << 2),
		NEEDS_FIXUP = (1 << 3),
		BUILT       = (1 << 4),
		WAY_AREA_TAGS   = (1 << 5),
		RELATION_AREA_TAGS   = (1 << 6),
	};

	TElement(Type type, Handle handle, uint32_t size, Alignment alignment, int anchor = 0) :
		Linked(nullptr),
		location_(0), 
		size_(size),
		alignment_(static_cast<unsigned int>(alignment)),
		handle_(handle),
		type_(type),
		flags_(0),
		anchor_(anchor)
	{
	}

	template<typename T>
	static T* cast(TElement* e)
	{
		T* casted = static_cast<T*>(e);
		if (e != nullptr && e->type() != T::TYPE)
		{
			Console::debug("Expected type %d but got %d\n", T::TYPE, e->type());
			Console::debug("  Handle = %d\n", e->handle());
			assert(false);
		}
		// assert(casted==nullptr || casted->type() == T::TYPE);
		return casted;
	}

	template<typename T>
	const T* cast() const
	{
		const T* casted = static_cast<const T*>(this);
#ifdef _DEBUG
		if (this != nullptr && type() != T::TYPE)
		{
			printf("Expected type %d but got %d\n", T::TYPE, type());
			assert(false);
		}
#endif
		// assert(casted==nullptr || casted->type() == T::TYPE);
		return casted;
	}

	Type type() const { return type_; }
	TElement* next() const { return next_; }
	void setNext(TElement* next) { next_ = next; }
	int32_t location() const { return location_; }
	int32_t target() const { return location_ + anchor_; }
	void setLocation(int32_t location) { location_ = location; }
	Handle handle() const { return handle_; }
	void setHandle(Handle h) { handle_ = h; }
	uint32_t size() const { return size_; }
	void setAlignment(Alignment alignment) 
	{ 
		alignment_ = static_cast<unsigned int>(alignment); 
	}
	int32_t alignedLocation(int32_t loc) const
	{
		int add = (1 << alignment_) - 1;
		int32_t mask = 0xffff'ffff << alignment_;
		return (loc + add) & mask;
	}
	uint32_t anchor() const { return anchor_; }
	uint32_t flags() const { return flags_; }
	bool isLast() const { return flags() & Flags::LAST; }
	void markLast() { flags_ |= Flags::LAST; }
	void setAnchor(uint32_t anchor) { anchor_ = anchor; }
	bool isOriginal() const { return flags() & Flags::ORIGINAL; }
	bool needsFixup() const { return flags() & Flags::NEEDS_FIXUP; }
	bool isBuilt() const { return flags() & Flags::BUILT; }
	void setFlag(uint32_t flag, bool b) { flags_ = (flags_ & ~flag) | (b ? flag : 0); }
	void setOriginal(bool b) { setFlag(Flags::ORIGINAL, b); }
	void setNeedsFixup(bool b) { setFlag(Flags::NEEDS_FIXUP, b); }
	void setSize(size_t size) { size_ = size; }

	static bool compareByHandle(TElement* a, TElement* b)
	{
		return a->handle() < b->handle();
	}

private:
	int32_t location_;
	unsigned int alignment_ :  2;
	unsigned int size_      : 30;
	Handle handle_;
		// TODO: move handle to TReferencedElement?
		// No, need it here because of alignment
		// If we were to take it out, this class only has 3 32-bit values,
		// which would leave a gap
	Type type_               :  4;
	unsigned int flags_      :  8;
	unsigned int anchor_     : 20;
		// Pre-anchor area is limited to 1 MB
};

// TODO: Changing bool to int gives expected alignment on MSVC

