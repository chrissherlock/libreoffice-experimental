/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ustring.hxx>
#include <tools/ref.hxx>

#include <vcl/dllapi.h>
#include <vcl/glyphitem.hxx>

#include <map>
#include <vector>

class FontFace;
class FontInstance;
class FontManager;
class FontSubsetInfo;
class ImplFontMetricData;
typedef tools::SvRef<ImplFontMetricData> ImplFontMetricDataRef;

typedef sal_Unicode sal_Ucs; // TODO: use sal_UCS4 instead of sal_Unicode
typedef std::map<sal_Ucs, sal_uInt32> Ucs2UIntMap;

namespace vcl
{
struct FontCapabilities;
}

class VCL_DLLPUBLIC SalFont
{
public:
    virtual ~SalFont() {}

    // set the font
    virtual void SetFont(FontInstance*, int nFallbackLevel) = 0;

    // release the fonts
    virtual void ReleaseFonts() = 0;

    // get the current font's metrics
    virtual void GetFontMetric(ImplFontMetricDataRef&, int nFallbackLevel) = 0;

    // get the repertoire of the current font
    virtual FontCharMapRef GetFontCharMap() const = 0;

    // get the layout capabilities of the current font
    virtual bool GetFontCapabilities(vcl::FontCapabilities& rFontCapabilities) const = 0;

    // graphics must fill supplied font list
    virtual void GetDevFontList(FontManager*) = 0;

    // graphics must drop any cached font info
    virtual void ClearDevFontCache() = 0;

    virtual bool AddTempDevFont(FontManager*, const OUString& rFileURL, const OUString& rFontName)
        = 0;

    // CreateFontSubset: a method to get a subset of glyhps of a font
    // inside a new valid font file
    // returns true if creation of subset was successful
    // parameters: rToFile: contains an osl file URL to write the subset to
    //             pFont: describes from which font to create a subset
    //             pGlyphIDs: the glyph ids to be extracted
    //             pEncoding: the character code corresponding to each glyph
    //             pWidths: the advance widths of the corresponding glyphs (in PS font units)
    //             nGlyphs: the number of glyphs
    //             rInfo: additional outgoing information
    // implementation note: encoding 0 with glyph id 0 should be added implicitly
    // as "undefined character"
    virtual bool CreateFontSubset(const OUString& rToFile, const FontFace* pFont,
                                  const sal_GlyphId* pGlyphIDs, const sal_uInt8* pEncoding,
                                  sal_Int32* pWidths, int nGlyphs, FontSubsetInfo& rInfo)
        = 0;

    // GetEmbedFontData: gets the font data for a font marked
    // embeddable by GetDevFontList or NULL in case of error
    // parameters: pFont: describes the font in question
    //             pDataLen: out parameter, contains the byte length of the returned buffer
    virtual const void* GetEmbedFontData(const FontFace* pFont, tools::Long* pDataLen) = 0;

    // free the font data again
    virtual void FreeEmbedFontData(const void* pData, tools::Long nDataLen) = 0;

    // get the same widths as in CreateFontSubset
    // in case of an embeddable font also fill the mapping
    // between unicode and glyph id
    // leave widths vector and mapping untouched in case of failure
    virtual void GetGlyphWidths(const FontFace* pFont, bool bVertical,
                                std::vector<sal_Int32>& rWidths, Ucs2UIntMap& rUnicodeEnc)
        = 0;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
