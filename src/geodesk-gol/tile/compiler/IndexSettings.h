// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <geodesk/feature/FeatureStore.h>

class IndexSettings 
{
public:
    IndexSettings(const FeatureStore::IndexedKeyMap& keysToCategories, 
        int rtreeBucketSize, int maxKeyIndexes, int keyIndexMinFeatures) :
        rtreeBucketSize_(rtreeBucketSize),
        maxKeyIndexes_(maxKeyIndexes),
        keyIndexMinFeatures_(keyIndexMinFeatures),
        keysToCategories_(keysToCategories),
        maxIndexedKey_(findMaxIndexedKey(keysToCategories))
    {
    }

    int rtreeBucketSize() const { return rtreeBucketSize_; }
    int maxKeyIndexes() const { return maxKeyIndexes_; }
    int keyIndexMinFeatures() const { return keyIndexMinFeatures_; }
    int maxIndexedKey() const { return maxIndexedKey_; }
    int getCategory(int key) const
    {
        auto it = keysToCategories_.find(key);
        return (it != keysToCategories_.end()) ? it->second : 0;
    }

private:
    static int findMaxIndexedKey(const FeatureStore::IndexedKeyMap& map)
    {
        int maxKey = 0;
        for (const auto& pair : map)
        {
            if (pair.first > maxKey) maxKey = pair.first;
        }
        return maxKey;
    }

    const int rtreeBucketSize_;
    const int maxKeyIndexes_;
    const int keyIndexMinFeatures_;
    const int maxIndexedKey_;
    FeatureStore::IndexedKeyMap keysToCategories_;
};

