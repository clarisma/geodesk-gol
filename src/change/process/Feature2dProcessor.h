// Copyright (c) 2026 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "FeatureProcessor.h"

class Feature2dProcessor : public FeatureProcessor
{
public:
	Feature2dProcessor(ChangeManager& mgr, ChangedFeature2D& feature) :
		FeatureProcessor(mgr, feature),
		pastBounds_(feature.bounds())
	{
	}

protected:
	ChangedFeature2D& wayOrRelation() const
	{
		assert(feature_.type() != FeatureType::NODE);
		return static_cast<ChangedFeature2D&>(feature_);
		// NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
		// cast is safe
	}

	std::span<CFeatureStub*> members() const
	{
		return wayOrRelation().members();
	}

	CRef getRefSE() const noexcept { return feature_.refSE(); }
	void setRefSE(CRef ref) const noexcept { feature_.setRefSE(ref); }

	int normalizeRefs() const
	{
		return mgr_.normalizeRefs(&wayOrRelation());
	}

	// TODO: move these to subclasses

	void ensureNodesLoaded() const
	{
		model().ensureNodesLoaded(&wayOrRelation());
	}

	void ensureMembersLoaded() const
	{
		model().ensureMembersLoaded(&wayOrRelation());
	}

	void processDeleted() const
	{
		model().getParentRelations(&feature_);
			// We need to ensure that the reltable is
			// loaded for a feature that has been deleted
			// without being removed from its parent relations,
			// so we can cascade
			// (Once we clear refs, we can no longer fetch
			// the original table)
		// TODO: This currently creates stub copies for all
		//  deletions (for ways, we could use the original
		//  for NW deletion)
		Tip tip = getRef().tip();
		if(!tip.isNull()) remove(false);
		// TIP could be null if feature does not exist
		// (already deleted)
		tip = getRefSE().tip();
		if(!tip.isNull()) remove(true);
		setRef(CRef::MISSING);
		setRefSE(CRef::MISSING);
	}

	void updateBounds() const
	{
		if (futureBounds_.isEmpty())
		{
			LOGS << "Empty bounds for " << feature_.typedId() << " @" << (&feature_);
		}
		assert(!futureBounds_.isEmpty());
		assert(futureBounds_ != pastBounds_);
		wayOrRelation().setBounds(futureBounds_);
		addFlags(ChangeFlags::BOUNDS_CHANGED);

		TilePair futureTiles = mgr_.tileCatalog().tilePair(futureBounds_);
		updateTiles(futureTiles);
	}

	void assignToTiles() const
	{
		if (getRef().tip().isNull())
		{
			LOGS << feature_.typedId() << " has ref " << getRef();
		}
		assert(!getRef().tip().isNull());
		assert(getRef().tip() != getRefSE().tip());

		CRef ref = getRefSE();
		Tip tip = ref.tip();
		if(!tip.isNull())   [[unlikely]]
		{
			mgr_.getChangedTile(tip)->addChanged(model().copy(&feature_));
		}
		ref = getRef();
		tip = ref.tip();
		mgr_.getChangedTile(tip)->addChanged(&feature_);
	}


private:
	void updateTiles(TilePair futureTiles) const
	{
	    ChangeFlags tileChanges = ChangeFlags::NONE;
	    CRef pastRefNW = getRef();
	    CRef pastRefSE = getRefSE();
	    Tip pastTipNW = pastRefNW.tip();
	    Tip pastTipSE = pastRefSE.tip();
	    Tip futureTipNW = mgr_.tileCatalog().tipOfTile(futureTiles.first());
	    Tip futureTipSE = futureTiles.hasSecond() ?
	        mgr_.tileCatalog().tipOfTile(futureTiles.second()) : Tip();
	    assert(pastTipNW != pastTipSE || pastTipNW.isNull());
	    assert(futureTipNW != futureTipSE);
	    assert(!futureTipNW.isNull());

	    if (pastTipNW != futureTipNW)
	    {
	        tileChanges |= ChangeFlags::TILES_CHANGED;
	        if (futureTipNW != pastTipSE)
	        {
	            tileChanges |= ChangeFlags::NEW_TO_NORTHWEST;
	            if (pastTipNW != futureTipSE)
	            {
	            	if (!pastTipNW.isNull())
	            	{
	            		// remove from past NW tile
	            		remove(false);
	            	}
	            }
	            setRef(CRef::ofNew(futureTipNW));
	        }
	        else
	        {
	            // Set SE tile as new NW tile
	            // (feature simply moved SE)
	            setRef(pastRefSE);
	        }
	    }

	    if (pastTipSE != futureTipSE)
	    {
	        tileChanges |= ChangeFlags::TILES_CHANGED;
	        if (futureTipSE != pastTipNW)
	        {
	            if (!pastTipSE.isNull())
	            {
	                // remove from past SE tile
	            	remove(true);
	            }
	            if (futureTipSE.isNull())
	            {
	                setRefSE(CRef::SINGLE_TILE);
	            }
	            else
	            {
	                setRefSE(CRef::ofNew(futureTipSE));
	                tileChanges |= ChangeFlags::NEW_TO_SOUTHEAST;
	            }
	        }
	        else
	        {
	            // Set NW tile as new SE tile
	            // (feature simply moved NW)
	            setRefSE(pastRefNW);
	        }
	    }
	    if (futureTipSE.isNull())
	    {
	        setRefSE(CRef::SINGLE_TILE);
	    }
	    addFlags(tileChanges);

	    // TODO: Does it make sense to mark a feature as NEW (a common case)
	    //  to skip these checks?
	    if (isAny(ChangeFlags::NEW_TO_NORTHWEST | ChangeFlags::NEW_TO_SOUTHEAST))
	    {
	        CRef sourceRef = pastRefNW;
	        if (!sourceRef.canGetFeature())
	        {
	            sourceRef = pastRefSE;
	        }
	        if (!feature_.tagTable() && sourceRef.canGetFeature())
	        {
	            feature_.setTagTable(model().getTagTable(sourceRef));
	        }
	        if (!is(ChangeFlags::RELTABLE_LOADED))
	        {
	            feature_.setParentRelations(model().getRelationTable(sourceRef));
	        }
	    }
	}

protected:
	Box pastBounds_;
	Box futureBounds_;
};
