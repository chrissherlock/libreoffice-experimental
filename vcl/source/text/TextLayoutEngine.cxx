/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/point/b2dpoint.hxx>
#include <tools/gen.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/digitlocalization.hxx>
#include <i18nutil/unicode.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/font.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/svapp.hxx>

#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/FontSelectPattern.hxx>
#include <sallayout.hxx>
#include <text/TextLayoutEngine.hxx>
#include <GraphicsState.hxx>
#include <ImplLayoutArgs.hxx>

#include <unicode/uchar.h>

namespace vcl::text
{
void TextLayoutEngine::GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
                                               std::vector<bool>& rOutMap)
{
    rOutMap.clear();
    if (!rLayout.HasFontKashidaPositions())
        return;

    size_t nEnd = rText.length();
    rOutMap.resize(nEnd, false);

    for (size_t i = 0; i < nEnd; ++i)
    {
        size_t nNextPos = i + 1;
        while (nNextPos < nEnd
               && u_getIntPropertyValue(rText[nNextPos], UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT)
            ++nNextPos;

        rOutMap[i] = rLayout.IsKashidaPosValid(i, nNextPos);
    }
}

std::vector<Point> TextLayoutEngine::GetEmphasisMarkPositions(const SalLayout& rLayout,
                                                              long nAscent, long nDescent,
                                                              FontEmphasisMark nStyle)
{
    std::vector<Point> aPositions;

    // Explicit bool cast for o3tl flags
    bool bBelow = bool(nStyle & FontEmphasisMark::PosBelow);
    long nYOffset = bBelow ? nDescent : -nAscent;

    // Empirical visual centering logic
    long nSpacing = (nAscent + nDescent) / 4;
    nYOffset += bBelow ? (nSpacing / 2) : -(nSpacing / 2);

    int nIterator = 0;
    const GlyphItem* pGlyph = nullptr;
    basegfx::B2DPoint aPos;

    while (rLayout.GetNextGlyph(&pGlyph, aPos, nIterator))
    {
        if (!pGlyph)
            continue;

        // Convert floating point positions to integer Device Pixels
        long nX = static_cast<long>(aPos.getX()) + (pGlyph->origWidth() / 2);
        long nY = static_cast<long>(aPos.getY()) + nYOffset;

        aPositions.emplace_back(nX, nY);
    }

    return aPositions;
}

void TextLayoutEngine::InitializeFontMetrics(
    LogicalFontInstance* pFontInstance, const vcl::Font& rFont, long nDPIY, long nPixelWidth,
    std::function<long(const OUString&)> const& fnGetTextWidth,
    std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect)
{
    if (!pFontInstance)
        return;

    // 1. Calculate Bullet Offset
    long nSpaceW = fnGetTextWidth(OUString(u' '));
    long nBulletW = fnGetTextWidth(u"\x00b7"_ustr);
    long nBulletOffset = (nSpaceW - nBulletW) >> 1;

    pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);

    // 2. Set Above Text Line Size
    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nPixelWidth);

    // 3. CJK Fullstop Centering
    bool bCentered = true;
    if (MsLangId::isCJK(rFont.GetLanguage()))
    {
        tools::Rectangle aRect;
        fnGetBoundRect(aRect, u"\x3001"_ustr); // Fullwidth fullstop

        const auto nH = rFont.GetFontSize().Height();
        const auto nB = aRect.Left();

        bCentered = nB > (((nH >> 1) + nH) >> 3);
    }
    pFontInstance->mxFontMetric->SetFullstopCenteredFlag(bCentered);
}

SalLayoutFlags TextLayoutEngine::GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                                    std::u16string_view rStr,
                                                    const sal_Int32 nMinIndex,
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

SalLayoutFlags TextLayoutEngine::CalculateLayoutFlags(
    const vcl::GraphicsState& rGraphicsState, const vcl::font::FontRealization& rFontRealization,
    bool bRTLWindow, std::u16string_view rStr, sal_Int32 nMinIndex, sal_Int32 nEndIndex,
    SalLayoutFlags nExistingFlags)
{
    SalLayoutFlags nFlags = nExistingFlags;

    nFlags |= TextLayoutEngine::GetBiDiLayoutFlags(rFontRealization.eLayoutMode, rStr, nMinIndex,
                                                   nEndIndex);

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

void TextLayoutEngine::ApplyDigitLocalization(const vcl::GraphicsState& rGraphicsState,
                                              OUString& rStr, sal_Int32 nMinIndex,
                                              sal_Int32& rEndIndex)
{
    if (rGraphicsState.meTextLanguage)
    {
        sal_Int32 nSubstringLen = rEndIndex - nMinIndex;
        rStr = i18nutil::LocalizeDigitsInString(rStr, rGraphicsState.meTextLanguage, nMinIndex,
                                                nSubstringLen);
        rEndIndex = nMinIndex + nSubstringLen;
    }
}

vcl::text::ImplLayoutArgs TextLayoutEngine::CreateLayoutRequest(
    OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth, SalLayoutFlags nFlags,
    const vcl::text::TextLayoutCache* pCache, const GraphicsState& rState,
    const font::FontRealization& rRealization, bool bRTL)
{
    assert(nMinIndex >= 0);
    assert(nLen >= 0);

    // get string length for calculating extents
    sal_Int32 nEndIndex = rStr.getLength();
    if (nMinIndex + nLen < nEndIndex)
        nEndIndex = nMinIndex + nLen;

    // don't bother if there is nothing to do
    if (nEndIndex < nMinIndex)
        nEndIndex = nMinIndex;

    vcl::text::TextLayoutEngine::ApplyDigitLocalization(rState, rStr, nMinIndex, nEndIndex);

    nFlags = vcl::text::TextLayoutEngine::CalculateLayoutFlags(rState, rRealization, bRTL, rStr,
                                                               nMinIndex, nEndIndex, nFlags);

    vcl::text::ImplLayoutArgs aLayoutArgs(rStr, nMinIndex, nEndIndex, nFlags,
                                          rState.maFont.GetLanguageTag(), pCache);

    Degree10 nOrientation = rRealization.mxFont ? rRealization.mxFont->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);

    return aLayoutArgs;
}

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
