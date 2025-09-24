// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "TIndex.h"

class FeatureTable;
class IndexSettings;
class Layout;
class TExportTable;

// TODO: Change name

class THeader : public TElement
{
public:
	explicit THeader(const IndexSettings& settings) :
		TElement(Type::HEADER, 0, 24, Alignment::DWORD),
		settings_(settings)
	{
	}

	void addFeatures(TileModel& tile);
	void setExportTable(TExportTable* exportTable) { exportTable_ = exportTable; }
	void build(TileModel& tile);
	void place(Layout& layout);
	void write(const TileModel& tile) const;

	static constexpr Type TYPE = Type::HEADER;

private:
	static const uint8_t FLAGS_TO_TYPE[16];

	enum
	{
		NODES,
		WAYS,
		AREAS,
		RELATIONS,
		INVALID
	};

	const IndexSettings& settings_;
	TIndex indexes_[4];			// for nodes, ways, areas & relations
	TExportTable* exportTable_ = nullptr;
};
