// Copyright (c) 2025 Clarisma / GeoDesk contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once
#include "ParallelQueryPrinter.h"
#include <clarisma/data/SmallVector.h>
#include <clarisma/io/FileBuffer2.h>
#include <clarisma/util/Xml.h>
#include <geodesk/feature/TagUtils.h>
#include <geodesk/format/LeafletFormatter.h>
#include "gol/map/MapFeatureOptions.h"

using namespace geodesk;

class MapQueryPrinter : public ParallelQueryPrinter<MapQueryPrinter>
{
public:
    MapQueryPrinter(clarisma::FileBuffer2& out,
        const QuerySpec* spec, const MapFeatureOptions* featureOptions) :
        ParallelQueryPrinter(spec),
        out_(out),
        featureOptions_(featureOptions)
    {
        // TODO: keyschema
        formatter_.precision(spec->precision());
    }

    const Box& resultBounds() const { return resultBounds_; }

    static void writeTagValue(FeaturePrinterBuffer& out, const TagValue& value)
    {
        if (value.isStoredNumeric())    [[unlikely]]
        {
            out << value.storedNumber();
            return;
        }
        clarisma::Xml::writeEscaped(out, value.storedString());
    }

    void writeTags(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) const
    {
        clarisma::SmallVector<Tag,16> tags;
        TagUtils::getTags(store, feature.tags(), spec()->keys(), tags);
        std::sort(tags.begin(), tags.end());
        out << "<pre>";
        for (Tag tag : tags)
        {
            out << "\\n" << tag.key() << "=";
            writeTagValue(out, tag.value());
        }
        out << "\\n</pre>";
    }

    static void writeTemplate(FeaturePrinterBuffer& out,
        const clarisma::TextTemplate* templ,
        const StringTable& strings, FeaturePtr feature)
    {
        templ->write(out, [feature,&strings](FeaturePrinterBuffer& out2, std::string_view k)
        {
            TagTablePtr tags = feature.tags();
            writeTagValue(out2, tags.tagValue(
                tags.getKeyValue(k, strings), strings));
        });
    }

    static void writeLink(FeaturePrinterBuffer& out,
        const clarisma::TextTemplate* urlTemplate,
        const StringTable& strings, FeaturePtr feature, bool forEdit)
    {
        if (urlTemplate)
        {
            writeTemplate(out, urlTemplate, strings, feature);
        }
        else
        {
            out << (forEdit ?
                    "https://www.openstreetmap.org/edit?" :
                    "https://www.openstreetmap.org/")
                << feature.typeName()
                << (forEdit ? '=' : '/')
                << feature.id();
        }
    }


    void print(FeaturePrinterBuffer& out, FeatureStore* store, FeaturePtr feature) // CRTP override
    {
        // out << "/* " << feature.typedId() << " */ ";
        const StringTable& strings = store->strings();
        formatter_.writeFeatureGeometry(out, store, feature);
        if (featureOptions_->hasPopup)
        {
            out << ").bindPopup('";
            if (featureOptions_->popup)
            {
                writeTemplate(out, featureOptions_->popup.get(),
                    strings, feature);
            }
            else
            {
                out << "<h3>";
                if (featureOptions_->hasLink)
                {
                    out << "<a href=\"";
                    writeLink(out, featureOptions_->linkUrl.get(),
                        strings, feature, false);
                    out << "\" target=\"_blank\">";
                }
                out << "NWR"[feature.typeCode()] << feature.id();
                if (featureOptions_->hasLink)
                {
                    out << "</a>";
                }
                if (featureOptions_->hasEdit)
                {
                    out << " <a class=\"edit\" href=\"";
                    writeLink(out, featureOptions_->editUrl.get(),
                        strings, feature, true);
                    out << "\" target=\"_blank\">EDIT</a>";
                }
                out << "</h3>";
                writeTags(out, store, feature);
            }
            out << '\'';
        }
        else if (featureOptions_->hasLink | featureOptions_->hasEdit)
        {
            out << ").on('click',e=>window.open('";
            writeLink(out,
                featureOptions_->hasEdit ?
                    featureOptions_->editUrl.get() :
                    featureOptions_->linkUrl.get(),
                strings, feature, featureOptions_->hasEdit);
            out << "','_blank')";
        }
        if (featureOptions_->hasTooltip)
        {
            out << ").bindTooltip('";
            if (featureOptions_->tooltip)
            {
                writeTemplate(out, featureOptions_->tooltip.get(),
                    strings, feature);
            }
            else
            {
                writeTags(out, store, feature);
            }
            out << '\'';
        }
        out << ").addTo(map);\n";
    }

protected:
     void processBatch(Batch& batch) override
     {
         Chunk<char>* chunk = batch.buffers.first();
         while(chunk)
         {
             out_.write(chunk->data(), chunk->size());
             chunk = chunk->next();
         }
         resultBounds_.expandToIncludeSimple(batch.bounds);
         resultCount_ += batch.count;
     }

private:
     clarisma::FileBuffer2& out_;
     LeafletFormatter formatter_;
     const MapFeatureOptions* featureOptions_;
     Box resultBounds_;
};
