/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <unotools/fontcfg.hxx>
#include <unotools/fontdefs.hxx>

#include <vcl/dllapi.h>

#include <font/FontAttributes.hxx>

using namespace vcl;

FontAttributes::FontAttributes()
    : meWeight(WEIGHT_DONTKNOW)
    , meFamily(FAMILY_DONTKNOW)
    , mePitch(PITCH_DONTKNOW)
    , meWidthType(WIDTH_DONTKNOW)
    , meItalic(ITALIC_NONE)
    , meAlign(ALIGN_TOP)
    , meUnderline(LINESTYLE_NONE)
    , meOverline(LINESTYLE_NONE)
    , meStrikeout(STRIKEOUT_NONE)
    , meRelief(FontRelief::NONE)
    , meEmphasisMark(FontEmphasisMark::NONE)
    , meKerning(FontKerning::FontSpecific)
    , meCharSet(RTL_TEXTENCODING_DONTKNOW)
    , maLanguageTag(LANGUAGE_DONTKNOW)
    , maCJKLanguageTag(LANGUAGE_DONTKNOW)
    , mbSymbolFlag(false)
    , mbOutline(false)
    , mbConfigLookup(false)
    , mbShadow(false)
    , mbVertical(false)
    , mbTransparent(true)
    , maColor(COL_TRANSPARENT)
    , maFillColor(COL_TRANSPARENT)
    , mbWordLine(false)
    , mnOrientation(0)
    , mnQuality(0)
{
}

FontAttributes::FontAttributes(FontAttributes const& rFontAttributes)
    : maFamilyName(rFontAttributes.maFamilyName)
    , maStyleName(rFontAttributes.maStyleName)
    , meWeight(rFontAttributes.meWeight)
    , meFamily(rFontAttributes.meFamily)
    , mePitch(rFontAttributes.mePitch)
    , meWidthType(rFontAttributes.meWidthType)
    , meItalic(rFontAttributes.meItalic)
    , meAlign(rFontAttributes.meAlign)
    , meUnderline(rFontAttributes.meUnderline)
    , meOverline(rFontAttributes.meOverline)
    , meStrikeout(rFontAttributes.meStrikeout)
    , meRelief(rFontAttributes.meRelief)
    , meEmphasisMark(rFontAttributes.meEmphasisMark)
    , meKerning(rFontAttributes.meKerning)
    , maAverageFontSize(rFontAttributes.maAverageFontSize)
    , meCharSet(rFontAttributes.meCharSet)
    , maLanguageTag(rFontAttributes.maLanguageTag)
    , maCJKLanguageTag(rFontAttributes.maCJKLanguageTag)
    , mbSymbolFlag(rFontAttributes.mbSymbolFlag)
    , mbOutline(rFontAttributes.mbOutline)
    , mbConfigLookup(rFontAttributes.mbConfigLookup)
    , mbShadow(rFontAttributes.mbShadow)
    , mbVertical(rFontAttributes.mbVertical)
    , mbTransparent(rFontAttributes.mbTransparent)
    , maColor(rFontAttributes.maColor)
    , maFillColor(rFontAttributes.maFillColor)
    , mbWordLine(rFontAttributes.mbWordLine)
    , mnOrientation(rFontAttributes.mnOrientation)
    , mnQuality(rFontAttributes.mnQuality)
{
}

bool FontAttributes::operator==(const FontAttributes& rOther) const
{
    // equality tests split up for easier debugging
    if ((meWeight != rOther.meWeight) || (meItalic != rOther.meItalic)
        || (meFamily != rOther.meFamily) || (mePitch != rOther.mePitch))
        return false;

    if ((meCharSet != rOther.meCharSet) || (maLanguageTag != rOther.maLanguageTag)
        || (maCJKLanguageTag != rOther.maCJKLanguageTag) || (meAlign != rOther.meAlign))
        return false;

    if ((maAverageFontSize != rOther.maAverageFontSize) || (mnOrientation != rOther.mnOrientation)
        || (mbVertical != rOther.mbVertical))
        return false;

    if ((maFamilyName != rOther.maFamilyName) || (maStyleName != rOther.maStyleName))
        return false;

    if ((maColor != rOther.maColor) || (maFillColor != rOther.maFillColor))
        return false;

    if ((meUnderline != rOther.meUnderline) || (meOverline != rOther.meOverline)
        || (meStrikeout != rOther.meStrikeout) || (meRelief != rOther.meRelief)
        || (meEmphasisMark != rOther.meEmphasisMark) || (mbWordLine != rOther.mbWordLine)
        || (mbOutline != rOther.mbOutline) || (mbShadow != rOther.mbShadow)
        || (meKerning != rOther.meKerning) || (mbTransparent != rOther.mbTransparent))
        return false;

    return true;
}

void FontAttributes::AskConfig()
{
    if (mbConfigLookup)
        return;

    mbConfigLookup = true;

    // prepare the FontSubst configuration lookup
    const utl::FontSubstConfiguration& rFontSubst = utl::FontSubstConfiguration::get();

    OUString aShortName;
    OUString aFamilyName;
    ImplFontAttrs nType = ImplFontAttrs::None;
    FontWeight eWeight = WEIGHT_DONTKNOW;
    FontWidth eWidthType = WIDTH_DONTKNOW;
    OUString aMapName = GetEnglishSearchFontName(maFamilyName);

    utl::FontSubstConfiguration::getMapName(aMapName, aShortName, aFamilyName, eWeight, eWidthType,
                                            nType);

    // lookup the font name in the configuration
    const utl::FontNameAttr* pFontAttr = rFontSubst.getSubstInfo(aMapName);

    // if the direct lookup failed try again with an alias name
    if (!pFontAttr && (aShortName != aMapName))
        pFontAttr = rFontSubst.getSubstInfo(aShortName);

    if (pFontAttr)
    {
        // the font was found in the configuration
        if (meFamily == FAMILY_DONTKNOW)
        {
            if (pFontAttr->Type & ImplFontAttrs::Serif)
                meFamily = FAMILY_ROMAN;
            else if (pFontAttr->Type & ImplFontAttrs::SansSerif)
                meFamily = FAMILY_SWISS;
            else if (pFontAttr->Type & ImplFontAttrs::Typewriter)
                meFamily = FAMILY_MODERN;
            else if (pFontAttr->Type & ImplFontAttrs::Italic)
                meFamily = FAMILY_SCRIPT;
            else if (pFontAttr->Type & ImplFontAttrs::Decorative)
                meFamily = FAMILY_DECORATIVE;
        }

        if (mePitch == PITCH_DONTKNOW)
        {
            if (pFontAttr->Type & ImplFontAttrs::Fixed)
                mePitch = PITCH_FIXED;
        }
    }

    // if some attributes are still unknown then use the FontSubst magic
    if (meFamily == FAMILY_DONTKNOW)
    {
        if (nType & ImplFontAttrs::Serif)
            meFamily = FAMILY_ROMAN;
        else if (nType & ImplFontAttrs::SansSerif)
            meFamily = FAMILY_SWISS;
        else if (nType & ImplFontAttrs::Typewriter)
            meFamily = FAMILY_MODERN;
        else if (nType & ImplFontAttrs::Italic)
            meFamily = FAMILY_SCRIPT;
        else if (nType & ImplFontAttrs::Decorative)
            meFamily = FAMILY_DECORATIVE;
    }

    if (GetWeight() == WEIGHT_DONTKNOW)
        SetWeight(eWeight);
    if (meWidthType == WIDTH_DONTKNOW)
        meWidthType = eWidthType;
}

bool FontAttributes::CompareDeviceIndependentFontAttributes(const FontAttributes& rOther) const
{
    if (maFamilyName != rOther.maFamilyName)
        return false;

    if (maStyleName != rOther.maStyleName)
        return false;

    if (meWeight != rOther.meWeight)
        return false;

    if (meItalic != rOther.meItalic)
        return false;

    if (meFamily != rOther.meFamily)
        return false;

    if (mePitch != rOther.mePitch)
        return false;

    if (meWidthType != rOther.meWidthType)
        return false;

    if (mbSymbolFlag != rOther.mbSymbolFlag)
        return false;

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
