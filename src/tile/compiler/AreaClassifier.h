// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include <clarisma/data/HashMap.h>
#include <clarisma/util/Strings.h>
#include <geodesk/feature/GlobalStrings.h>

using namespace clarisma;
using namespace geodesk;

class AreaClassifier
{
public:
    static constexpr int IS_AREA = 1;
    static constexpr int DEFINITE_FOR_WAYS = 4;
    static constexpr int DEFINITE_FOR_RELATIONS = 8;

    AreaClassifier(const char* rules);

    int check(uint_fast32_t key, uint_fast32_t value) const
    {
        auto it = globalKeyRules_.find(Global(key,0));
        if (it == globalKeyRules_.end()) return 0;
        if(value == GlobalStrings::NO) return 0;
        return check(it->second, value);
    }

    int check(uint_fast32_t key, ShortVarString* value) const
    {
        auto it = globalKeyRules_.find(Global(key,0));
        if (it == globalKeyRules_.end()) return 0;
        return check(it->second, value);
    }

    int check(ShortVarString* key, uint_fast32_t value) const
    {
        auto it = localKeyRules_.find(Local(key,0));
        if (it == localKeyRules_.end()) return 0;
        if(value == GlobalStrings::NO) return 0;
        return check(it->second, value);
    }

    int check(ShortVarString* key, ShortVarString* value) const
    {
        auto it = localKeyRules_.find(Local(key,0));
        if (it == localKeyRules_.end()) return 0;
        return check(it->second, value);
    }

private:
    struct Global
    {
        uint16_t key;
        uint16_t ruleNumber;

        Global(uint16_t k, uint16_t n) : key(k), ruleNumber(n) {}

        bool operator==(const Global& other) const
        {
            return key==other.key && ruleNumber==other.ruleNumber;
        }

        struct Hash
        {
            std::size_t operator()(const Global& g) const noexcept
            {
                // Just hash based on key, as keys are likely unique among rules
                return std::hash<uint16_t>{}(g.key);
            }
        };
    };

    struct Local
    {
        ShortVarString* key;
        uint16_t ruleNumber;

        Local(ShortVarString* k, uint16_t n) : key(k), ruleNumber(n) {}

        bool operator==(const Local& other) const
        {
            return *key==*other.key && ruleNumber==other.ruleNumber;
        }

        struct Hash
        {
            std::size_t operator()(const Local& l) const noexcept
            {
                // Just hash based on key, as keys are likely unique among rules
                return Strings::hash(l.key->data(), l.key->length());
            }
        };
    };

    enum
    {
        REJECT_SOME = 1,		// keep these values
        ACCEPT_ALL = 2
    };


    struct RuleRef
    {
        uint16_t number;
        uint8_t flags;
        uint8_t mode;
    };

    int check(RuleRef rule, uint_fast32_t value) const
    {
        if(rule.mode == ACCEPT_ALL) return IS_AREA | rule.flags;
        bool found = globalKeyRules_.find(Global(value,rule.number)) != globalKeyRules_.end();
        return (found ^ rule.mode) | rule.flags;
    }

    int check(RuleRef rule, ShortVarString* value) const
    {
        if(rule.mode == ACCEPT_ALL) return IS_AREA | rule.flags;
        bool found = localKeyRules_.find(Local(value,rule.number)) != localKeyRules_.end();
        return (found ^ rule.mode) | rule.flags;
    }

    HashMap<Global,RuleRef,Global::Hash> globalKeyRules_;
    HashMap<Local,RuleRef,Local::Hash> localKeyRules_;
    std::unique_ptr<std::byte[]> strings_;
};

