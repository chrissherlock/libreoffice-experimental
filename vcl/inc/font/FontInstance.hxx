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

#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <rtl/ref.hxx>
#include <salhelper/simplereferenceobject.hxx>
#include <tools/gen.hxx>
#include <tools/fontenum.hxx>
#include <tools/degree.hxx>

#include <vcl/glyphitem.hxx>

#include <font/FontSelectPattern.hxx>
#include <font/ImplFontMetricData.hxx>

#include <optional>
#include <unordered_map>
#include <memory>

#include <hb.h>

class ConvertChar;
class FontManager;
class FontFace;

// TODO: allow sharing of metrics for related fonts

class VCL_PLUGIN_PUBLIC FontInstance : public salhelper::SimpleReferenceObject
{
    // just declaring the factory function doesn't work AKA
    // friend FontInstance* FontFace::CreateFontInstance(const FontSelectPattern&) const;
    friend class FontFace;
    friend class FontManager;

public: // TODO: make data members private
    virtual ~FontInstance() override;

    // font instance attributes from the font request
    tools::Long GetWidth() const { return mxFontMetric->GetWidth(); }
    Degree10 GetOrientation() const { return mxFontMetric->GetOrientation(); }

    void SetWidth(tools::Long nWidth) { mxFontMetric->SetWidth(nWidth); }
    void SetOrientation(Degree10 nOrientation) { mxFontMetric->SetOrientation(nOrientation); }

    // font metrics measured for the font instance
    tools::Long GetAscent() const { return mxFontMetric->GetAscent(); }
    tools::Long GetDescent() const { return mxFontMetric->GetDescent(); }
    tools::Long GetInternalLeading() const { return mxFontMetric->GetInternalLeading(); }
    tools::Long GetExternalLeading() const { return mxFontMetric->GetExternalLeading(); }
    int GetSlant() const { return mxFontMetric->GetSlant(); }
    tools::Long GetMinKashida() const { return mxFontMetric->GetMinKashida(); }

    void SetSlant(int nSlant) { mxFontMetric->SetSlant(nSlant); }
    void SetMinKashida(tools::Long nMinKashida) { mxFontMetric->SetMinKashida(nMinKashida); }

    // font attributes queried from the font instance
    bool IsFullstopCentered() const { return mxFontMetric->IsFullstopCentered(); }
    tools::Long GetBulletOffset() const { return mxFontMetric->GetBulletOffset(); }

    void SetFullstopCenteredFlag(bool bFullstopCentered)
    {
        mxFontMetric->SetFullstopCenteredFlag(bFullstopCentered);
    }

    // font metrics that are usually derived from the measurements
    tools::Long GetUnderlineSize() const { return mxFontMetric->GetUnderlineSize(); }

    tools::Long GetUnderlineOffset() const { return mxFontMetric->GetUnderlineOffset(); }

    tools::Long GetBoldUnderlineSize() const { return mxFontMetric->GetBoldUnderlineSize(); }

    tools::Long GetBoldUnderlineOffset() const { return mxFontMetric->GetBoldUnderlineOffset(); }

    tools::Long GetDoubleUnderlineSize() const { return mxFontMetric->GetDoubleUnderlineSize(); }

    tools::Long GetDoubleUnderlineOffset1() const
    {
        return mxFontMetric->GetDoubleUnderlineOffset1();
    }

    tools::Long GetDoubleUnderlineOffset2() const
    {
        return mxFontMetric->GetDoubleUnderlineOffset2();
    }

    tools::Long GetWavelineUnderlineSize() const
    {
        return mxFontMetric->GetWavelineUnderlineSize();
    }

    tools::Long GetWavelineUnderlineOffset() const
    {
        return mxFontMetric->GetWavelineUnderlineOffset();
    }

    tools::Long GetAboveUnderlineSize() const { return mxFontMetric->GetAboveUnderlineSize(); }

    tools::Long GetAboveUnderlineOffset() const { return mxFontMetric->GetAboveUnderlineOffset(); }

    tools::Long GetAboveBoldUnderlineSize() const
    {
        return mxFontMetric->GetAboveBoldUnderlineSize();
    }

    tools::Long GetAboveBoldUnderlineOffset() const
    {
        return mxFontMetric->GetAboveBoldUnderlineOffset();
    }

    tools::Long GetAboveDoubleUnderlineSize() const
    {
        return mxFontMetric->GetAboveDoubleUnderlineSize();
    }

    tools::Long GetAboveDoubleUnderlineOffset1() const
    {
        return mxFontMetric->GetAboveDoubleUnderlineOffset1();
    }

    tools::Long GetAboveDoubleUnderlineOffset2() const
    {
        return mxFontMetric->GetAboveDoubleUnderlineOffset2();
    }

    tools::Long GetAboveWavelineUnderlineSize() const
    {
        return mxFontMetric->GetAboveWavelineUnderlineSize();
    }

    tools::Long GetAboveWavelineUnderlineOffset() const
    {
        return mxFontMetric->GetAboveWavelineUnderlineOffset();
    }

    tools::Long GetStrikeoutSize() const { return mxFontMetric->GetStrikeoutSize(); }

    tools::Long GetStrikeoutOffset() const { return mxFontMetric->GetStrikeoutOffset(); }

    tools::Long GetBoldStrikeoutSize() const { return mxFontMetric->GetBoldStrikeoutSize(); }

    tools::Long GetBoldStrikeoutOffset() const { return mxFontMetric->GetBoldStrikeoutOffset(); }

    tools::Long GetDoubleStrikeoutSize() const { return mxFontMetric->GetDoubleStrikeoutSize(); }

    tools::Long GetDoubleStrikeoutOffset1() const
    {
        return mxFontMetric->GetDoubleStrikeoutOffset1();
    }

    tools::Long GetDoubleStrikeoutOffset2() const
    {
        return mxFontMetric->GetDoubleStrikeoutOffset2();
    }

    void ImplInitTextLineSize(RenderContext2 const* pDev)
    {
        mxFontMetric->ImplInitTextLineSize(pDev);
    }

    void ImplInitAboveTextLineSize() { mxFontMetric->ImplInitAboveTextLineSize(); }

    void ImplInitFlags(RenderContext2 const* pDev) { mxFontMetric->ImplInitFlags(pDev); }

    void ImplCalcLineSpacing(FontInstance* pFontInstance)
    {
        mxFontMetric->ImplCalcLineSpacing(pFontInstance);
    }

    tools::Long GetLineHeight() const { return mnLineHeight; }
    void SetLineHeight(tools::Long nLineHeight) { mnLineHeight = nLineHeight; }
    Degree10 GetOwnOrientation() const { return mnOwnOrientation; }
    void SetOwnOrientation(Degree10 nOwnOrientation) { mnOwnOrientation = nOwnOrientation; }
    Degree10 GetTextAngle() const { return mnOrientation; }
    void SetTextAngle(Degree10 nOrientation) { mnOrientation = nOrientation; }
    bool IsInit() const { return mbInit; }
    void SetInitFlag(bool bInit) { mbInit = bInit; }

    tools::Long GetEmphasisAscent() const;
    tools::Long GetEmphasisDescent() const;

    void AddFallbackForUnicode(sal_UCS4, FontWeight eWeight, const OUString& rFontName);
    bool GetFallbackForUnicode(sal_UCS4, FontWeight eWeight, OUString* pFontName) const;
    void IgnoreFallbackForUnicode(sal_UCS4, FontWeight eWeight, std::u16string_view rFontName);

    inline hb_font_t* GetHbFont();
    bool IsGraphiteFont();
    void SetAverageWidthFactor(double nFactor);
    double GetAverageWidthFactor() const;
    const FontSelectPattern& GetFontSelectPattern() const;
    tools::Long GetEmphasisHeight() const;
    void SetEmphasisMarkStyle(FontEmphasisMark eEmphasisMark);

    const FontFace* GetFontFace() const;
    const FontManager* GetFontManager() const;
    FontFace* GetFontFace();
    const FontManager* GetFontCache() const;

    bool GetGlyphBoundRect(sal_GlyphId, tools::Rectangle&, bool) const;
    virtual bool GetGlyphOutline(sal_GlyphId, basegfx::B2DPolyPolygon&, bool) const = 0;

    int GetKashidaWidth();

    void GetScale(double* nXScale, double* nYScale);
    static inline void DecodeOpenTypeTag(const uint32_t nTableTag, char* pTagName);

    void InitConversion(ConvertChar const* pConvertChar);
    void RecodeString(OUString& rString);
    bool CanRecodeString();

    ImplFontMetricDataRef& GetFontMetricData() { return mxFontMetric; } // Font attributes
protected:
    explicit FontInstance(const FontFace&, const FontSelectPattern&);

    virtual bool ImplGetGlyphBoundRect(sal_GlyphId, tools::Rectangle&, bool) const = 0;

    // Takes ownership of pHbFace.
    static hb_font_t* InitHbFont(hb_face_t* pHbFace);
    virtual hb_font_t* ImplInitHbFont();

private:
    ImplFontMetricDataRef mxFontMetric; // Font attributes
    tools::Long mnLineHeight;
    Degree10 mnOwnOrientation; // text angle if lower layers don't rotate text themselves
    Degree10 mnOrientation; // text angle in 3600 system
    bool mbInit; // true if maFontMetric member is valid

    // cache of Unicode characters and replacement font names
    // TODO: a fallback map can be shared with many other ImplFontEntries
    // TODO: at least the ones which just differ in orientation, stretching or height
    ConvertChar const* mpConversion; // used e.g. for StarBats->StarSymbol
    typedef ::std::unordered_map<::std::pair<sal_UCS4, FontWeight>, OUString> UnicodeFallbackList;
    std::unique_ptr<UnicodeFallbackList> mpUnicodeFallbackList;
    mutable FontManager* mpFontManager;
    const FontSelectPattern m_aFontSelData;
    hb_font_t* m_pHbFont;
    double m_nAveWidthFactor;
    rtl::Reference<FontFace> m_pFontFace;
    std::optional<bool> m_xbIsGraphiteFont;
};

inline hb_font_t* FontInstance::GetHbFont()
{
    if (!m_pHbFont)
        m_pHbFont = ImplInitHbFont();
    return m_pHbFont;
}

inline void FontInstance::DecodeOpenTypeTag(const uint32_t nTableTag, char* pTagName)
{
    pTagName[0] = static_cast<char>(nTableTag >> 24);
    pTagName[1] = static_cast<char>(nTableTag >> 16);
    pTagName[2] = static_cast<char>(nTableTag >> 8);
    pTagName[3] = static_cast<char>(nTableTag);
    pTagName[4] = 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
