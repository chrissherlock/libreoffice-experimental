/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/digitlocalization.hxx>
#include <i18nutil/unicode.hxx>

#include <vcl/mnemonic.hxx>
#include <vcl/font.hxx>

#include <GraphicsState.hxx>
#include <font/FontController.hxx>
#include <font/FontSelectPattern.hxx>
#include <text/TextAnalyzer.hxx>

#include <unicode/uchar.h>

#include <algorithm>

namespace vcl::text
{
SalLayoutFlags TextAnalyzer::GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                                std::u16string_view rStr, const sal_Int32 nMinIndex,
                                                const sal_Int32 nEndIndex)
{
    SalLayoutFlags nLayoutFlags = SalLayoutFlags::NONE;

    if (eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiRtl;
    }
    if (eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiStrong)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }
    else if (!(eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl))
    {
        // Disable Bidi if no RTL hint and only known LTR codes used.
        bool bAllLtr = true;
        for (sal_Int32 i = nMinIndex; i < nEndIndex; i++)
        {
            // [0x0000, 0x052F] are Latin, Greek and Cyrillic.
            if (rStr[i] > 0x052F)
            {
                bAllLtr = false;
                break;
            }
        }

        if (bAllLtr)
            nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }

    return nLayoutFlags;
}

SalLayoutFlags
TextAnalyzer::CalculateLayoutFlags(const vcl::GraphicsState& rGraphicsState,
                                   const vcl::font::FontRealization& rFontRealization,
                                   bool bRTLWindow, std::u16string_view rStr, sal_Int32 nMinIndex,
                                   sal_Int32 nEndIndex, SalLayoutFlags nExistingFlags)
{
    SalLayoutFlags nFlags = nExistingFlags;

    nFlags |= GetBiDiLayoutFlags(rFontRealization.eLayoutMode, rStr, nMinIndex, nEndIndex);

    if (!rGraphicsState.maFont.IsKerning())
        nFlags |= SalLayoutFlags::DisableKerning;

    if (rGraphicsState.maFont.GetKerning() & FontKerning::Asian)
        nFlags |= SalLayoutFlags::KerningAsian;

    if (rGraphicsState.maFont.IsVertical())
        nFlags |= SalLayoutFlags::Vertical;

    if (rGraphicsState.maFont.IsFixKerning()
        || (rFontRealization.mxFont
            && rFontRealization.mxFont->GetFontSelectPattern().GetPitch() == PITCH_FIXED))
    {
        nFlags |= SalLayoutFlags::DisableLigatures;
    }

    bool bRightAlign
        = bool(rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl);

    if (rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::TextOriginLeft)
        bRightAlign = false;
    else if (rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::TextOriginRight)
        bRightAlign = true;

    bRightAlign ^= bRTLWindow;

    if (bRightAlign)
        nFlags |= SalLayoutFlags::RightAlign;

    return nFlags;
}

void TextAnalyzer::ApplyDigitLocalization(const vcl::GraphicsState& rGraphicsState, OUString& rStr,
                                          sal_Int32 nMinIndex, sal_Int32& rEndIndex)
{
    if (rGraphicsState.meTextLanguage)
    {
        sal_Int32 nSubstringLen = rEndIndex - nMinIndex;
        rStr = i18nutil::LocalizeDigitsInString(rStr, rGraphicsState.meTextLanguage, nMinIndex,
                                                nSubstringLen);
        rEndIndex = nMinIndex + nSubstringLen;
    }
}

MnemonicText TextAnalyzer::PrepareMnemonicText(const OUString& rStr, sal_Int32 nIndex,
                                               sal_Int32 nLen)
{
    sal_Int32 nMnemonicPos = -1;
    OUString aStr = removeMnemonicFromString(rStr, nMnemonicPos);

    if (nMnemonicPos != -1)
    {
        if (nMnemonicPos < nIndex)
            nIndex--;
        else if (nMnemonicPos < nIndex + nLen)
            nLen--;
    }
    return { aStr, nIndex, nLen, nMnemonicPos };
}

bool TextAnalyzer::IsMnemonicInRange(sal_Int32 nMnemonicPos, sal_Int32 nIndex, sal_Int32 nLen)
{
    return nMnemonicPos >= nIndex && nMnemonicPos < nIndex + nLen;
}

sal_Int32 TextAnalyzer::GetNormalizedLength(const OUString& rStr, sal_Int32 nIdx, sal_Int32 nLen)
{
    if (nLen < 0 || (nIdx + nLen) > rStr.getLength())
        return std::max<sal_Int32>(0, rStr.getLength() - nIdx);

    return nLen;
}

} // namespace vcl::text
