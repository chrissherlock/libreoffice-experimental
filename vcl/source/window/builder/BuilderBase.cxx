/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_feature_desktop.h>
#include <config_options.h>
#include <config_vclplug.h>

#include <sal/log.hxx>
#include <o3tl/string_view.hxx>

#include <vcl/builder.hxx>

#include <desktop/crashreport.hxx>

#include <svdata.hxx>

#if defined(DISABLE_DYNLOADING) || defined(LINUX)
#include <dlfcn.h>
#endif

#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>

#include <memory>
#include <string_view>
#include <utility>

bool toBool(std::u16string_view rValue)
{
    return (!rValue.empty() && (rValue[0] == 't' || rValue[0] == 'T' || rValue[0] == '1'));
}

// static
void BuilderBase::reportException(const css::uno::Exception& rExcept)
{
    CrashReporter::addKeyValue(u"VclBuilderException"_ustr,
                               "Unable to read .ui file: " + rExcept.Message, CrashReporter::Write);
}

BuilderBase::BuilderBase(std::u16string_view sUIDir, const OUString& rUIFile, bool bLegacy)
    : m_pParserState(new ParserState)
    , m_sUIFileUrl(sUIDir + rUIFile)
    , m_sHelpRoot(rUIFile)
    , m_bLegacy(bLegacy)
{
    const sal_Int32 nIdx = m_sHelpRoot.lastIndexOf('.');
    if (nIdx != -1)
        m_sHelpRoot = m_sHelpRoot.copy(0, nIdx);
    m_sHelpRoot += "/";
}

const std::locale& BuilderBase::getResLocale() const
{
    assert(m_pParserState && "parser state no more valid");
    return m_pParserState->m_aResLocale;
}

const std::vector<BuilderBase::SizeGroup>& BuilderBase::getSizeGroups() const
{
    assert(m_pParserState && "parser state no more valid");
    return m_pParserState->m_aSizeGroups;
}

const std::vector<BuilderBase::MnemonicWidgetMap>& BuilderBase::getMnemonicWidgetMaps() const
{
    assert(m_pParserState && "parser state no more valid");
    return m_pParserState->m_aMnemonicWidgetMaps;
}

const std::vector<BuilderBase::RadioButtonGroupMap>& BuilderBase::getRadioButtonGroupMaps() const
{
    assert(m_pParserState && "parser state no more valid");
    return m_pParserState->m_aRadioButtonGroupMaps;
}

OUString BuilderBase::finalizeValue(const OString& rContext, const OString& rValue,
                                    const bool bTranslate) const
{
    OUString sFinalValue;
    if (bTranslate)
    {
        sFinalValue
            = Translate::get(TranslateId{ rContext.getStr(), rValue.getStr() }, getResLocale());
    }
    else
        sFinalValue = OUString::fromUtf8(rValue);

    if (ResHookProc pStringReplace = Translate::GetReadStringHook())
        sFinalValue = (*pStringReplace)(sFinalValue);

    return sFinalValue;
}

void BuilderBase::resetParserState() { m_pParserState.reset(); }

static inline OUString lcl_extractStringEntry(BuilderBase::stringmap& rMap, const OUString& rKey,
                                   const OUString& rDefaultValue = OUString())
{
    BuilderBase::stringmap::iterator aFind = rMap.find(rKey);
    if (aFind != rMap.end())
    {
        const OUString sValue = aFind->second;
        rMap.erase(aFind);
        return sValue;
    }
    return rDefaultValue;
}

static inline bool lcl_extractBoolEntry(BuilderBase::stringmap& rMap, const OUString& rKey, bool bDefaultValue)
{
    BuilderBase::stringmap::iterator aFind = rMap.find(rKey);
    if (aFind != rMap.end())
    {
        const bool bValue = toBool(aFind->second);
        rMap.erase(aFind);
        return bValue;
    }
    return bDefaultValue;
}

void BuilderBase::extractRadioButtonGroup(const OUString& id, stringmap& rMap)
{
    const OUString sGroupId = extractGroup(rMap);
    if (sGroupId.isEmpty())
        return;

    m_pParserState->m_aRadioButtonGroupMaps.emplace_back(id, sGroupId);
}

void BuilderBase::extractMnemonicWidget(const OUString& rLabelID, stringmap& rMap)
{
    VclBuilder::stringmap::iterator aFind = rMap.find(u"mnemonic-widget"_ustr);
    if (aFind != rMap.end())
    {
        OUString sID = aFind->second;
        sal_Int32 nDelim = sID.indexOf(':');
        if (nDelim != -1)
            sID = sID.copy(0, nDelim);
        m_pParserState->m_aMnemonicWidgetMaps.emplace_back(rLabelID, sID);
        rMap.erase(aFind);
    }
}

void BuilderBase::collectPangoAttribute(xmlreader::XmlReader& reader, stringmap& rMap)
{
    xmlreader::Span span;
    int nsId;

    OUString sProperty;
    OUString sValue;

    while (reader.nextAttribute(&nsId, &span))
    {
        if (span == "name")
        {
            span = reader.getAttributeValue(false);
            sProperty = OUString(span.begin, span.length, RTL_TEXTENCODING_UTF8);
        }
        else if (span == "value")
        {
            span = reader.getAttributeValue(false);
            sValue = OUString(span.begin, span.length, RTL_TEXTENCODING_UTF8);
        }
    }

    if (!sProperty.isEmpty())
        rMap[sProperty] = sValue;
}

void BuilderBase::collectAtkRelationAttribute(xmlreader::XmlReader& reader, stringmap& rMap)
{
    xmlreader::Span span;
    int nsId;

    OUString sProperty;
    OUString sValue;

    while (reader.nextAttribute(&nsId, &span))
    {
        if (span == "type")
        {
            span = reader.getAttributeValue(false);
            sProperty = OUString(span.begin, span.length, RTL_TEXTENCODING_UTF8);
        }
        else if (span == "target")
        {
            span = reader.getAttributeValue(false);
            sValue = OUString(span.begin, span.length, RTL_TEXTENCODING_UTF8);
            sal_Int32 nDelim = sValue.indexOf(':');
            if (nDelim != -1)
                sValue = sValue.copy(0, nDelim);
        }
    }

    if (!sProperty.isEmpty())
        rMap[sProperty] = sValue;
}

void BuilderBase::collectAtkRoleAttribute(xmlreader::XmlReader& reader, stringmap& rMap)
{
    xmlreader::Span span;
    int nsId;

    OUString sProperty;

    while (reader.nextAttribute(&nsId, &span))
    {
        if (span == "type")
        {
            span = reader.getAttributeValue(false);
            sProperty = OUString(span.begin, span.length, RTL_TEXTENCODING_UTF8);
        }
    }

    if (!sProperty.isEmpty())
        rMap[u"role"_ustr] = sProperty;
}

void BuilderBase::handleListStore(xmlreader::XmlReader& reader)
{
    int nLevel = 1;

    while (true)
    {
        xmlreader::Span name;
        int nsId;

        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            assert(name != "row" && "Defining model data in UI files is not supported");
            ++nLevel;
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }
}

BuilderBase::stringmap BuilderBase::handleAtkObject(xmlreader::XmlReader& reader) const
{
    int nLevel = 1;

    stringmap aProperties;

    while (true)
    {
        xmlreader::Span name;
        int nsId;

        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            ++nLevel;
            if (name == "property")
                collectProperty(reader, aProperties);
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }

    return aProperties;
}

std::vector<ComboBoxTextItem> BuilderBase::handleItems(xmlreader::XmlReader& reader) const
{
    int nLevel = 1;

    std::vector<ComboBoxTextItem> aItems;

    while (true)
    {
        xmlreader::Span name;
        int nsId;

        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            ++nLevel;
            if (name == "item")
            {
                bool bTranslated = false;
                OString sContext;
                OUString sId;

                while (reader.nextAttribute(&nsId, &name))
                {
                    if (name == "translatable" && reader.getAttributeValue(false) == "yes")
                    {
                        bTranslated = true;
                    }
                    else if (name == "context")
                    {
                        name = reader.getAttributeValue(false);
                        sContext = OString(name.begin, name.length);
                    }
                    else if (name == "id")
                    {
                        name = reader.getAttributeValue(false);
                        sId = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
                    }
                }

                (void)reader.nextItem(xmlreader::XmlReader::Text::Raw, &name, &nsId);

                OString sValue(name.begin, name.length);
                const OUString sFinalValue = finalizeValue(sContext, sValue, bTranslated);
                aItems.emplace_back(sFinalValue, sId);
            }
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }

    return aItems;
}

void BuilderBase::handleSizeGroup(xmlreader::XmlReader& reader)
{
    m_pParserState->m_aSizeGroups.emplace_back();
    SizeGroup& rSizeGroup = m_pParserState->m_aSizeGroups.back();

    int nLevel = 1;

    while (true)
    {
        xmlreader::Span name;
        int nsId;

        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            ++nLevel;
            if (name == "widget")
            {
                while (reader.nextAttribute(&nsId, &name))
                {
                    if (name == "name")
                    {
                        name = reader.getAttributeValue(false);
                        OUString sWidget(name.begin, name.length, RTL_TEXTENCODING_UTF8);
                        sal_Int32 nDelim = sWidget.indexOf(':');
                        if (nDelim != -1)
                            sWidget = sWidget.copy(0, nDelim);
                        rSizeGroup.m_aWidgets.push_back(sWidget);
                    }
                }
            }
            else
            {
                if (name == "property")
                    collectProperty(reader, rSizeGroup.m_aProperties);
            }
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }
}

/// Insert items to a ComboBox or a ListBox.
/// They have no common ancestor that would have 'InsertEntry()', so use a template.
template <typename T>
static bool insertItems(vcl::Window* pWindow, std::vector<std::unique_ptr<OUString>>& rUserData,
                        const std::vector<ComboBoxTextItem>& rItems, sal_Int32 nActiveIndex)
{
    T* pContainer = dynamic_cast<T*>(pWindow);
    if (!pContainer)
        return false;

    for (auto const& item : rItems)
    {
        sal_Int32 nPos = pContainer->InsertEntry(item.m_sItem);
        if (!item.m_sId.isEmpty())
        {
            rUserData.emplace_back(std::make_unique<OUString>(item.m_sId));
            pContainer->SetEntryData(nPos, rUserData.back().get());
        }
    }
    if (o3tl::make_unsigned(nActiveIndex) < rItems.size())
        pContainer->SelectEntryPos(nActiveIndex);

    return true;
}

void BuilderBase::extractClassAndIdAndCustomProperty(xmlreader::XmlReader& reader, OUString& rClass,
                                                     OUString& rId, OUString& rCustomProperty)
{
    xmlreader::Span name;
    int nsId;

    while (reader.nextAttribute(&nsId, &name))
    {
        if (name == "class")
        {
            name = reader.getAttributeValue(false);
            rClass = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
        else if (name == "id")
        {
            name = reader.getAttributeValue(false);
            rId = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
            if (isLegacy())
            {
                sal_Int32 nDelim = rId.indexOf(':');
                if (nDelim != -1)
                {
                    rCustomProperty = rId.subView(nDelim + 1);
                    rId = rId.copy(0, nDelim);
                }
            }
        }
    }
}

Image BuilderBase::loadThemeImage(const OUString& rFileName)
{
    return Image(StockImage::Yes, rFileName);
}

void BuilderBase::handleInterfaceDomain(xmlreader::XmlReader& rReader)
{
    xmlreader::Span name = rReader.getAttributeValue(false);
    const OString sPrefixName(name.begin, name.length);
    m_pParserState->m_aResLocale = Translate::Create(sPrefixName);
}

BuilderBase::stringmap BuilderBase::collectPackingProperties(xmlreader::XmlReader& reader)
{
    int nLevel = 1;
    stringmap aPackingProperties;

    while (true)
    {
        xmlreader::Span name;
        int nsId;

        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            ++nLevel;
            if (name == "property")
                collectProperty(reader, aPackingProperties);
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }

    return aPackingProperties;
}

std::vector<vcl::EnumContext::Context> BuilderBase::handleStyle(xmlreader::XmlReader& reader,
                                                                int& nPriority)
{
    std::vector<vcl::EnumContext::Context> aContext;

    xmlreader::Span name;
    int nsId;

    int nLevel = 1;

    while (true)
    {
        xmlreader::XmlReader::Result res
            = reader.nextItem(xmlreader::XmlReader::Text::NONE, &name, &nsId);

        if (res == xmlreader::XmlReader::Result::Done)
            break;

        if (res == xmlreader::XmlReader::Result::Begin)
        {
            ++nLevel;
            if (name == "class")
            {
                OUString classStyle = getStyleClass(reader);
                std::u16string_view rest;

                if (classStyle.startsWith("context-", &rest))
                {
                    aContext.push_back(vcl::EnumContext::GetContextEnum(OUString(rest)));
                }
                else if (classStyle.startsWith("priority-", &rest))
                {
                    nPriority = o3tl::toInt32(rest);
                }
                else if (classStyle != "small-button" && classStyle != "destructive-action"
                         && classStyle != "suggested-action" && classStyle != "novertpad")
                {
                    SAL_WARN("vcl.builder", "unknown class: " << classStyle);
                }
            }
        }

        if (res == xmlreader::XmlReader::Result::End)
        {
            --nLevel;
        }

        if (!nLevel)
            break;
    }

    return aContext;
}

OUString BuilderBase::getStyleClass(xmlreader::XmlReader& reader)
{
    xmlreader::Span name;
    int nsId;
    OUString aRet;

    while (reader.nextAttribute(&nsId, &name))
    {
        if (name == "name")
        {
            name = reader.getAttributeValue(false);
            aRet = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
    }

    return aRet;
}

bool BuilderBase::hasOrientationVertical(VclBuilder::stringmap& rMap)
{
    bool bVertical = false;
    VclBuilder::stringmap::iterator aFind = rMap.find(u"orientation"_ustr);
    if (aFind != rMap.end())
    {
        bVertical = aFind->second.equalsIgnoreAsciiCase("vertical");
        rMap.erase(aFind);
    }
    return bVertical;
}

OUString BuilderBase::extractActionName(stringmap& rMap)
{
    return lcl_extractStringEntry(rMap, u"action-name"_ustr);
}

sal_Int32 BuilderBase::extractActive(VclBuilder::stringmap& rMap)
{
    sal_Int32 nActiveId = 0;
    VclBuilder::stringmap::iterator aFind = rMap.find(u"active"_ustr);
    if (aFind != rMap.end())
    {
        nActiveId = aFind->second.toInt32();
        rMap.erase(aFind);
    }
    return nActiveId;
}

bool BuilderBase::extractEntry(VclBuilder::stringmap& rMap)
{
    return lcl_extractBoolEntry(rMap, u"has-entry"_ustr, false);
}

OUString BuilderBase::extractGroup(stringmap& rMap)
{
    OUString sGroup = lcl_extractStringEntry(rMap, u"group"_ustr);
    sal_Int32 nDelim = sGroup.indexOf(':');
    if (nDelim != -1)
        sGroup = sGroup.copy(0, nDelim);

    return sGroup;
}

bool BuilderBase::extractHeadersVisible(VclBuilder::stringmap& rMap)
{
    return lcl_extractBoolEntry(rMap, u"headers-visible"_ustr, true);
}

OUString BuilderBase::extractIconName(VclBuilder::stringmap& rMap)
{
    OUString sIconName;
    // allow pixbuf, but prefer icon-name
    {
        VclBuilder::stringmap::iterator aFind = rMap.find(u"pixbuf"_ustr);
        if (aFind != rMap.end())
        {
            sIconName = aFind->second;
            rMap.erase(aFind);
        }
    }
    {
        VclBuilder::stringmap::iterator aFind = rMap.find(u"icon-name"_ustr);
        if (aFind != rMap.end())
        {
            sIconName = aFind->second;
            rMap.erase(aFind);
        }
    }
    if (sIconName == "missing-image")
        return OUString();
    OUString sReplace = mapStockToImageResource(sIconName);
    return !sReplace.isEmpty() ? sReplace : sIconName;
}

OUString BuilderBase::extractLabel(VclBuilder::stringmap& rMap)
{
    return lcl_extractStringEntry(rMap, u"label"_ustr);
}

OUString BuilderBase::extractPopupMenu(stringmap& rMap)
{
    return lcl_extractStringEntry(rMap, u"popup"_ustr);
}

bool BuilderBase::extractResizable(stringmap& rMap)
{
    return lcl_extractBoolEntry(rMap, u"resizable"_ustr, true);
}

bool BuilderBase::extractShowExpanders(VclBuilder::stringmap& rMap)
{
    return lcl_extractBoolEntry(rMap, u"show-expanders"_ustr, true);
}

OUString BuilderBase::extractTitle(VclBuilder::stringmap& rMap)
{
    return lcl_extractStringEntry(rMap, u"title"_ustr);
}

OUString BuilderBase::extractTooltipText(stringmap& rMap)
{
    OUString sTooltipText;
    VclBuilder::stringmap::iterator aFind = rMap.find(u"tooltip-text"_ustr);
    if (aFind == rMap.end())
        aFind = rMap.find(u"tooltip-markup"_ustr);
    if (aFind != rMap.end())
    {
        sTooltipText = aFind->second;
        rMap.erase(aFind);
    }
    return sTooltipText;
}

bool BuilderBase::extractVisible(VclBuilder::stringmap& rMap)
{
    return lcl_extractBoolEntry(rMap, u"visible"_ustr, false);
}

void BuilderBase::collectProperty(xmlreader::XmlReader& reader, stringmap& rMap) const
{
    xmlreader::Span name;
    int nsId;

    OUString sProperty;
    OString sContext;

    bool bTranslated = false;

    while (reader.nextAttribute(&nsId, &name))
    {
        if (name == "name")
        {
            name = reader.getAttributeValue(false);
            sProperty = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
        else if (name == "context")
        {
            name = reader.getAttributeValue(false);
            sContext = OString(name.begin, name.length);
        }
        else if (name == "translatable" && reader.getAttributeValue(false) == "yes")
        {
            bTranslated = true;
        }
    }

    (void)reader.nextItem(xmlreader::XmlReader::Text::Raw, &name, &nsId);

    if (!sProperty.isEmpty())
    {
        OString sValue(name.begin, name.length);
        const OUString sFinalValue = finalizeValue(sContext, sValue, bTranslated);
        sProperty = sProperty.replace('_', '-');
        rMap[sProperty] = sFinalValue;
    }
}

void BuilderBase::handleActionWidget(xmlreader::XmlReader& reader)
{
    xmlreader::Span name;
    int nsId;

    OString sResponse;

    while (reader.nextAttribute(&nsId, &name))
    {
        if (name == "response")
        {
            name = reader.getAttributeValue(false);
            sResponse = OString(name.begin, name.length);
        }
    }

    (void)reader.nextItem(xmlreader::XmlReader::Text::Raw, &name, &nsId);
    OUString sID(name.begin, name.length, RTL_TEXTENCODING_UTF8);
    sal_Int32 nDelim = sID.indexOf(':');
    if (nDelim != -1)
        sID = sID.copy(0, nDelim);

    int nResponse = sResponse.toInt32();
    switch (nResponse)
    {
        case -5:
            nResponse = RET_OK;
            break;
        case -6:
            nResponse = RET_CANCEL;
            break;
        case -7:
            nResponse = RET_CLOSE;
            break;
        case -8:
            nResponse = RET_YES;
            break;
        case -9:
            nResponse = RET_NO;
            break;
        case -11:
            nResponse = RET_HELP;
            break;
        case RET_RESET:
            break;
        default:
            assert(nResponse >= 100
                   && "keep non-canned responses in range 100+ to avoid collision with vcl RET_*");
            break;
    }

    set_response(sID, nResponse);
}

void BuilderBase::collectAccelerator(xmlreader::XmlReader& reader, accelmap& rMap)
{
    xmlreader::Span name;
    int nsId;

    OUString sProperty;
    OUString sValue;
    OUString sModifiers;

    while (reader.nextAttribute(&nsId, &name))
    {
        if (name == "key")
        {
            name = reader.getAttributeValue(false);
            sValue = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
        else if (name == "signal")
        {
            name = reader.getAttributeValue(false);
            sProperty = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
        else if (name == "modifiers")
        {
            name = reader.getAttributeValue(false);
            sModifiers = OUString(name.begin, name.length, RTL_TEXTENCODING_UTF8);
        }
    }

    if (!sProperty.isEmpty() && !sValue.isEmpty())
    {
        rMap[sProperty] = std::make_pair(sValue, sModifiers);
    }
}

VclButtonsType BuilderBase::mapGtkToVclButtonsType(std::u16string_view sGtkButtons)
{
    if (sGtkButtons == u"none")
        return VclButtonsType::NONE;
    if (sGtkButtons == u"ok")
        return VclButtonsType::Ok;
    if (sGtkButtons == u"cancel")
        return VclButtonsType::Cancel;
    if (sGtkButtons == u"close")
        return VclButtonsType::Close;
    else if (sGtkButtons == u"yes-no")
        return VclButtonsType::YesNo;
    else if (sGtkButtons == u"ok-cancel")
        return VclButtonsType::OkCancel;

    assert(false && "unknown buttons type mode");
    return VclButtonsType::NONE;
}

bool BuilderBase::isToolbarItemClass(std::u16string_view sClass)
{
    return sClass == u"GtkToolButton" || sClass == u"GtkMenuToolButton"
           || sClass == u"GtkToggleToolButton" || sClass == u"GtkRadioToolButton"
           || sClass == u"GtkToolItem";
}

void BuilderBase::addTextBuffer(const OUString& sID, TextBuffer&& rTextBuffer)
{
    m_pParserState->m_aTextBuffers[sID] = std::move(rTextBuffer);
}

const BuilderBase::TextBuffer* BuilderBase::get_buffer_by_name(const OUString& sID) const
{
    const auto aI = m_pParserState->m_aTextBuffers.find(sID);
    if (aI != m_pParserState->m_aTextBuffers.end())
        return &(aI->second);
    return nullptr;
}

void BuilderBase::addAdjustment(const OUString& sID, Adjustment&& rAdjustment)
{
    m_pParserState->m_aAdjustments[sID] = std::move(rAdjustment);
}

const BuilderBase::Adjustment* BuilderBase::get_adjustment_by_name(const OUString& sID) const
{
    const auto aI = m_pParserState->m_aAdjustments.find(sID);
    if (aI != m_pParserState->m_aAdjustments.end())
        return &(aI->second);
    return nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
