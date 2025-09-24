// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <geodesk/feature/ForeignFeatureRef.h>
#include "Role.h"

class TFeature;
using geodesk::ForeignFeatureRef;

struct RelationMember
{
    RelationMember(TFeature* local_, ForeignFeatureRef foreign_, Role role_) :
        local(local_), foreign(foreign_), role(role_) {}

    TFeature* local;
    ForeignFeatureRef foreign;
    Role role;
};

