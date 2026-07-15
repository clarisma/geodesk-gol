// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

enum OsmPbf
{
	BLOBHEADER_TYPE = (1 << 3) | 2,
	BLOBHEADER_DATASIZE = (3 << 3),

	BLOB_RAW_DATA = (1 << 3) | 2,
	BLOB_RAW_SIZE = (2 << 3),
	BLOB_ZLIB_DATA = (3 << 3) | 2,

	HEADER_BBOX = (1 << 3) | 2,
	HEADER_REQUIRED_FEATURES = (4 << 3) | 2,
	HEADER_OPTIONAL_FEATURES = (5 << 3) | 2,
	HEADER_WRITINGPROGRAM = (16 << 3) | 2,
	HEADER_SOURCE = (17 << 3) | 2,
	HEADER_REPLICATION_TIMESTAMP = (32 << 3),
	HEADER_REPLICATION_SEQUENCE = (33 << 3),
	HEADER_REPLICATION_URL = (34 << 3) | 2,

	BLOCK_STRINGTABLE = (1 << 3) | 2,
	BLOCK_GROUP = (2 << 3) | 2,
	BLOCK_GRANULARITY = 17 << 3,
	BLOCK_DATE_GRANULARITY = 18 << 3,
	BLOCK_LAT_OFFSET = 19 << 3,
	BLOCK_LON_OFFSET = 20 << 3,

	STRINGTABLE_ENTRY = (1 << 3) | 2,

	// Structures that appear within a PrimitiveGroup
	GROUP_NODE = (1 << 3) | 2,
	GROUP_DENSENODES = (2 << 3) | 2,
	GROUP_WAY = (3 << 3) | 2,
	GROUP_RELATION = (4 << 3) | 2,
	GROUP_CHANGESET = (5 << 3) | 2,

	DENSENODE_IDS = (1 << 3) | 2,
	DENSENODE_INFO = (5 << 3) | 2,
	DENSENODE_LATS = (8 << 3) | 2,
	DENSENODE_LONS = (9 << 3) | 2,
	DENSENODE_TAGS = (10 << 3) | 2,

	ELEMENT_ID = (1 << 3),
	ELEMENT_KEYS = (2 << 3) | 2,
	ELEMENT_VALUES = (3 << 3) | 2,
	ELEMENT_INFO = (4 << 3) | 2,

	WAY_NODES = (8 << 3) | 2,
	WAY_LATS = (9 << 3) | 2,
	WAY_LONS = (10 << 3) | 2,

	RELATION_MEMBER_ROLES = (8 << 3) | 2,
	RELATION_MEMBER_IDS = (9 << 3) | 2,
	RELATION_MEMBER_TYPES = (10 << 3) | 2
};
