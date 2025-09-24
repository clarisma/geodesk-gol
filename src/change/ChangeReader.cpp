// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChangeReader.h"
#include <geodesk/feature/FeatureStore.h>
#include <clarisma/util/log.h>
#include "change/model/ChangeModel.h"
#include "change/model/ChangedNode.h"
#include "tag/TagTableModel.h"

// TODO: If version is missing from a change, set it to 1,
//  so we can distinguish explicit vs. implciit changes

ChangeReader::ChangeReader(ChangeModel& model, char* xml) :
    model_(model),
    strings_(model.store()->strings()),
    SimpleXmlParser(xml),
    id_(0),
    attributes_(0),
    version_(0)
{
}


void ChangeReader::read()
{
    bool foundOsmChange = false;
    for(;;)
    {
        int token = next();
        if(token == END) break;
        if(token == TAG_START)
        {
            if(name() == "osmChange")
            {
                foundOsmChange = true;
                for(;;)
                {
                    token = next();
                    if(token == END) break;
                    if(token == TAG_START) readChanges();
                }
            }
        }
    }
    if (!foundOsmChange)   [[unlikely]]
    {
        error("Expected <osmChange> element");
    }
}

void ChangeReader::readChanges()
{
    std::string_view section = name();
    ChangeFlags flags = ChangeFlags::NONE;
    if(section == "modify" || section == "create")
    {
        // do nothing
    }
    else if (section == "delete")
    {
        flags |= ChangeFlags::DELETED;
    }
    else
    {
        error("Unknown section: %.*s", section.size(), section.data());
    }
    for(;;)
    {
        int token = next();

        if(token == TAG_START)
        {
            readFeature(flags);
        }
        else if(token == END)
        {
            break;
        }
    }
}

void ChangeReader::readTag()
{
    constexpr int ATTR_KEY = 1;
    constexpr int ATTR_VALUE = 2;
    int attributes = 0;
    std::string_view k;
    std::string_view v;
    for(;;)
    {
        int token = next();
        if(token != ATTR) break;
        std::string_view attr = name();
        if(attr == "k")
        {
            k = value();
            attributes |= ATTR_KEY;
        }
        else if(attr == "v")
        {
            v = value();
            attributes |= ATTR_VALUE;
        }
    }
    if(attributes != (ATTR_KEY | ATTR_VALUE))
    {
        error("<tag> must have attributes 'k' and 'v'");
    }
    int keyCode = strings_.getCode(k);
    int valueCode = strings_.getCode(v);
    if(keyCode >= 0 && keyCode <= TagValues::MAX_COMMON_KEY)    [[likely]]
    {
        if(valueCode >= 0)
        {
            tags_.addGlobalTag(keyCode, valueCode);
        }
        else
        {
            tags_.addGlobalTag(keyCode, v);
        }
    }
    else
    {
        if(valueCode >= 0)
        {
            tags_.addLocalTag(k, valueCode);
        }
        else
        {
            tags_.addLocalTag(k, v);
        }
    }
}


const CTagTable* ChangeReader::setTags(ChangedFeatureBase* changed, bool checkIfArea)
{
    const CTagTable* tagTable;
    if(!tags_.isEmpty())
    {
        tags_.normalize();
        tagTable = model_.getTagTable(tags_, checkIfArea);
    }
    else
    {
        tagTable = &CTagTable::EMPTY;
    }
    changed->setTagTable(tagTable);
    return tagTable;
}


void ChangeReader::readFeature(ChangeFlags flags)
{
    assert(tags_.isEmpty());
    assert(members_.empty());
    assert(roles_.empty());

    std::string_view type = name();
    int token = readFeatureAttributes();
    readFeatureElements(token);
    uint32_t adjustedVersion = version_;

    if(test(flags, ChangeFlags::DELETED))
    {
        tags_.clear();
        members_.clear();
        adjustedVersion++;
    }
    else
    {
        flags = ChangeFlags::TAGS_CHANGED | ChangeFlags::GEOMETRY_CHANGED |
            ChangeFlags::MEMBERS_CHANGED | ChangeFlags::WAYNODE_IDS_CHANGED;
    }

    if(type == "node")
    {
        if(476124671 == id_)
        {
            LOGS << "Reading node/" << id_;
        }

        ChangedNode* node = model_.getChangedNode(id_);
        if(adjustedVersion > node->version())
        {
            node->setVersion(version_);
            node->setFlags(flags & ~(ChangeFlags::MEMBERS_CHANGED | ChangeFlags::WAYNODE_IDS_CHANGED));
            node->setXY(xy_);
            setTags(node, false);
        }
        else
        {
            LOGS << "Omitting change for " << node->typedId() << ": Version "
                << version_ << " read after " << node->version();
        }
    }
    else
    {
        FeatureType featureType;
        CFeature::Role* roles;
        bool possibleArea = false;
        bool isRelation;

        if(type == "way")
        {
            isRelation = false;
            if(476124671 == id_)
            {
                LOGS << "Reading way/" << id_;
            }

            featureType = FeatureType::WAY;
            flags &= ~ChangeFlags::MEMBERS_CHANGED;

            roles = nullptr;
            if(members_.size() < 2)
            {
                flags = ChangeFlags::DELETED;
                tags_.clear();
                members_.clear();
            }
            else if (members_.size() > 2)
            {
                if(members_[0]->id() == members_.back()->id())
                {
                    possibleArea = true;
                }
            }
        }
        else
        {
            assert(type == "relation");
            isRelation = true;
            featureType = FeatureType::RELATION;
            roles = roles_.data();
            flags &= ~ChangeFlags::WAYNODE_IDS_CHANGED;
            if(members_.empty())
            {
                flags = ChangeFlags::DELETED;
                tags_.clear();
            }
            else
            {
                for(CFeature::Role role : roles_)
                {
                    if(role.isGlobal(GlobalStrings::OUTER))
                    {
                        possibleArea = true;
                        break;
                    }
                }
            }
        }

        ChangedFeature2D* feature = model_.getChangedFeature2D(featureType, id_);
        if(adjustedVersion > feature->version())
        {
            feature->setVersion(version_);
            const CTagTable* tags = setTags(feature, possibleArea);
            bool willBeArea = possibleArea && tags->isArea(isRelation);
            size_t adjustedMemberCount = members_.size();
            if (!isRelation && willBeArea)
            {
                adjustedMemberCount--;
                // For ways that are areas, we omit the final node
                // (must be equal to first)
            }
            flags |= willBeArea ? ChangeFlags::WILL_BE_AREA : ChangeFlags::NONE;
            feature->setFlags(flags);
            // TODO: Treatment of empty member list?
            model_.setMembers(feature, members_.data(), adjustedMemberCount, roles);
        }
        else
        {
            LOGS << "Omitting change for " << feature->typedId() << ": Version "
                << version_ << " read after " << feature->version();
        }
    }

    tags_.clear();
    members_.clear();
    roles_.clear();

    // LOGS << "Read " << type << "/" << id_;
}

int ChangeReader::readFeatureAttributes()
{
    attributes_ = 0;
    for(;;)
    {
        int token = next();
        if(token != ATTR) return token;
        std::string_view attr = name();
        if(attr == "id")
        {
            id_ = longValue();
            attributes_ |= ATTR_ID;
        }
        else if(attr == "version")
        {
            version_ = static_cast<uint32_t>(longValue());
            attributes_ |= ATTR_VERSION;
        }
        else if(attr == "lon")
        {
            // TODO: clipping
            xy_.x = Mercator::xFromLon(doubleValue());
            attributes_ |= ATTR_LON;
        }
        else if(attr == "lat")
        {
            // TODO: clipping
            xy_.y = Mercator::yFromLat(doubleValue());
            attributes_ |= ATTR_LAT;
        }
    }
}


void ChangeReader::readFeatureElements(int token)
{
    for(;;)
    {
        if(token == TAG_START)
        {
            std::string_view n = name();
            if(n == "tag")
            {
                readTag();
            }
            else if(n == "nd")
            {
                readNodeRef();
            }
            else if(n == "member")
            {
                readMember();
            }
            else
            {
                error("Unexpected element: %.*s", n.size(), n.data());
            }
        }
        else if(token == END)
        {
            break;
        }
        token = next();
    }
}


void ChangeReader::readNodeRef()
{
    for(;;)
    {
        int token = next();
        if(token != ATTR) break;
        if(name() == "ref")
        {
            int64_t id = longValue();
            members_.push_back(model_.getFeatureStub(
                TypedFeatureId::ofNode(id)));
        }
    }
}


void ChangeReader::readMember()
{
    constexpr int ATTR_TYPE = 1;
    constexpr int ATTR_REF = 2;
    constexpr int ATTR_ROLE = 4;
    int attributes = 0;
    std::string_view type;
    int64_t id = 0;
    std::string_view role;
    for(;;)
    {
        int token = next();
        if(token != ATTR) break;
        if(name() == "type")
        {
            type = value();
            attributes |= ATTR_TYPE;
        }
        else if(name() == "ref")
        {
            id = longValue();
            attributes |= ATTR_REF;
        }
        else if(name() == "role")
        {
            role = value();
            attributes |= ATTR_ROLE;
        }
    }
    if(attributes != (ATTR_TYPE | ATTR_REF | ATTR_ROLE))
    {
        error("<member> must have attributes 'type', 'ref' and 'role'");
    }
    TypedFeatureId typedId(0);
    if(type == "node")
    {
        typedId = TypedFeatureId::ofNode(id);
    }
    else if(type == "way")
    {
        typedId = TypedFeatureId::ofWay(id);
    }
    else if(type == "relation")
    {
        if(id == id_)   [[unlikely]]
        {
            Console::msg("relation/%lld: Removed self-reference", id);
            return;
        }
        typedId = TypedFeatureId::ofRelation(id);
    }
    else
    {
        error("Invalid feature type: %.*s", type.size(), type.data());
    }
    members_.push_back(model_.getFeatureStub(typedId));
    roles_.push_back(model_.getRole(role));
}
