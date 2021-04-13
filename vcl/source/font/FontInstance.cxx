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

#include <unotools/fontdefs.hxx>

#include <font/FontInstance.hxx>
#include <font/FontManager.hxx>
#include <font/FontFace.hxx>

#include <hb-ot.h>
#include <hb-graphite2.h>

// extend std namespace to add custom hash needed for FontInstance

namespace std
{
template <> struct hash<pair<sal_UCS4, FontWeight>>
{
    size_t operator()(const pair<sal_UCS4, FontWeight>& rData) const
    {
        std::size_t seed = 0;
        boost::hash_combine(seed, rData.first);
        boost::hash_combine(seed, rData.second);
        return seed;
    }
};
}

FontInstance::FontInstance(const FontFace& rFontFace, const FontSelectPattern& rFontSelData)
    : mxFontMetric(new ImplFontMetricData(rFontSelData))
    , mnLineHeight(0)
    , mnOwnOrientation(0)
    , mnOrientation(0)
    , mbInit(false)
    , mpConversion(nullptr)
    , mpFontManager(nullptr)
    , m_aFontSelData(rFontSelData)
    , m_pHbFont(nullptr)
    , m_nAveWidthFactor(1.0f)
    , m_pFontFace(&const_cast<FontFace&>(rFontFace))
{
}

FontInstance::~FontInstance()
{
    mpUnicodeFallbackList.reset();
    mpFontManager = nullptr;
    mxFontMetric = nullptr;

    if (m_pHbFont)
        hb_font_destroy(m_pHbFont);
}

hb_font_t* FontInstance::InitHbFont(hb_face_t* pHbFace)
{
    assert(pHbFace);
    hb_font_t* pHbFont = hb_font_create(pHbFace);
    unsigned int nUPEM = hb_face_get_upem(pHbFace);
    hb_font_set_scale(pHbFont, nUPEM, nUPEM);
    hb_ot_font_set_funcs(pHbFont);
    // hb_font_t keeps a reference to hb_face_t, so destroy this one.
    hb_face_destroy(pHbFace);
    return pHbFont;
}

int FontInstance::GetKashidaWidth()
{
    hb_font_t* pHbFont = GetHbFont();
    hb_position_t nWidth = 0;
    hb_codepoint_t nIndex = 0;

    if (hb_font_get_glyph(pHbFont, 0x0640, 0, &nIndex))
    {
        double nXScale = 0;
        GetScale(&nXScale, nullptr);
        nWidth = hb_font_get_glyph_h_advance(pHbFont, nIndex) * nXScale;
    }

    return nWidth;
}

void FontInstance::GetScale(double* nXScale, double* nYScale)
{
    hb_face_t* pHbFace = hb_font_get_face(GetHbFont());
    unsigned int nUPEM = hb_face_get_upem(pHbFace);

    double nHeight(m_aFontSelData.mnHeight);

    // On Windows, mnWidth is relative to average char width not font height,
    // and we need to keep it that way for GDI to correctly scale the glyphs.
    // Here we compensate for this so that HarfBuzz gives us the correct glyph
    // positions.
    double nWidth(m_aFontSelData.mnWidth ? m_aFontSelData.mnWidth * m_nAveWidthFactor : nHeight);

    if (nYScale)
        *nYScale = nHeight / nUPEM;

    if (nXScale)
        *nXScale = nWidth / nUPEM;
}

void FontInstance::AddFallbackForUnicode(sal_UCS4 cChar, FontWeight eWeight,
                                         const OUString& rFontName)
{
    if (!mpUnicodeFallbackList)
        mpUnicodeFallbackList.reset(new UnicodeFallbackList);
    (*mpUnicodeFallbackList)[std::pair<sal_UCS4, FontWeight>(cChar, eWeight)] = rFontName;
}

bool FontInstance::GetFallbackForUnicode(sal_UCS4 cChar, FontWeight eWeight,
                                         OUString* pFontName) const
{
    if (!mpUnicodeFallbackList)
        return false;

    UnicodeFallbackList::const_iterator it
        = mpUnicodeFallbackList->find(std::pair<sal_UCS4, FontWeight>(cChar, eWeight));
    if (it == mpUnicodeFallbackList->end())
        return false;

    *pFontName = (*it).second;
    return true;
}

void FontInstance::IgnoreFallbackForUnicode(sal_UCS4 cChar, FontWeight eWeight,
                                            std::u16string_view rFontName)
{
    UnicodeFallbackList::iterator it
        = mpUnicodeFallbackList->find(std::pair<sal_UCS4, FontWeight>(cChar, eWeight));
    if (it == mpUnicodeFallbackList->end())
        return;
    if ((*it).second == rFontName)
        mpUnicodeFallbackList->erase(it);
}

bool FontInstance::GetGlyphBoundRect(sal_GlyphId nID, tools::Rectangle& rRect, bool bVertical) const
{
    if (mpFontManager && mpFontManager->GetCachedGlyphBoundRect(this, nID, rRect))
        return true;

    bool res = ImplGetGlyphBoundRect(nID, rRect, bVertical);
    if (mpFontManager && res)
        mpFontManager->CacheGlyphBoundRect(this, nID, rRect);
    return res;
}

bool FontInstance::IsGraphiteFont()
{
    if (!m_xbIsGraphiteFont)
    {
        m_xbIsGraphiteFont
            = hb_graphite2_face_get_gr_face(hb_font_get_face(GetHbFont())) != nullptr;
    }
    return *m_xbIsGraphiteFont;
}

void FontInstance::SetAverageWidthFactor(double nFactor) { m_nAveWidthFactor = std::abs(nFactor); }

double FontInstance::GetAverageWidthFactor() const { return m_nAveWidthFactor; }

const FontSelectPattern& FontInstance::GetFontSelectPattern() const { return m_aFontSelData; }

const FontFace* FontInstance::GetFontFace() const { return m_pFontFace.get(); }

const FontManager* FontInstance::GetFontCache() const { return mpFontManager; }

FontFace* FontInstance::GetFontFace() { return m_pFontFace.get(); }

hb_font_t* FontInstance::ImplInitHbFont()
{
    assert(false);
    return hb_font_get_empty();
}

void FontInstance::InitConversion(ConvertChar const* pConvertChar) { mpConversion = pConvertChar; }

bool FontInstance::CanRecodeString()
{
    if (mpConversion)
        return true;

    return false;
}

void FontInstance::RecodeString(OUString& rString)
{
    if (mpConversion)
        mpConversion->RecodeString(rString, 0, rString.getLength());
}

tools::Long FontInstance::GetEmphasisHeight() const
{
    tools::Long nEmphasisHeight = (mnLineHeight * 250) / 1000;

    if (nEmphasisHeight < 1)
        nEmphasisHeight = 1;

    return nEmphasisHeight;
}

void FontInstance::SetEmphasisMarkStyle(FontEmphasisMark eEmphasisMark)
{
    tools::Long nEmphasisHeight = GetEmphasisHeight();

    if (eEmphasisMark & FontEmphasisMark::PosBelow)
    {
        mxFontMetric->SetEmphasisDescent(nEmphasisHeight);
        mxFontMetric->SetEmphasisAscent(0);
    }
    else
    {
        mxFontMetric->SetEmphasisDescent(0);
        mxFontMetric->SetEmphasisAscent(nEmphasisHeight);
    }
}

tools::Long FontInstance::GetEmphasisAscent() const { return mxFontMetric->GetEmphasisAscent(); }

tools::Long FontInstance::GetEmphasisDescent() const { return mxFontMetric->GetEmphasisDescent(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
