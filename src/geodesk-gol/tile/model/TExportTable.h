// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "TElement.h"
#include <geodesk/feature/TypedFeatureId.h>

using namespace geodesk;

class TFeature;

class TExportTable : public TElement
{
public:
	TExportTable(TFeature** features, TypedFeatureId* typedIds, size_t count) :
		TElement(Type::EXPORTS, 0,
			static_cast<uint32_t>(count * 4 + 4),
			Alignment::DWORD, 4),
		features_(features),
		typedIds_(typedIds)
	{
		assert(count);
	}

	size_t count() const { return (size() / 4) - 1; }
	TFeature** features() const { return features_; }
	void write(const TileModel& tile) const;

	static constexpr Type TYPE = Type::EXPORTS;

private:
	TFeature** features_;
	TypedFeatureId* typedIds_;
};
