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

#include <i18nlangtag/languagetag.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <unotools/configmgr.hxx>
#include <unotools/fontdefs.hxx>
#include <o3tl/sorted_vector.hxx>

#include <outdev.h>
#include <font/FontSelectPattern.hxx>
#include <font/FontInstance.hxx>
#include <font/FontManager.hxx>
#include <font/FontFaceCollection.hxx>
#include <font/FontFaceSizeCollection.hxx>

#include <memory>

static ImplFontAttrs lcl_IsCJKFont(const OUString& rFontName)
{
    // Test, if Fontname includes CJK characters --> In this case we
    // mention that it is a CJK font
    for (int i = 0; i < rFontName.getLength(); i++)
    {
        const sal_Unicode ch = rFontName[i];
        // japanese
        if (((ch >= 0x3040) && (ch <= 0x30FF)) || ((ch >= 0x3190) && (ch <= 0x319F)))
            return ImplFontAttrs::CJK | ImplFontAttrs::CJK_JP;

        // korean
        if (((ch >= 0xAC00) && (ch <= 0xD7AF)) || ((ch >= 0xA960) && (ch <= 0xA97F))
            || ((ch >= 0xD7B0) && (ch <= 0xD7FF)) || ((ch >= 0x3130) && (ch <= 0x318F))
            || ((ch >= 0x1100) && (ch <= 0x11FF)))
            return ImplFontAttrs::CJK | ImplFontAttrs::CJK_KR;

        // chinese
        if ((ch >= 0x3400) && (ch <= 0x9FFF))
            return ImplFontAttrs::CJK | ImplFontAttrs::CJK_TC | ImplFontAttrs::CJK_SC;

        // cjk
        if (((ch >= 0x3000) && (ch <= 0xD7AF)) || ((ch >= 0xFF00) && (ch <= 0xFFEE)))
            return ImplFontAttrs::CJK;
    }

    return ImplFontAttrs::None;
}

FontManager::FontManager()
    : mbMatchData(false)
    , mpPreMatchHook(nullptr)
    , mpFallbackHook(nullptr)
    , mnFallbackCount(-1)
    , mpLastHitCacheEntry(nullptr)
    , maFontInstanceList(0)
    // The cache limit is set by the rough number of characters needed to read your average Asian newspaper.
    , m_aBoundRectCache(3000)
{
}

FontManager::~FontManager() { Clear(); }

void FontManager::SetPreMatchHook(ImplPreMatchFontSubstitution* pHook) { mpPreMatchHook = pHook; }

void FontManager::SetFallbackHook(ImplGlyphFallbackFontSubstitution* pHook)
{
    mpFallbackHook = pHook;
}

void FontManager::Clear()
{
    // remove fallback lists
    mpFallbackList.reset();
    mnFallbackCount = -1;

    // clear all entries in the device font list
    maFontFamilies.clear();

    // match data must be recalculated too
    mbMatchData = false;
}

void FontManager::ImplInitGenericGlyphFallback() const
{
    // normalized family names of fonts suited for glyph fallback
    // if a font is available related fonts can be ignored
    // TODO: implement dynamic lists
    static const char* aGlyphFallbackList[]
        = { // empty strings separate the names of unrelated fonts
            "eudc",
            "",
            "arialunicodems",
            "cyberbit",
            "code2000",
            "",
            "andalesansui",
            "",
            "starsymbol",
            "opensymbol",
            "",
            "msmincho",
            "fzmingti",
            "fzheiti",
            "ipamincho",
            "sazanamimincho",
            "kochimincho",
            "",
            "sunbatang",
            "sundotum",
            "baekmukdotum",
            "gulim",
            "batang",
            "dotum",
            "",
            "hgmincholightj",
            "msunglightsc",
            "msunglighttc",
            "hymyeongjolightk",
            "",
            "tahoma",
            "dejavusans",
            "timesnewroman",
            "liberationsans",
            "",
            "shree",
            "mangal",
            "",
            "raavi",
            "shruti",
            "tunga",
            "",
            "latha",
            "gautami",
            "kartika",
            "vrinda",
            "",
            "shayyalmt",
            "naskmt",
            "scheherazade",
            "",
            "david",
            "nachlieli",
            "lucidagrande",
            "",
            "norasi",
            "angsanaupc",
            "",
            "khmerossystem",
            "",
            "muktinarrow",
            "",
            "phetsarathot",
            "",
            "padauk",
            "pinlonmyanmar",
            "",
            "iskoolapota",
            "lklug",
            "",
            nullptr
          };

    bool bHasEudc = false;
    int nMaxLevel = 0;
    int nBestQuality = 0;
    std::unique_ptr<std::array<FontFamily*, MAX_GLYPHFALLBACK>> pFallbackList;

    for (const char** ppNames = &aGlyphFallbackList[0];; ++ppNames)
    {
        // advance to next sub-list when end-of-sublist marker
        if (!**ppNames) // #i46456# check for empty string, i.e., deref string itself not only ptr to it
        {
            if (nBestQuality > 0)
                if (++nMaxLevel >= MAX_GLYPHFALLBACK)
                    break;

            if (!ppNames[1])
                break;

            nBestQuality = 0;
            continue;
        }

        // test if the glyph fallback candidate font is available and scalable
        OUString aTokenName(*ppNames, strlen(*ppNames), RTL_TEXTENCODING_UTF8);
        FontFamily* pFallbackFont = FindFontFamily(aTokenName);

        if (!pFallbackFont)
            continue;

        // keep the best font of the glyph fallback sub-list
        if (nBestQuality < pFallbackFont->GetMinQuality())
        {
            nBestQuality = pFallbackFont->GetMinQuality();
            // store available glyph fallback fonts
            if (!pFallbackList)
                pFallbackList.reset(new std::array<FontFamily*, MAX_GLYPHFALLBACK>);

            (*pFallbackList)[nMaxLevel] = pFallbackFont;
            if (!bHasEudc && !nMaxLevel)
                bHasEudc = !strncmp(*ppNames, "eudc", 5);
        }
    }

    mnFallbackCount = nMaxLevel;
    mpFallbackList = std::move(pFallbackList);
}

FontFamily* FontManager::GetGlyphFallbackFontFamily(FontSelectPattern& rFontSelData,
                                                    FontInstance* pFontInstance,
                                                    OUString& rMissingCodes,
                                                    int nFallbackLevel) const
{
    FontFamily* pFallbackData = nullptr;

    // find a matching font candidate for platform specific glyph fallback
    if (mpFallbackHook)
    {
        // check cache for the first matching entry
        // to avoid calling the expensive fallback hook (#i83491#)
        sal_UCS4 cChar = 0;
        bool bCached = true;
        sal_Int32 nStrIndex = 0;
        while (nStrIndex < rMissingCodes.getLength())
        {
            cChar = rMissingCodes.iterateCodePoints(&nStrIndex);
            bCached = pFontInstance->GetFallbackForUnicode(cChar, rFontSelData.GetWeight(),
                                                           &rFontSelData.maSearchName);

            // ignore entries which don't have a fallback
            if (!bCached || !rFontSelData.maSearchName.isEmpty())
                break;
        }

        if (bCached)
        {
            // there is a matching fallback in the cache
            // so update rMissingCodes with codepoints not yet resolved by this fallback
            int nRemainingLength = 0;
            std::unique_ptr<sal_UCS4[]> const pRemainingCodes(
                new sal_UCS4[rMissingCodes.getLength()]);
            OUString aFontName;

            while (nStrIndex < rMissingCodes.getLength())
            {
                cChar = rMissingCodes.iterateCodePoints(&nStrIndex);
                bCached = pFontInstance->GetFallbackForUnicode(cChar, rFontSelData.GetWeight(),
                                                               &aFontName);
                if (!bCached || (rFontSelData.maSearchName != aFontName))
                    pRemainingCodes[nRemainingLength++] = cChar;
            }
            rMissingCodes = OUString(pRemainingCodes.get(), nRemainingLength);
        }
        else
        {
            OUString aOldMissingCodes = rMissingCodes;

            // call the hook to query the best matching glyph fallback font
            if (mpFallbackHook->FindFontSubstitute(rFontSelData, pFontInstance, rMissingCodes))
                // apply outdev3.cxx specific fontname normalization
                rFontSelData.maSearchName = GetEnglishSearchFontName(rFontSelData.maSearchName);
            else
                rFontSelData.maSearchName.clear();

            // See fdo#32665 for an example. FreeSerif that has glyphs in normal
            // font, but not in the italic or bold version
            bool bSubSetOfFontRequiresPropertyFaking
                = rFontSelData.mbEmbolden || rFontSelData.maItalicMatrix != ItalicMatrix();

            // Cache the result even if there was no match, unless its from part of a font for which the properties need
            // to be faked. We need to rework this cache to take into account that fontconfig can return different fonts
            // for different input sizes, weights, etc. Basically the cache is way to naive
            if (!bSubSetOfFontRequiresPropertyFaking)
            {
                for (;;)
                {
                    if (!pFontInstance->GetFallbackForUnicode(cChar, rFontSelData.GetWeight(),
                                                              &rFontSelData.maSearchName))
                        pFontInstance->AddFallbackForUnicode(cChar, rFontSelData.GetWeight(),
                                                             rFontSelData.maSearchName);
                    if (nStrIndex >= aOldMissingCodes.getLength())
                        break;
                    cChar = aOldMissingCodes.iterateCodePoints(&nStrIndex);
                }
                if (!rFontSelData.maSearchName.isEmpty())
                {
                    // remove cache entries that were still not resolved
                    for (nStrIndex = 0; nStrIndex < rMissingCodes.getLength();)
                    {
                        cChar = rMissingCodes.iterateCodePoints(&nStrIndex);
                        pFontInstance->IgnoreFallbackForUnicode(cChar, rFontSelData.GetWeight(),
                                                                rFontSelData.maSearchName);
                    }
                }
            }
        }

        // find the matching device font
        if (!rFontSelData.maSearchName.isEmpty())
            pFallbackData = FindFontFamily(rFontSelData.maSearchName);
    }

    // else find a matching font candidate for generic glyph fallback
    if (!pFallbackData)
    {
        // initialize font candidates for generic glyph fallback if needed
        if (mnFallbackCount < 0)
            ImplInitGenericGlyphFallback();

        // TODO: adjust nFallbackLevel by number of levels resolved by the fallback hook
        if (nFallbackLevel < mnFallbackCount)
            pFallbackData = (*mpFallbackList)[nFallbackLevel];
    }

    return pFallbackData;
}

void FontManager::Add(FontFace* pNewData)
{
    OUString aSearchName = GetEnglishSearchFontName(pNewData->GetFamilyName());

    FontFamily* pFoundData = FindOrCreateFontFamily(aSearchName);

    pFoundData->AddFontFace(pNewData);
}

// find the font from the normalized font family name
FontFamily* FontManager::ImplFindFontFamilyBySearchName(const OUString& rSearchName) const
{
    // must be called with a normalized name.
    assert(GetEnglishSearchFontName(rSearchName) == rSearchName);

    FontFamilies::const_iterator it = maFontFamilies.find(rSearchName);
    if (it == maFontFamilies.end())
        return nullptr;

    FontFamily* pFoundData = (*it).second.get();
    return pFoundData;
}

FontFamily* FontManager::FindFontFamily(const OUString& rFontName) const
{
    return ImplFindFontFamilyBySearchName(GetEnglishSearchFontName(rFontName));
}

FontFamily* FontManager::FindOrCreateFontFamily(const OUString& rFamilyName)
{
    FontFamilies::const_iterator it = maFontFamilies.find(rFamilyName);
    FontFamily* pFoundData = nullptr;

    if (it != maFontFamilies.end())
        pFoundData = (*it).second.get();

    if (!pFoundData)
    {
        pFoundData = new FontFamily(rFamilyName);
        maFontFamilies[rFamilyName].reset(pFoundData);
    }

    return pFoundData;
}

FontFamily* FontManager::FindFontFamilyByTokenNames(const OUString& rTokenStr) const
{
    FontFamily* pFoundData = nullptr;

    // use normalized font name tokens to find the font
    for (sal_Int32 nTokenPos = 0; nTokenPos != -1;)
    {
        OUString aFamilyName = GetNextFontToken(rTokenStr, nTokenPos);
        if (aFamilyName.isEmpty())
            continue;

        pFoundData = FindFontFamily(aFamilyName);

        if (pFoundData)
            break;
    }

    return pFoundData;
}

FontFamily* FontManager::ImplFindFontFamilyBySubstFontAttr(const utl::FontNameAttr& rFontAttr) const
{
    FontFamily* pFoundData = nullptr;

    // use the font substitutions suggested by the FontNameAttr to find the font
    for (auto const& substitution : rFontAttr.Substitutions)
    {
        pFoundData = FindFontFamily(substitution);
        if (pFoundData)
            return pFoundData;
    }

    // use known attributes from the configuration to find a matching substitute
    const ImplFontAttrs nSearchType = rFontAttr.Type;
    if (nSearchType != ImplFontAttrs::None)
    {
        const FontWeight eSearchWeight = rFontAttr.Weight;
        const FontWidth eSearchWidth = rFontAttr.Width;
        const FontItalic eSearchSlant = ITALIC_DONTKNOW;

        pFoundData = FindFontFamilyByAttributes(nSearchType, eSearchWeight, eSearchWidth,
                                                eSearchSlant, "");

        if (pFoundData)
            return pFoundData;
    }

    return nullptr;
}

void FontManager::ImplInitMatchData() const
{
    // short circuit if already done
    if (mbMatchData)
        return;
    mbMatchData = true;

    if (utl::ConfigManager::IsFuzzing())
        return;

    // calculate MatchData for all entries
    const utl::FontSubstConfiguration& rFontSubst = utl::FontSubstConfiguration::get();

    for (auto const& family : maFontFamilies)
    {
        const OUString& rSearchName = family.first;
        FontFamily* pEntry = family.second.get();

        pEntry->InitMatchData(rFontSubst, rSearchName);
    }
}

FontFamily* FontManager::FindFontFamilyByAttributes(ImplFontAttrs nSearchType,
                                                    FontWeight eSearchWeight,
                                                    FontWidth eSearchWidth,
                                                    FontItalic eSearchItalic,
                                                    const OUString& rSearchFamilyName) const
{
    if ((eSearchItalic != ITALIC_NONE) && (eSearchItalic != ITALIC_DONTKNOW))
        nSearchType |= ImplFontAttrs::Italic;

    // don't bother to match attributes if the attributes aren't worth matching
    if (nSearchType == ImplFontAttrs::None
        && ((eSearchWeight == WEIGHT_DONTKNOW) || (eSearchWeight == WEIGHT_NORMAL))
        && ((eSearchWidth == WIDTH_DONTKNOW) || (eSearchWidth == WIDTH_NORMAL)))
        return nullptr;

    ImplInitMatchData();
    FontFamily* pFoundData = nullptr;

    tools::Long nBestMatch = 40000;
    ImplFontAttrs nBestType = ImplFontAttrs::None;

    for (auto const& family : maFontFamilies)
    {
        FontFamily* pData = family.second.get();

        // Get all information about the matching font
        ImplFontAttrs nMatchType = pData->GetMatchType();
        FontWeight eMatchWeight = pData->GetMatchWeight();
        FontWidth eMatchWidth = pData->GetMatchWidth();

        // Calculate Match Value
        // 1000000000
        //  100000000
        //   10000000   CJK, CTL, None-Latin, Symbol
        //    1000000   FamilyName, Script, Fixed, -Special, -Decorative,
        //              Titling, Capitals, Outline, Shadow
        //     100000   Match FamilyName, Serif, SansSerif, Italic,
        //              Width, Weight
        //      10000   Scalable, Standard, Default,
        //              full, Normal, Knownfont,
        //              Otherstyle, +Special, +Decorative,
        //       1000   Typewriter, Rounded, Gothic, Schollbook
        //        100
        tools::Long nTestMatch = 0;

        // test CJK script attributes
        if (nSearchType & ImplFontAttrs::CJK)
        {
            // Matching language
            if (ImplFontAttrs::None == ((nSearchType ^ nMatchType) & ImplFontAttrs::CJK_AllLang))
                nTestMatch += 10000000 * 3;
            if (nMatchType & ImplFontAttrs::CJK)
                nTestMatch += 10000000 * 2;
            if (nMatchType & ImplFontAttrs::Full)
                nTestMatch += 10000000;
        }
        else if (nMatchType & ImplFontAttrs::CJK)
        {
            nTestMatch -= 10000000;
        }

        // test CTL script attributes
        if (nSearchType & ImplFontAttrs::CTL)
        {
            if (nMatchType & ImplFontAttrs::CTL)
                nTestMatch += 10000000 * 2;
            if (nMatchType & ImplFontAttrs::Full)
                nTestMatch += 10000000;
        }
        else if (nMatchType & ImplFontAttrs::CTL)
        {
            nTestMatch -= 10000000;
        }

        // test LATIN script attributes
        if (nSearchType & ImplFontAttrs::NoneLatin)
        {
            if (nMatchType & ImplFontAttrs::NoneLatin)
                nTestMatch += 10000000 * 2;
            if (nMatchType & ImplFontAttrs::Full)
                nTestMatch += 10000000;
        }

        // test SYMBOL attributes
        if (nSearchType & ImplFontAttrs::Symbol)
        {
            const OUString& rSearchName = family.first;
            // prefer some special known symbol fonts
            if (rSearchName == "starsymbol")
            {
                nTestMatch += 10000000 * 6 + (10000 * 3);
            }
            else if (rSearchName == "opensymbol")
            {
                nTestMatch += 10000000 * 6;
            }
            else if (rSearchName == "starbats" || rSearchName == "wingdings"
                     || rSearchName == "monotypesorts" || rSearchName == "dingbats"
                     || rSearchName == "zapfdingbats")
            {
                nTestMatch += 10000000 * 5;
            }
            else if (pData->GetTypeFaces() & FontTypeFaces::Symbol)
            {
                nTestMatch += 10000000 * 4;
            }
            else
            {
                if (nMatchType & ImplFontAttrs::Symbol)
                    nTestMatch += 10000000 * 2;
                if (nMatchType & ImplFontAttrs::Full)
                    nTestMatch += 10000000;
            }
        }
        else if ((pData->GetTypeFaces() & (FontTypeFaces::Symbol | FontTypeFaces::NoneSymbol))
                 == FontTypeFaces::Symbol)
        {
            nTestMatch -= 10000000;
        }
        else if (nMatchType & ImplFontAttrs::Symbol)
        {
            nTestMatch -= 10000;
        }

        // match stripped family name
        if (!rSearchFamilyName.isEmpty() && (rSearchFamilyName == pData->GetMatchFamilyName()))
        {
            nTestMatch += 1000000 * 3;
        }

        // match ALLSCRIPT? attribute
        if (nSearchType & ImplFontAttrs::AllScript)
        {
            if (nMatchType & ImplFontAttrs::AllScript)
            {
                nTestMatch += 1000000 * 2;
            }
            if (nSearchType & ImplFontAttrs::AllSubscript)
            {
                if (ImplFontAttrs::None
                    == ((nSearchType ^ nMatchType) & ImplFontAttrs::AllSubscript))
                    nTestMatch += 1000000 * 2;
                if (ImplFontAttrs::None
                    != ((nSearchType ^ nMatchType) & ImplFontAttrs::BrushScript))
                    nTestMatch -= 1000000;
            }
        }
        else if (nMatchType & ImplFontAttrs::AllScript)
        {
            nTestMatch -= 1000000;
        }

        // test MONOSPACE+TYPEWRITER attributes
        if (nSearchType & ImplFontAttrs::Fixed)
        {
            if (nMatchType & ImplFontAttrs::Fixed)
                nTestMatch += 1000000 * 2;
            // a typewriter attribute is even better
            if (ImplFontAttrs::None == ((nSearchType ^ nMatchType) & ImplFontAttrs::Typewriter))
                nTestMatch += 10000 * 2;
        }
        else if (nMatchType & ImplFontAttrs::Fixed)
        {
            nTestMatch -= 1000000;
        }

        // test SPECIAL attribute
        if (nSearchType & ImplFontAttrs::Special)
        {
            if (nMatchType & ImplFontAttrs::Special)
            {
                nTestMatch += 10000;
            }
            else if (!(nSearchType & ImplFontAttrs::AllSerifStyle))
            {
                if (nMatchType & ImplFontAttrs::Serif)
                {
                    nTestMatch += 1000 * 2;
                }
                else if (nMatchType & ImplFontAttrs::SansSerif)
                {
                    nTestMatch += 1000;
                }
            }
        }
        else if ((nMatchType & ImplFontAttrs::Special) && !(nSearchType & ImplFontAttrs::Symbol))
        {
            nTestMatch -= 1000000;
        }

        // test DECORATIVE attribute
        if (nSearchType & ImplFontAttrs::Decorative)
        {
            if (nMatchType & ImplFontAttrs::Decorative)
            {
                nTestMatch += 10000;
            }
            else if (!(nSearchType & ImplFontAttrs::AllSerifStyle))
            {
                if (nMatchType & ImplFontAttrs::Serif)
                    nTestMatch += 1000 * 2;
                else if (nMatchType & ImplFontAttrs::SansSerif)
                    nTestMatch += 1000;
            }
        }
        else if (nMatchType & ImplFontAttrs::Decorative)
        {
            nTestMatch -= 1000000;
        }

        // test TITLE+CAPITALS attributes
        if (nSearchType & (ImplFontAttrs::Titling | ImplFontAttrs::Capitals))
        {
            if (nMatchType & (ImplFontAttrs::Titling | ImplFontAttrs::Capitals))
            {
                nTestMatch += 1000000 * 2;
            }
            if (ImplFontAttrs::None
                == ((nSearchType ^ nMatchType)
                    & ImplFontAttrs(ImplFontAttrs::Titling | ImplFontAttrs::Capitals)))
            {
                nTestMatch += 1000000;
            }
            else if ((nMatchType & (ImplFontAttrs::Titling | ImplFontAttrs::Capitals))
                     && (nMatchType & (ImplFontAttrs::Standard | ImplFontAttrs::Default)))
            {
                nTestMatch += 1000000;
            }
        }
        else if (nMatchType & (ImplFontAttrs::Titling | ImplFontAttrs::Capitals))
        {
            nTestMatch -= 1000000;
        }

        // test OUTLINE+SHADOW attributes
        if (nSearchType & (ImplFontAttrs::Outline | ImplFontAttrs::Shadow))
        {
            if (nMatchType & (ImplFontAttrs::Outline | ImplFontAttrs::Shadow))
            {
                nTestMatch += 1000000 * 2;
            }
            if (ImplFontAttrs::None
                == ((nSearchType ^ nMatchType)
                    & ImplFontAttrs(ImplFontAttrs::Outline | ImplFontAttrs::Shadow)))
            {
                nTestMatch += 1000000;
            }
            else if ((nMatchType & (ImplFontAttrs::Outline | ImplFontAttrs::Shadow))
                     && (nMatchType & (ImplFontAttrs::Standard | ImplFontAttrs::Default)))
            {
                nTestMatch += 1000000;
            }
        }
        else if (nMatchType & (ImplFontAttrs::Outline | ImplFontAttrs::Shadow))
        {
            nTestMatch -= 1000000;
        }

        // test font name substrings
        // TODO: calculate name matching score using e.g. Levenstein distance
        if ((rSearchFamilyName.getLength() >= 4) && (pData->GetMatchFamilyName().getLength() >= 4)
            && ((rSearchFamilyName.indexOf(pData->GetMatchFamilyName()) != -1)
                || (pData->GetMatchFamilyName().indexOf(rSearchFamilyName) != -1)))
        {
            nTestMatch += 5000;
        }
        // test SERIF attribute
        if (nSearchType & ImplFontAttrs::Serif)
        {
            if (nMatchType & ImplFontAttrs::Serif)
                nTestMatch += 1000000 * 2;
            else if (nMatchType & ImplFontAttrs::SansSerif)
                nTestMatch -= 1000000;
        }

        // test SANSERIF attribute
        if (nSearchType & ImplFontAttrs::SansSerif)
        {
            if (nMatchType & ImplFontAttrs::SansSerif)
                nTestMatch += 1000000;
            else if (nMatchType & ImplFontAttrs::Serif)
                nTestMatch -= 1000000;
        }

        // test ITALIC attribute
        if (nSearchType & ImplFontAttrs::Italic)
        {
            if (pData->GetTypeFaces() & FontTypeFaces::Italic)
                nTestMatch += 1000000 * 3;
            if (nMatchType & ImplFontAttrs::Italic)
                nTestMatch += 1000000;
        }
        else if (!(nSearchType & ImplFontAttrs::AllScript)
                 && ((nMatchType & ImplFontAttrs::Italic)
                     || !(pData->GetTypeFaces() & FontTypeFaces::NoneItalic)))
        {
            nTestMatch -= 1000000 * 2;
        }

        // test WIDTH attribute
        if ((eSearchWidth != WIDTH_DONTKNOW) && (eSearchWidth != WIDTH_NORMAL))
        {
            if (eSearchWidth < WIDTH_NORMAL)
            {
                if (eSearchWidth == eMatchWidth)
                    nTestMatch += 1000000 * 3;
                else if ((eMatchWidth < WIDTH_NORMAL) && (eMatchWidth != WIDTH_DONTKNOW))
                    nTestMatch += 1000000;
            }
            else
            {
                if (eSearchWidth == eMatchWidth)
                    nTestMatch += 1000000 * 3;
                else if (eMatchWidth > WIDTH_NORMAL)
                    nTestMatch += 1000000;
            }
        }
        else if ((eMatchWidth != WIDTH_DONTKNOW) && (eMatchWidth != WIDTH_NORMAL))
        {
            nTestMatch -= 1000000;
        }

        // test WEIGHT attribute
        if ((eSearchWeight != WEIGHT_DONTKNOW) && (eSearchWeight != WEIGHT_NORMAL)
            && (eSearchWeight != WEIGHT_MEDIUM))
        {
            if (eSearchWeight < WEIGHT_NORMAL)
            {
                if (pData->GetTypeFaces() & FontTypeFaces::Light)
                    nTestMatch += 1000000;
                if ((eMatchWeight < WEIGHT_NORMAL) && (eMatchWeight != WEIGHT_DONTKNOW))
                    nTestMatch += 1000000;
            }
            else
            {
                if (pData->GetTypeFaces() & FontTypeFaces::Bold)
                    nTestMatch += 1000000;
                if (eMatchWeight > WEIGHT_BOLD)
                    nTestMatch += 1000000;
            }
        }
        else if (((eMatchWeight != WEIGHT_DONTKNOW) && (eMatchWeight != WEIGHT_NORMAL)
                  && (eMatchWeight != WEIGHT_MEDIUM))
                 || !(pData->GetTypeFaces() & FontTypeFaces::Normal))
        {
            nTestMatch -= 1000000;
        }

        // prefer scalable fonts
        if (pData->GetTypeFaces() & FontTypeFaces::Scalable)
            nTestMatch += 10000 * 4;
        else
            nTestMatch -= 10000 * 4;

        // test STANDARD+DEFAULT+FULL+NORMAL attributes
        if (nMatchType & ImplFontAttrs::Standard)
            nTestMatch += 10000 * 2;
        if (nMatchType & ImplFontAttrs::Default)
            nTestMatch += 10000;
        if (nMatchType & ImplFontAttrs::Full)
            nTestMatch += 10000;
        if (nMatchType & ImplFontAttrs::Normal)
            nTestMatch += 10000;

        // test OTHERSTYLE attribute
        if (((nSearchType ^ nMatchType) & ImplFontAttrs::OtherStyle) != ImplFontAttrs::None)
        {
            nTestMatch -= 10000;
        }

        // test ROUNDED attribute
        if (ImplFontAttrs::None == ((nSearchType ^ nMatchType) & ImplFontAttrs::Rounded))
            nTestMatch += 1000;

        // test TYPEWRITER attribute
        if (ImplFontAttrs::None == ((nSearchType ^ nMatchType) & ImplFontAttrs::Typewriter))
            nTestMatch += 1000;

        // test GOTHIC attribute
        if (nSearchType & ImplFontAttrs::Gothic)
        {
            if (nMatchType & ImplFontAttrs::Gothic)
                nTestMatch += 1000 * 3;
            if (nMatchType & ImplFontAttrs::SansSerif)
                nTestMatch += 1000 * 2;
        }

        // test SCHOOLBOOK attribute
        if (nSearchType & ImplFontAttrs::Schoolbook)
        {
            if (nMatchType & ImplFontAttrs::Schoolbook)
                nTestMatch += 1000 * 3;
            if (nMatchType & ImplFontAttrs::Serif)
                nTestMatch += 1000 * 2;
        }

        // compare with best matching font yet
        if (nTestMatch > nBestMatch)
        {
            pFoundData = pData;
            nBestMatch = nTestMatch;
            nBestType = nMatchType;
        }
        else if (nTestMatch == nBestMatch)
        {
            // some fonts are more suitable defaults
            if (nMatchType & ImplFontAttrs::Default)
            {
                pFoundData = pData;
                nBestType = nMatchType;
            }
            else if ((nMatchType & ImplFontAttrs::Standard)
                     && !(nBestType & ImplFontAttrs::Default))
            {
                pFoundData = pData;
                nBestType = nMatchType;
            }
        }
    }

    return pFoundData;
}

FontFamily* FontManager::ImplFindFontFamilyOfDefaultFont() const
{
    // try to find one of the default fonts of the
    // UNICODE, SANSSERIF, SERIF or FIXED default font lists
    FontFamily* pFoundData = nullptr;
    if (!utl::ConfigManager::IsFuzzing())
    {
        const utl::DefaultFontConfiguration& rDefaults = utl::DefaultFontConfiguration::get();
        LanguageTag aLanguageTag("en");
        OUString aFontname = rDefaults.getDefaultFont(aLanguageTag, DefaultFontType::SANS_UNICODE);
        pFoundData = FindFontFamilyByTokenNames(aFontname);

        if (pFoundData)
            return pFoundData;

        aFontname = rDefaults.getDefaultFont(aLanguageTag, DefaultFontType::SANS);
        pFoundData = FindFontFamilyByTokenNames(aFontname);
        if (pFoundData)
            return pFoundData;

        aFontname = rDefaults.getDefaultFont(aLanguageTag, DefaultFontType::SERIF);
        pFoundData = FindFontFamilyByTokenNames(aFontname);
        if (pFoundData)
            return pFoundData;

        aFontname = rDefaults.getDefaultFont(aLanguageTag, DefaultFontType::FIXED);
        pFoundData = FindFontFamilyByTokenNames(aFontname);
        if (pFoundData)
            return pFoundData;
    }

    // now try to find a reasonable non-symbol font

    ImplInitMatchData();

    for (auto const& family : maFontFamilies)
    {
        FontFamily* pData = family.second.get();
        if (pData->GetMatchType() & ImplFontAttrs::Symbol)
            continue;

        pFoundData = pData;
        if (pData->GetMatchType() & (ImplFontAttrs::Default | ImplFontAttrs::Standard))
            break;
    }
    if (pFoundData)
        return pFoundData;

    // finding any font is better than finding no font at all
    auto it = maFontFamilies.begin();
    if (it != maFontFamilies.end())
        pFoundData = (*it).second.get();

    return pFoundData;
}

std::shared_ptr<FontManager> FontManager::Clone() const
{
    auto xClonedCollection = std::make_shared<FontManager>();
    xClonedCollection->mpPreMatchHook = mpPreMatchHook;
    xClonedCollection->mpFallbackHook = mpFallbackHook;

    // TODO: clone the config-font attributes too?
    xClonedCollection->mbMatchData = false;

    for (auto const& family : maFontFamilies)
    {
        const FontFamily* pFontFace = family.second.get();
        pFontFace->UpdateCloneFontList(*xClonedCollection);
    }

    return xClonedCollection;
}

std::unique_ptr<FontFaceCollection> FontManager::GetDeviceFontList() const
{
    std::unique_ptr<FontFaceCollection> pDeviceFontList(new FontFaceCollection);

    for (auto const& family : maFontFamilies)
    {
        const FontFamily* pFontFamily = family.second.get();
        pFontFamily->UpdateDevFontList(*pDeviceFontList);
    }

    return pDeviceFontList;
}

std::unique_ptr<FontFaceSizeCollection>
FontManager::GetDeviceFontSizeList(const OUString& rFontName) const
{
    std::unique_ptr<FontFaceSizeCollection> pDeviceFontSizeList(new FontFaceSizeCollection);

    FontFamily* pFontFamily = FindFontFamily(rFontName);
    if (pFontFamily != nullptr)
    {
        o3tl::sorted_vector<int> rHeights;
        pFontFamily->GetFontHeights(rHeights);

        for (const auto& rHeight : rHeights)
            pDeviceFontSizeList->Add(rHeight);
    }

    return pDeviceFontSizeList;
}

// These are the metric-compatible replacement fonts that are bundled with
// LibreOffice, we prefer them over generic substitutions that might be
// provided by the system.
const std::vector<std::pair<OUString, OUString>> aMetricCompatibleMap = {
    { "Times New Roman", "Liberation Serif" },
    { "Arial", "Liberation Sans" },
    { "Arial Narrow", "Liberation Sans Narrow" },
    { "Courier New", "Liberation Mono" },
    { "Cambria", "Caladea" },
    { "Calibri", "Carlito" },
};

static bool FindMetricCompatibleFont(FontSelectPattern& rFontSelData)
{
    for (const auto& aSub : aMetricCompatibleMap)
    {
        if (rFontSelData.maSearchName == GetEnglishSearchFontName(aSub.first))
        {
            rFontSelData.maSearchName = aSub.second;
            return true;
        }
    }

    return false;
}

FontFamily* FontManager::FindFontFamily(FontSelectPattern& rFSD) const
{
    // give up if no fonts are available
    if (!Count())
        return nullptr;

    static bool noFontLookup = getenv("SAL_NO_FONT_LOOKUP") != nullptr;
    if (noFontLookup)
    {
        // Hard code the use of Liberation Sans and skip font search.
        sal_Int32 nIndex = 0;
        rFSD.maTargetName = GetNextFontToken(rFSD.GetFamilyName(), nIndex);
        rFSD.maSearchName = "liberationsans";
        FontFamily* pFont = ImplFindFontFamilyBySearchName(rFSD.maSearchName);
        assert(pFont);
        return pFont;
    }

    bool bMultiToken = false;
    sal_Int32 nTokenPos = 0;
    OUString& aSearchName = rFSD.maSearchName; // TODO: get rid of reference
    for (;;)
    {
        rFSD.maTargetName = GetNextFontToken(rFSD.GetFamilyName(), nTokenPos);
        aSearchName = rFSD.maTargetName;

        // Until features are properly supported, they are appended to the
        // font name, so we need to strip them off so the font is found.
        sal_Int32 nFeat = aSearchName.indexOf(FontSelectPattern::FEAT_PREFIX);
        OUString aOrigName = rFSD.maTargetName;
        OUString aBaseFontName
            = aSearchName.copy(0, (nFeat != -1) ? nFeat : aSearchName.getLength());

        if (nFeat != -1)
        {
            aSearchName = aBaseFontName;
            rFSD.maTargetName = aBaseFontName;
        }

        aSearchName = GetEnglishSearchFontName(aSearchName);
        ImplFontSubstitute(aSearchName);
        // #114999# special emboldening for Ricoh fonts
        // TODO: smarter check for special cases by using PreMatch infrastructure?
        if ((rFSD.GetWeight() > WEIGHT_MEDIUM) && aSearchName.startsWithIgnoreAsciiCase("hg"))
        {
            OUString aBoldName;
            if (aSearchName.startsWithIgnoreAsciiCase("hggothicb"))
                aBoldName = "hggothice";
            else if (aSearchName.startsWithIgnoreAsciiCase("hgpgothicb"))
                aBoldName = "hgpgothice";
            else if (aSearchName.startsWithIgnoreAsciiCase("hgminchol"))
                aBoldName = "hgminchob";
            else if (aSearchName.startsWithIgnoreAsciiCase("hgpminchol"))
                aBoldName = "hgpminchob";
            else if (aSearchName.equalsIgnoreAsciiCase("hgminchob"))
                aBoldName = "hgminchoe";
            else if (aSearchName.equalsIgnoreAsciiCase("hgpminchob"))
                aBoldName = "hgpminchoe";

            if (!aBoldName.isEmpty() && ImplFindFontFamilyBySearchName(aBoldName))
            {
                // the other font is available => use it
                aSearchName = aBoldName;
                // prevent synthetic emboldening of bold version
                rFSD.SetWeight(WEIGHT_DONTKNOW);
            }
        }

        // restore the features to make the font selection data unique
        rFSD.maTargetName = aOrigName;

        // check if the current font name token or its substitute is valid
        FontFamily* pFoundData = ImplFindFontFamilyBySearchName(aSearchName);
        if (pFoundData)
            return pFoundData;

        // some systems provide special customization
        // e.g. they suggest "serif" as UI-font, but this name cannot be used directly
        //      because the system wants to map it to another font first, e.g. "Helvetica"

        // use the target name to search in the prematch hook
        rFSD.maTargetName = aBaseFontName;

        // Related: fdo#49271 RTF files often contain weird-ass
        // Win 3.1/Win95 style fontnames which attempt to put the
        // charset encoding into the filename
        // http://www.webcenter.ru/~kazarn/eng/fonts_ttf.htm
        OUString sStrippedName = StripScriptFromName(rFSD.maTargetName);
        if (sStrippedName != rFSD.maTargetName)
        {
            rFSD.maTargetName = sStrippedName;
            aSearchName = GetEnglishSearchFontName(rFSD.maTargetName);
            pFoundData = ImplFindFontFamilyBySearchName(aSearchName);
            if (pFoundData)
                return pFoundData;
        }

        if (FindMetricCompatibleFont(rFSD)
            || (mpPreMatchHook && mpPreMatchHook->FindFontSubstitute(rFSD)))
        {
            aSearchName = GetEnglishSearchFontName(aSearchName);
        }

        // the prematch hook uses the target name to search, but we now need
        // to restore the features to make the font selection data unique
        rFSD.maTargetName = aOrigName;

        pFoundData = ImplFindFontFamilyBySearchName(aSearchName);
        if (pFoundData)
            return pFoundData;

        // break after last font name token was checked unsuccessfully
        if (nTokenPos == -1)
            break;
        bMultiToken = true;
    }

    // if the first font was not available find the next available font in
    // the semicolon separated list of font names. A font is also considered
    // available when there is a matching entry in the Tools->Options->Fonts
    // dialog with neither ALWAYS nor SCREENONLY flags set and the substitution
    // font is available
    for (nTokenPos = 0; nTokenPos != -1;)
    {
        if (bMultiToken)
        {
            rFSD.maTargetName = GetNextFontToken(rFSD.GetFamilyName(), nTokenPos);
            aSearchName = GetEnglishSearchFontName(rFSD.maTargetName);
        }
        else
            nTokenPos = -1;
        if (FindMetricCompatibleFont(rFSD)
            || (mpPreMatchHook && mpPreMatchHook->FindFontSubstitute(rFSD)))
        {
            aSearchName = GetEnglishSearchFontName(aSearchName);
        }
        ImplFontSubstitute(aSearchName);
        FontFamily* pFoundData = ImplFindFontFamilyBySearchName(aSearchName);
        if (pFoundData)
            return pFoundData;
    }

    // if no font with a directly matching name is available use the
    // first font name token and get its attributes to find a replacement
    if (bMultiToken)
    {
        nTokenPos = 0;
        rFSD.maTargetName = GetNextFontToken(rFSD.GetFamilyName(), nTokenPos);
        aSearchName = GetEnglishSearchFontName(rFSD.maTargetName);
    }

    OUString aSearchShortName;
    OUString aSearchFamilyName;
    FontWeight eSearchWeight = rFSD.GetWeight();
    FontWidth eSearchWidth = rFSD.GetWidthType();
    ImplFontAttrs nSearchType = ImplFontAttrs::None;
    utl::FontSubstConfiguration::getMapName(aSearchName, aSearchShortName, aSearchFamilyName,
                                            eSearchWeight, eSearchWidth, nSearchType);

    // note: the search name was already translated to english (if possible)
    // use the font's shortened name if needed
    if (aSearchShortName != aSearchName)
    {
        FontFamily* pFoundData = ImplFindFontFamilyBySearchName(aSearchShortName);
        if (pFoundData)
        {
#ifdef UNX
            /* #96738# don't use mincho as a replacement for "MS Mincho" on X11: Mincho is
            a korean bitmap font that is not suitable here. Use the font replacement table,
            that automatically leads to the desired "HG Mincho Light J". Same story for
            MS Gothic, there are thai and korean "Gothic" fonts, so we even prefer Andale */
            if ((aSearchName != "msmincho") && (aSearchName != "msgothic"))
            // TODO: add heuristic to only throw out the fake ms* fonts
#endif
            {
                return pFoundData;
            }
        }
    }

    // use font fallback
    const utl::FontNameAttr* pFontAttr = nullptr;
    if (!aSearchName.isEmpty() && !utl::ConfigManager::IsFuzzing())
    {
        // get fallback info using FontSubstConfiguration and
        // the target name, it's shortened name and family name in that order
        const utl::FontSubstConfiguration& rFontSubst = utl::FontSubstConfiguration::get();
        pFontAttr = rFontSubst.getSubstInfo(aSearchName);
        if (!pFontAttr && (aSearchShortName != aSearchName))
            pFontAttr = rFontSubst.getSubstInfo(aSearchShortName);
        if (!pFontAttr && (aSearchFamilyName != aSearchShortName))
            pFontAttr = rFontSubst.getSubstInfo(aSearchFamilyName);

        // try the font substitutions suggested by the fallback info
        if (pFontAttr)
        {
            FontFamily* pFoundData = ImplFindFontFamilyBySubstFontAttr(*pFontAttr);
            if (pFoundData)
                return pFoundData;
        }
    }

    // if a target symbol font is not available use a default symbol font
    if (rFSD.IsSymbolFont())
    {
        LanguageTag aDefaultLanguageTag("en");
        if (utl::ConfigManager::IsFuzzing())
            aSearchName = "OpenSymbol";
        else
            aSearchName = utl::DefaultFontConfiguration::get().getDefaultFont(
                aDefaultLanguageTag, DefaultFontType::SYMBOL);
        FontFamily* pFoundData = FindFontFamilyByTokenNames(aSearchName);
        if (pFoundData)
            return pFoundData;
    }

    // now try the other font name tokens
    while (nTokenPos != -1)
    {
        rFSD.maTargetName = GetNextFontToken(rFSD.GetFamilyName(), nTokenPos);
        if (rFSD.maTargetName.isEmpty())
            continue;

        aSearchName = GetEnglishSearchFontName(rFSD.maTargetName);

        OUString aTempShortName;
        OUString aTempFamilyName;
        ImplFontAttrs nTempType = ImplFontAttrs::None;
        FontWeight eTempWeight = rFSD.GetWeight();
        FontWidth eTempWidth = WIDTH_DONTKNOW;
        utl::FontSubstConfiguration::getMapName(aSearchName, aTempShortName, aTempFamilyName,
                                                eTempWeight, eTempWidth, nTempType);

        // use a shortened token name if available
        if (aTempShortName != aSearchName)
        {
            FontFamily* pFoundData = ImplFindFontFamilyBySearchName(aTempShortName);
            if (pFoundData)
                return pFoundData;
        }

        const utl::FontNameAttr* pTempFontAttr = nullptr;
        if (!utl::ConfigManager::IsFuzzing())
        {
            // use a font name from font fallback list to determine font attributes
            // get fallback info using FontSubstConfiguration and
            // the target name, it's shortened name and family name in that order
            const utl::FontSubstConfiguration& rFontSubst = utl::FontSubstConfiguration::get();
            pTempFontAttr = rFontSubst.getSubstInfo(aSearchName);

            if (!pTempFontAttr && (aTempShortName != aSearchName))
                pTempFontAttr = rFontSubst.getSubstInfo(aTempShortName);

            if (!pTempFontAttr && (aTempFamilyName != aTempShortName))
                pTempFontAttr = rFontSubst.getSubstInfo(aTempFamilyName);
        }

        // try the font substitutions suggested by the fallback info
        if (pTempFontAttr)
        {
            FontFamily* pFoundData = ImplFindFontFamilyBySubstFontAttr(*pTempFontAttr);
            if (pFoundData)
                return pFoundData;
            if (!pFontAttr)
                pFontAttr = pTempFontAttr;
        }
    }

    // if still needed use the font request's attributes to find a good match
    if (MsLangId::isSimplifiedChinese(rFSD.meLanguage))
        nSearchType |= ImplFontAttrs::CJK | ImplFontAttrs::CJK_SC;
    else if (MsLangId::isTraditionalChinese(rFSD.meLanguage))
        nSearchType |= ImplFontAttrs::CJK | ImplFontAttrs::CJK_TC;
    else if (MsLangId::isKorean(rFSD.meLanguage))
        nSearchType |= ImplFontAttrs::CJK | ImplFontAttrs::CJK_KR;
    else if (rFSD.meLanguage == LANGUAGE_JAPANESE)
        nSearchType |= ImplFontAttrs::CJK | ImplFontAttrs::CJK_JP;
    else
    {
        nSearchType |= lcl_IsCJKFont(rFSD.GetFamilyName());
        if (rFSD.IsSymbolFont())
            nSearchType |= ImplFontAttrs::Symbol;
    }

    FontFamily::CalcType(nSearchType, eSearchWeight, eSearchWidth, rFSD.GetFamilyType(), pFontAttr);
    FontFamily* pFoundData = FindFontFamilyByAttributes(nSearchType, eSearchWeight, eSearchWidth,
                                                        rFSD.GetItalic(), aSearchFamilyName);

    if (pFoundData)
    {
        // overwrite font selection attributes using info from the typeface flags
        if ((eSearchWeight >= WEIGHT_BOLD) && (eSearchWeight > rFSD.GetWeight())
            && (pFoundData->GetTypeFaces() & FontTypeFaces::Bold))
        {
            rFSD.SetWeight(eSearchWeight);
        }
        else if ((eSearchWeight < WEIGHT_NORMAL) && (eSearchWeight < rFSD.GetWeight())
                 && (eSearchWeight != WEIGHT_DONTKNOW)
                 && (pFoundData->GetTypeFaces() & FontTypeFaces::Light))
        {
            rFSD.SetWeight(eSearchWeight);
        }

        if ((nSearchType & ImplFontAttrs::Italic)
            && ((rFSD.GetItalic() == ITALIC_DONTKNOW) || (rFSD.GetItalic() == ITALIC_NONE))
            && (pFoundData->GetTypeFaces() & FontTypeFaces::Italic))
        {
            rFSD.SetItalic(ITALIC_NORMAL);
        }
    }
    else
    {
        // if still needed fall back to default fonts
        pFoundData = ImplFindFontFamilyOfDefaultFont();
    }

    return pFoundData;
}

size_t FontManager::IFSD_Hash::operator()(const FontSelectPattern& rFSD) const
{
    return rFSD.hashCode();
}

bool FontManager::IFSD_Equal::operator()(const FontSelectPattern& rA,
                                         const FontSelectPattern& rB) const
{
    // check normalized font family name
    if (rA.maSearchName != rB.maSearchName)
        return false;

    // check font transformation
    if ((rA.mnHeight != rB.mnHeight) || (rA.mnWidth != rB.mnWidth)
        || (rA.mnOrientation != rB.mnOrientation))
        return false;

    // check mapping relevant attributes
    if ((rA.mbVertical != rB.mbVertical) || (rA.meLanguage != rB.meLanguage))
        return false;

    // check font face attributes
    if ((rA.GetWeight() != rB.GetWeight())
        || (rA.GetItalic() != rB.GetItalic())
        //  ||  (rA.meFamily       != rB.meFamily) // TODO: remove this mostly obsolete member
        || (rA.GetPitch() != rB.GetPitch()))
        return false;

    // check style name
    if (rA.GetStyleName() != rB.GetStyleName())
        return false;

    // Symbol fonts may recode from one type to another So they are only
    // safely equivalent for equal targets
    if (rA.IsSymbolFont() || rB.IsSymbolFont())
    {
        if (rA.maTargetName != rB.maTargetName)
            return false;
    }

    // check for features
    if ((rA.maTargetName.indexOf(FontSelectPattern::FEAT_PREFIX) != -1
         || rB.maTargetName.indexOf(FontSelectPattern::FEAT_PREFIX) != -1)
        && rA.maTargetName != rB.maTargetName)
        return false;

    if (rA.mbEmbolden != rB.mbEmbolden)
        return false;

    if (rA.maItalicMatrix != rB.maItalicMatrix)
        return false;

    return true;
}

rtl::Reference<FontInstance>
FontManager::GetFontInstance(vcl::Font const& rFont, FontSize const& rFontSize, bool bNonAntialias)
{
    // initialize internal font request object
    FontSelectPattern aFontSelData(rFont, rFont.GetFamilyName(), rFontSize, bNonAntialias);
    return GetFontInstance(aFontSelData);
}

rtl::Reference<FontInstance> FontManager::GetFontInstance(FontSelectPattern& aFontSelData)
{
    rtl::Reference<FontInstance> pFontInstance;
    FontFamily* pFontFamily = nullptr;

    // check if a directly matching logical font instance is already cached,
    // the most recently used font usually has a hit rate of >50%
    if (mpLastHitCacheEntry
        && IFSD_Equal()(aFontSelData, mpLastHitCacheEntry->GetFontSelectPattern()))
    {
        pFontInstance = mpLastHitCacheEntry;
    }
    else
    {
        FontInstanceList::const_iterator it = maFontInstanceList.find(aFontSelData);
        if (it != maFontInstanceList.end())
            pFontInstance = (*it).second;
    }

    if (!pFontInstance) // no direct cache hit
    {
        // find the best matching logical font family and update font selector accordingly
        pFontFamily = FindFontFamily(aFontSelData);
        SAL_WARN_IF((pFontFamily == nullptr), "vcl", "FontManager::Get() No logical font found!");
        if (pFontFamily)
        {
            aFontSelData.maSearchName = pFontFamily->GetSearchName();

            // check if an indirectly matching logical font instance is already cached
            FontInstanceList::const_iterator it = maFontInstanceList.find(aFontSelData);
            if (it != maFontInstanceList.end())
                pFontInstance = (*it).second;
        }
    }

    if (!pFontInstance && pFontFamily) // still no cache hit => create a new font instance
    {
        FontFace* pFontData = pFontFamily->FindBestFontFace(aFontSelData);

        // create a new logical font instance from this physical font face
        pFontInstance = pFontData->CreateFontInstance(aFontSelData);
        pFontInstance->mpFontManager = this;

        // if we're substituting from or to a symbol font we may need a symbol
        // conversion table
        if (pFontData->IsSymbolFont() || aFontSelData.IsSymbolFont())
        {
            if (aFontSelData.maTargetName != aFontSelData.maSearchName)
            {
                ConvertChar const* pConvertChar = ConvertChar::GetRecodeData(
                    aFontSelData.maTargetName, aFontSelData.maSearchName);
                pFontInstance->InitConversion(pConvertChar);
            }
        }

#ifdef MACOSX
        //It might be better to dig out the font version of the target font
        //to see if it's a modern re-coded apple symbol font in case that
        //font shows up on a different platform
        if (!pFontInstance->mpConversion
            && aFontSelData.maTargetName.equalsIgnoreAsciiCase("symbol")
            && aFontSelData.maSearchName.equalsIgnoreAsciiCase("symbol"))
        {
            pFontInstance->mpConversion = ConvertChar::GetRecodeData("Symbol", "AppleSymbol");
        }
#endif

        static const size_t FONTCACHE_MAX = getenv("LO_TESTNAME") ? 1 : 50;

        if (maFontInstanceList.size() >= FONTCACHE_MAX)
        {
            struct limit_exception : public std::exception
            {
            };
            try
            {
                maFontInstanceList.remove_if(
                    [this](FontInstanceList::key_value_pair_t const& rFontPair) {
                        if (maFontInstanceList.size() < FONTCACHE_MAX)
                            throw limit_exception();
                        FontInstance* pFontEntry = rFontPair.second.get();
                        if (pFontEntry->m_nCount > 1)
                            return false;
                        m_aBoundRectCache.remove_if(
                            [&pFontEntry](GlyphBoundRectCache::key_value_pair_t const& rGlyphPair) {
                                return rGlyphPair.first.m_pFont == pFontEntry;
                            });
                        if (mpLastHitCacheEntry == pFontEntry)
                            mpLastHitCacheEntry = nullptr;
                        return true;
                    });
            }
            catch (limit_exception&)
            {
            }
        }

        assert(pFontInstance);
        // add the new entry to the cache
        maFontInstanceList.insert({ aFontSelData, pFontInstance.get() });
    }

    mpLastHitCacheEntry = pFontInstance.get();
    return pFontInstance;
}

rtl::Reference<FontInstance>
FontManager::GetGlyphFallbackFontInstance(FontSelectPattern& rFontSelData,
                                          FontInstance* pFontInstance, int nFallbackLevel,
                                          OUString& rMissingCodes)
{
    // get a candidate font for glyph fallback
    // unless the previously selected font got a device specific substitution
    // e.g. PsPrint Arial->Helvetica for udiaeresis when Helvetica doesn't support it
    if (nFallbackLevel >= 1)
    {
        FontFamily* pFallbackData = nullptr;

        //fdo#33898 If someone has EUDC installed then they really want that to
        //be used as the first-choice glyph fallback seeing as it's filled with
        //private area codes with don't make any sense in any other font so
        //prioritize it here if it's available. Ideally we would remove from
        //rMissingCodes all the glyphs which it is able to resolve as an
        //optimization, but that's tricky to achieve cross-platform without
        //sufficient heavy-weight code that's likely to undo the value of the
        //optimization
        if (nFallbackLevel == 1)
            pFallbackData = FindFontFamily("EUDC");
        if (!pFallbackData)
            pFallbackData = GetGlyphFallbackFontFamily(rFontSelData, pFontInstance, rMissingCodes,
                                                       nFallbackLevel - 1);
        // escape when there are no font candidates
        if (!pFallbackData)
            return nullptr;
        // override the font name
        rFontSelData.SetFamilyName(pFallbackData->GetFamilyName());
        // clear the cached normalized name
        rFontSelData.maSearchName.clear();
    }

    rtl::Reference<FontInstance> pFallbackFont = GetFontInstance(rFontSelData);
    return pFallbackFont;
}

void FontManager::Invalidate()
{
    // #112304# make sure the font cache is really clean
    mpLastHitCacheEntry = nullptr;
    for (auto const& pair : maFontInstanceList)
        pair.second->mpFontManager = nullptr;
    maFontInstanceList.clear();
    m_aBoundRectCache.clear();
}

bool FontManager::GetCachedGlyphBoundRect(FontInstance const* pFont, sal_GlyphId nID,
                                          tools::Rectangle& rRect)
{
    if (!pFont->GetFontCache())
        return false;
    assert(pFont->GetFontCache() == this);
    if (pFont->GetFontCache() != this)
        return false;

    auto it = m_aBoundRectCache.find({ pFont, nID });
    if (it != m_aBoundRectCache.end())
    {
        rRect = it->second;
        return true;
    }
    return false;
}

void FontManager::CacheGlyphBoundRect(FontInstance const* pFont, sal_GlyphId nID,
                                      tools::Rectangle& rRect)
{
    if (!pFont->GetFontCache())
        return;
    assert(pFont->GetFontCache() == this);
    if (pFont->GetFontCache() != this)
        return;

    m_aBoundRectCache.insert({ { pFont, nID }, rRect });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
