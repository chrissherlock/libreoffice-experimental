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

#include <o3tl/lru_map.hxx>

#include <vcl/dllapi.h>
#include <vcl/glyphitem.hxx>

#include <font/FontFamily.hxx>

#include <array>

#include <boost/functional/hash.hpp>

#define MAX_GLYPHFALLBACK 16

class FontInstance;
class FontFaceSizeCollection;
class ImplGlyphFallbackFontSubstitution;
class ImplPreMatchFontSubstitution;

// TODO: rename to FontManager

struct GlyphBoundRectCacheKey
{
    const FontInstance* m_pFont;
    const sal_GlyphId m_nId;

    GlyphBoundRectCacheKey(const FontInstance* pFont, sal_GlyphId nID)
        : m_pFont(pFont)
        , m_nId(nID)
    {
    }

    bool operator==(GlyphBoundRectCacheKey const& aOther) const
    {
        return m_pFont == aOther.m_pFont && m_nId == aOther.m_nId;
    }
};

struct GlyphBoundRectCacheHash
{
    std::size_t operator()(GlyphBoundRectCacheKey const& aCache) const
    {
        std::size_t seed = 0;
        boost::hash_combine(seed, aCache.m_pFont);
        boost::hash_combine(seed, aCache.m_nId);
        return seed;
    }
};

typedef o3tl::lru_map<GlyphBoundRectCacheKey, tools::Rectangle, GlyphBoundRectCacheHash>
    GlyphBoundRectCache;

class VCL_PLUGIN_PUBLIC FontManager final
{
public:
    explicit FontManager();
    ~FontManager();

    // fill the list with device font faces
    void Add(FontFace*);
    void Clear();
    int Count() const { return maFontFamilies.size(); }

    // find the device font family
    FontFamily* FindFontFamily(const OUString& rFontName) const;
    FontFamily* FindOrCreateFontFamily(const OUString& rFamilyName);
    FontFamily* FindFontFamily(FontSelectPattern&) const;
    FontFamily* FindFontFamilyByTokenNames(const OUString& rTokenStr) const;
    FontFamily* FindFontFamilyByAttributes(ImplFontAttrs nSearchType, FontWeight, FontWidth,
                                           FontItalic, const OUString& rSearchFamily) const;

    rtl::Reference<FontInstance> GetFontInstance(const vcl::Font&, const FontSize& rPixelSize,
                                                 bool bNonAntialias = false);
    // suggest fonts for glyph fallback
    FontFamily* GetGlyphFallbackFontFamily(FontSelectPattern&, FontInstance* pLogicalFont,
                                           OUString& rMissingCodes, int nFallbackLevel) const;

    rtl::Reference<FontInstance> GetGlyphFallbackFontInstance(FontSelectPattern&,
                                                              FontInstance* pLogicalFont,
                                                              int nFallbackLevel,
                                                              OUString& rMissingCodes);

    // prepare platform specific font substitutions
    void SetPreMatchHook(ImplPreMatchFontSubstitution*);
    void SetFallbackHook(ImplGlyphFallbackFontSubstitution*);

    // misc utilities
    std::shared_ptr<FontManager> Clone() const;
    std::unique_ptr<FontFaceCollection> GetDeviceFontList() const;
    std::unique_ptr<FontFaceSizeCollection> GetDeviceFontSizeList(const OUString& rFontName) const;

    // font cache
    bool GetCachedGlyphBoundRect(const FontInstance*, sal_GlyphId, tools::Rectangle&);
    void CacheGlyphBoundRect(const FontInstance*, sal_GlyphId, tools::Rectangle&);
    void Invalidate();

private:
    // cache of recently used font instances
    struct IFSD_Equal
    {
        bool operator()(const FontSelectPattern&, const FontSelectPattern&) const;
    };

    struct IFSD_Hash
    {
        size_t operator()(const FontSelectPattern&) const;
    };

    typedef o3tl::lru_map<FontSelectPattern, rtl::Reference<FontInstance>, IFSD_Hash, IFSD_Equal>
        FontInstanceList;

    mutable bool mbMatchData; // true if matching attributes are initialized

    typedef std::unordered_map<OUString, std::unique_ptr<FontFamily>> FontFamilies;
    FontFamilies maFontFamilies;

    ImplPreMatchFontSubstitution* mpPreMatchHook; // device specific prematch substitution
    ImplGlyphFallbackFontSubstitution*
        mpFallbackHook; // device specific glyph fallback substitution

    mutable std::unique_ptr<std::array<FontFamily*, MAX_GLYPHFALLBACK>> mpFallbackList;
    mutable int mnFallbackCount;

    FontInstance* mpLastHitCacheEntry; ///< keeps the last hit cache entry
    FontInstanceList maFontInstanceList;
    GlyphBoundRectCache m_aBoundRectCache;

    void ImplInitMatchData() const;
    void ImplInitGenericGlyphFallback() const;

    FontFamily* ImplFindFontFamilyBySearchName(const OUString&) const;
    FontFamily* ImplFindFontFamilyBySubstFontAttr(const utl::FontNameAttr&) const;

    FontFamily* ImplFindFontFamilyOfDefaultFont() const;

    rtl::Reference<FontInstance> GetFontInstance(FontSelectPattern&);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
