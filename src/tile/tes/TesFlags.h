// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

namespace TesFlags
{
	// Change (Stored in TES)

	constexpr int TAGS_CHANGED = 1 << 0;
	constexpr int SHARED_TAGS = 1 << 1;
	constexpr int RELATIONS_CHANGED = 1 << 2;
	constexpr int GEOMETRY_CHANGED = 1 << 3;
	constexpr int MEMBERS_CHANGED = 1 << 4;
	constexpr int NODE_BELONGS_TO_WAY = 1 << 4;
		// TODO: Make NODE_BELONGS_TO_WAY Bit 5 to match FeatureFlags::WAYNODE?
	constexpr int NODE_IDS_CHANGED = 1 << 5;
	constexpr int BBOX_CHANGED = 1 << 5;
	constexpr int HAS_SHARED_LOCATION = 1 << 5;
	constexpr int IS_AREA = 1 << 6;
	constexpr int IS_EXCEPTION_NODE = 1 << 6;

	// Change (Calculated)

	constexpr int RELTABLE_CREATED = 1 << 8;	// TODO: not needed?
	constexpr int RELTABLE_DROPPED = 1 << 9;	// TODO: not needed?

	// Member
	constexpr int FOREIGN_MEMBER = 1 << 0;
	constexpr int DIFFERENT_ROLE = 1 << 1;
	constexpr int DIFFERENT_TILE = 1 << 2;
}
