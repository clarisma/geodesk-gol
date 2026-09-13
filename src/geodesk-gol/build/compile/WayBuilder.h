// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "Compiler.h"
#include "tile/compiler/NodeTableWriter.h"
#include "tile/model/MutableFeaturePtr.h"
#include "tile/model/TWay.h"

class TRelationTable;

// TODO: Handle the WAY_NODE flag of nodes!

class WayBuilder
{
public:
	WayBuilder(CompilerWorker ctx) : 
		ctx_(ctx)
	{
	}

	void build(TWay* way)
	{
		const uint8_t* p = way->body()->data();
		const uint8_t* pEnd = p + way->body()->size();

		uint32_t relTablePtrSize = 4;	// TODO

		ForeignFeatureRef firstForeign;
		ForeignFeatureRef lastForeign;
		TNode* firstLocal = nullptr;
		TNode* lastLocal = nullptr;
		bool isFirst = true;

		uint32_t nodeCount = readVarint32(p);		// TODO: closed_ring_flag

		// We pre-allocate space for the way's body using the most conservative
		// assumptions:
		// - Each node is a foreign feature node, with wide tip/tex deltas (8 bytes each)
		// - Each coordinate pair requires 10 bytes to varint-encode
		// - We assume one extra node (first node duplicated for non-area closed ways)
		// We also account for the relation-table pointer (if way is a member), and
		// round up to the nearest 4 bytes so the temporary Coordinate array is
		// properly aligned
		// TODO: also need space for the node-IDs (optional) + 1 for extra node

		uint32_t maxBodySize = ((nodeCount + 1) * 18 + relTablePtrSize + 3) & ~3;

		static_assert(alignof(Coordinate) == 4, "Unexpected alignment");
		uint8_t* pBodyStart = ctx_.tile_.arena().alloc(maxBodySize, 4);

		Coordinate* pFirstXY = reinterpret_cast<Coordinate*>(
			pBodyStart + maxBodySize) - nodeCount;
		Coordinate* pNextXY = pFirstXY;

		TElement::Handle bodyHandle = ctx_.tile_.newHandle();
		uint8_t* pNodeTableUpper = reinterpret_cast<uint8_t*>(pFirstXY);
		NodeTableWriter writer(bodyHandle, pNodeTableUpper);
			// we place the FeatureNodeTable ahead of the coordinates;
			// we'll move the encoded data into its proper place once 
			// it's fully written and we know its size

		Box bounds;
		int64_t nodeId = 0;
		for (int i = 0; i < nodeCount; i++)
		{
			Coordinate xy(0, 0);
			nodeId += readSignedVarint64(p);
			auto it = ctx_.coords_.find(nodeId);
			if (it != ctx_.coords_.end())
			{
				// Plain coordinate (local or foreign) -- most likely case
				xy = it->second;
				lastForeign = ForeignFeatureRef();
				lastLocal = nullptr;
			}
			else
			{
				TNode* local = ctx_.tile_.getNode(nodeId);
				if (local)
				{
					MutableFeaturePtr(local->feature()).setFlag(FeatureFlags::WAYNODE, true);
					writer.writeLocalNode(local);
					*(isFirst ? &firstLocal : &lastLocal) = local;
					// TODO: fix!!!
					lastForeign = ForeignFeatureRef();
					xy = local->xy();
				}
				else
				{
					// Must be a foreign feature node
					auto it = ctx_.foreignNodes_.find(nodeId);
					if (it == ctx_.foreignNodes_.end())
					{
						// TODO: We have a problem
						assert(false);
					}

                    // TODO: Remember, DIFFERENT_TILE flag must
                    //  be set for first node even if its TIP
					//  is the same as the starting TIP
					// TODO

					xy = it->second.xy;
					*(isFirst ? &firstForeign : &lastForeign) = it->second;
					lastLocal = nullptr;
				}
			}
			*pNextXY++ = xy;
			bounds.expandToInclude(xy);
			isFirst = false;
		}

		// TODO: if first node is a feature node and the way forms a closed loop,
		// add the feature-node reference for the last node

		writer.markLast();
		uint8_t* pNodeTableLower = writer.ptr().ptr();
		assert(nodeTableLower >= pBodyStart);
			// Make sure we didn't write the feature-node table beyond the buffer start
		assert(reinterpret_cast<uint8_t*>(pNextXY) == pBodyStart + maxBodySize);
			// Make sure we didn't write the coordinate table beyond the buffer end
		assert(p <= pEnd);		// We didn't read past end of proto-gol body data

		MutableFeaturePtr pWay(way->feature());
		assert(!bounds.isEmpty());
		pWay.setBounds(bounds);
		pWay.setTags(way->handle(), ctx_.readTags(ByteSpan(p, pEnd)));
		// TODO: Determine if way is an area based on tags

		size_t nodeTableSize = pNodeTableUpper - pNodeTableLower;
		// Move the feature-node table into its proper place
		memmove(pBodyStart, pNodeTableLower, nodeTableSize);
		uint8_t* pEncodedCoord = pBodyStart + nodeTableSize + relTablePtrSize;
		Coordinate prevXY = bounds.bottomLeft();
		for (int i = 0; i < nodeCount; i++)
		{
			Coordinate xy = pFirstXY[i];
			writeSignedVarint(pEncodedCoord, xy.x - prevXY.x);
			writeSignedVarint(pEncodedCoord, xy.y - prevXY.y);
			prevXY = xy;
		}
		if (false)	// TODO: isClosedRing && !isArea
		{
			writeSignedVarint(pEncodedCoord, pFirstXY->x - prevXY.x);
			writeSignedVarint(pEncodedCoord, pFirstXY->y - prevXY.y);
		}

		// TODO: copy node IDs
		// TODO: add first node ID at end (delta-encoded) if non-area closed ring

		// TODO: write extra coord if closed way, but not area
	}

	CompilerWorker& ctx_;
	TWay* way_;
};

