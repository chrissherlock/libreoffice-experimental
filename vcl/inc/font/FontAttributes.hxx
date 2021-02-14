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

#pragma once

#include <sal/log.hxx>
#include <rtl/ustring.hxx>
#include <tools/color.hxx>
#include <tools/fontenum.hxx>
#include <tools/gen.hxx>
#include <i18nlangtag/languagetag.hxx>

#include <vcl/fntstyle.hxx>

namespace vcl
{
class Font;
}

class VCL_DLLPUBLIC FontAttributes
{
public:
    explicit FontAttributes();
    FontAttributes(FontAttributes const&);

    // device independent font functions
    const OUString& GetFamilyName() const { return maFamilyName; }
    FontFamily GetFamilyType()
    {
        if (meFamily == FAMILY_DONTKNOW)
            AskConfig();
        return meFamily;
    }
    const OUString& GetStyleName() const { return maStyleName; }

    FontWeight GetWeight()
    {
        if (meWeight == WEIGHT_DONTKNOW)
            AskConfig();
        return meWeight;
    }
    FontItalic GetItalic()
    {
        if (meItalic == ITALIC_DONTKNOW)
            AskConfig();
        return meItalic;
    }
    FontPitch GetPitch()
    {
        if (mePitch == PITCH_DONTKNOW)
            AskConfig();
        return mePitch;
    }
    FontWidth GetWidthType()
    {
        if (meWidthType == WIDTH_DONTKNOW)
            AskConfig();
        return meWidthType;
    }
    TextAlign GetAlignment() const { return meAlign; }
    rtl_TextEncoding GetCharSet() const { return meCharSet; }
    const Size& GetFontSize() const { return maAverageFontSize; }

    bool IsSymbolFont() const { return mbSymbolFlag; }

    void SetFamilyName(const OUString& sFamilyName) { maFamilyName = sFamilyName; }
    void SetStyleName(const OUString& sStyleName) { maStyleName = sStyleName; }
    void SetFamilyType(const FontFamily eFontFamily) { meFamily = eFontFamily; }

    void SetPitch(const FontPitch ePitch) { mePitch = ePitch; }
    void SetItalic(const FontItalic eItalic) { meItalic = eItalic; }
    void SetWeight(const FontWeight eWeight) { meWeight = eWeight; }
    void SetWidthType(const FontWidth eWidthType) { meWidthType = eWidthType; }
    void SetAlignment(const TextAlign eAlignment) { meAlign = eAlignment; }
    void SetCharSet(const rtl_TextEncoding eCharSet) { meCharSet = eCharSet; }
    void SetFontSize(const Size& rSize) { maAverageFontSize = rSize; }

    void SetSymbolFlag(const bool bSymbolFlag);

    // straight properties, no getting them from AskConfig()
    FontFamily GetFamilyTypeNoAsk() const { return meFamily; }
    FontWeight GetWeightNoAsk() const { return meWeight; }
    FontItalic GetItalicNoAsk() const { return meItalic; }
    FontPitch GetPitchNoAsk() const { return mePitch; }
    FontWidth GetWidthTypeNoAsk() const { return meWidthType; }

    // device dependent functions
    int GetQuality() const { return mnQuality; }

    void SetQuality(int nQuality) { mnQuality = nQuality; }
    void IncreaseQualityBy(int nQualityAmount) { mnQuality += nQualityAmount; }
    void DecreaseQualityBy(int nQualityAmount) { mnQuality -= nQualityAmount; }

    bool CompareDeviceIndependentFontAttributes(const FontAttributes& rOther) const;

    const OUString& GetMapNames() const { return maMapNames; }
    void AddMapName(OUString const&);

    bool operator==(const FontAttributes&) const;

private:
    friend class vcl::Font;
    friend SvStream& ReadFontAttributes(SvStream& rIStm, FontAttributes&, tools::Long&);
    friend SvStream& WriteFontAttributes(SvStream& rOStm, const FontAttributes&,
                                         const tools::Long&);

    void AskConfig();

    // Device independent variables
    OUString maFamilyName;
    OUString maStyleName;
    FontWeight meWeight;
    FontFamily meFamily;
    FontPitch mePitch;
    FontWidth meWidthType;
    FontItalic meItalic;
    TextAlign meAlign;
    FontLineStyle meUnderline;
    FontLineStyle meOverline;
    FontStrikeout meStrikeout;
    FontRelief meRelief;
    FontEmphasisMark meEmphasisMark;
    FontKerning meKerning;
    Size maAverageFontSize;
    rtl_TextEncoding meCharSet;

    LanguageTag maLanguageTag;
    LanguageTag maCJKLanguageTag;

    // Flags - device independent
    bool mbSymbolFlag : 1, mbOutline : 1,
        mbConfigLookup : 1, // config lookup should only be done once
        mbShadow : 1, mbVertical : 1,
        mbTransparent : 1; // compatibility, now on output device

    // deprecated variables - device independent
    Color maColor; // compatibility, now on output device
    Color maFillColor; // compatibility, now on output device

    // Device dependent variables
    bool mbWordLine : 1;

    // TODO: metric data, should be migrated to FontAttributesMetric
    Degree10 mnOrientation;

    OUString maMapNames;
    int mnQuality;
};

inline void FontAttributes::SetSymbolFlag(const bool bSymbolFlag)
{
    mbSymbolFlag = bSymbolFlag;

    if (mbSymbolFlag)
    {
        meCharSet = RTL_TEXTENCODING_SYMBOL;
    }
    else
    {
        // if the symbol flag is unset, but it was a symbol font before then
        // until the character set encoding is set via SetCharSet then we
        // can't know what the characterset is!
        if (meCharSet == RTL_TEXTENCODING_SYMBOL)
        {
            meCharSet = RTL_TEXTENCODING_DONTKNOW;
        }
    }
}

inline void FontAttributes::AddMapName(OUString const& aMapName)
{
    if (maMapNames.getLength() > 0)
    {
        maMapNames += ";";
    }

    if (aMapName.getLength() == 0)
    {
        SAL_WARN("vcl.fonts", "New map name is empty");
        return;
    }

    maMapNames += aMapName;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
