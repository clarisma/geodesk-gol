// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "TReferencedElement.h"
#include <geodesk/feature/FeaturePtr.h>
#include <geodesk/feature/NodePtr.h>
#include <geodesk/feature/WayPtr.h>
#include <geodesk/feature/RelationPtr.h>

class Layout;
class Membership;
class MutableFeaturePtr;
class TTagTable;
class TRelationTable;
class TileModel;

using namespace geodesk;

class TFeature : public TReferencedElement
{
public:
	TFeature(Type type, Handle handle, uint32_t size, FeaturePtr feature, int anchor) :
		TReferencedElement(type, handle, feature, size, Alignment::DWORD, anchor),
		nextById_(nullptr), firstMembership_(nullptr)
	{
	}

	FeaturePtr feature() const { return FeaturePtr(data_); }
	uint64_t id() const { return feature().id(); }
	int typeCode() const { return feature().typeCode(); }
	FeatureType featureType() const { return feature().type(); }
	TypedFeatureId typedId() const { return feature().typedId(); }
	uint64_t idBits() const { return feature().idBits(); }
	int flags() const { return feature().flags(); }
	bool isRelationMember() const { return flags() & FeatureFlags::RELATION_MEMBER; }
	TTagTable* tags(const TileModel& tile) const;
	TRelationTable* parentRelations(TileModel& tile) const;
	TFeature* nextFeature() const
	{
		assert(next_ == nullptr || 
			next_->type() == Type::NODE ||
			next_->type() == Type::FEATURE2D);
		return reinterpret_cast<TFeature*>(next_);
	}

	static TFeature* cast(TElement* e)
	{
		assert(e == nullptr || e->type() == Type::NODE || e->type() == Type::FEATURE2D);
		return static_cast<TFeature*>(e);	// NOLINT: cast is safe
	}

	void placeRelationTable(Layout& layout);

	static bool compareById(const TFeature* a, const TFeature* b)
	{
		return a->id() < b->id();
	}

	MutableFeaturePtr makeMutable(TileModel& tile);

	/// Returns a pointer to the prepended extra data of type T
	/// (Allocated by createFeature)
	/*
	template <typename T>
	T* extraData()
	{
		return reinterpret_cast<T*>(reinterpret_cast<uint8_t>(this) - sizeof(T));
	}
	*/

	Membership* firstMembership() const { return firstMembership_; }
	void addMembership(Membership* membership);

protected:
	void write(const TileModel& tile) const;

	TFeature* nextById_;
	Membership* firstMembership_;

	friend class FeatureTable;
};



// TODO: Use nextById_ for lookup; next_ is used for forming chains or features
//  Could use next_ if we're careful about iterating, and if we ensure that ID
//  lookup is no longer needed once we start adding features to the indexes
class FeatureTable : public Lookup<FeatureTable, TFeature>
{
public:
	static uint64_t getId(TFeature* element)
	{
		return element->feature().idBits();
	}

	static TFeature** next(TFeature* elem)
	{
		return reinterpret_cast<TFeature**>(&elem->nextById_);
	}
};

