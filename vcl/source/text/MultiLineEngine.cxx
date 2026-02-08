/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <rtl/ustrbuf.hxx>
#include <tools/lineend.hxx>

#include <vcl/text/MultiLineEngine.hxx>

#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>
#include <CoordinateMapper.hxx>
#include <textlayout.hxx>

#include <algorithm>

namespace vcl::text
{
// Internal helpers
static tools::Long GetSubPixelFactor(const CoordinateMapper& rMapper)
{
    return rMapper.IsMapModeEnabled() ? 1 : 64;
}

static double GetLayoutPixelWidth(const CoordinateMapper& rMapper, tools::Long nLogicWidth,
                                  tools::Long nSubPixelFactor)
{
    return rMapper.LogicWidthToDeviceSubPixel(nLogicWidth * nSubPixelFactor);
}

OUString MultiLineEngine::GetEllipsisString(
    const OUString& rStr, tools::Long nMaxWidth, DrawTextFlags nStyle,
    const std::function<tools::Long(const OUString&)>& rfnGetTextWidth)
{
    if (rStr.isEmpty() || rfnGetTextWidth(rStr) <= nMaxWidth)
        return rStr;

    const OUString aEllipsisStr = "...";
    const bool bClipText = bool(nStyle & DrawTextFlags::Clip);
    sal_Int32 nLen = rStr.getLength();

    // Center Ellipsis (Balanced Truncation)
    if (nStyle & DrawTextFlags::CenterEllipsis)
    {
        sal_Int32 nIndex = nLen / 2;
        sal_Int32 nEraseChars = std::max<sal_Int32>(4, nLen - (nIndex * 4) / 3);

        while (nEraseChars < nLen)
        {
            sal_Int32 i = (nLen - nEraseChars) / 2;
            OUString aTmpStr = rStr.copy(0, i) + aEllipsisStr + rStr.copy(i + nEraseChars);

            if (rfnGetTextWidth(aTmpStr) <= nMaxWidth)
                return aTmpStr;

            nEraseChars++;
        }
        return aEllipsisStr;
    }

    // Path Ellipsis
    if (nStyle & DrawTextFlags::PathEllipsis)
    {
        sal_Int32 nLastSep = rStr.lastIndexOf('/');
        if (nLastSep == -1)
            nLastSep = rStr.lastIndexOf('\\');

        if (nLastSep != -1 && nLastSep > 0)
        {
            for (sal_Int32 i = 1; i < nLastSep; ++i)
            {
                OUString aTest = rStr.copy(0, i) + aEllipsisStr + rStr.copy(nLastSep);

                if (rfnGetTextWidth(aTest) > nMaxWidth)
                {
                    return (i > 1)
                               ? OUString(rStr.copy(0, i - 1) + aEllipsisStr + rStr.copy(nLastSep))
                               : OUString(aEllipsisStr + rStr.copy(nLastSep));
                }
            }
        }
    }

    // News Ellipsis (Structure Preserving)
    if (nStyle & DrawTextFlags::NewsEllipsis)
    {
        OUString aCurrentStr = rStr;
        auto lcl_IsSep = [](sal_Unicode c) { return c == '.'; };
        sal_Int32 nLastContent = nLen;

        while (nLastContent > 0)
        {
            nLastContent--;
            if (lcl_IsSep(aCurrentStr[nLastContent]))
                break;
        }

        while (nLastContent > 0 && lcl_IsSep(aCurrentStr[nLastContent - 1]))
        {
            nLastContent--;
        }

        OUString aLastStr = aCurrentStr.copy(nLastContent);

        if (rfnGetTextWidth(aEllipsisStr + aLastStr) <= nMaxWidth)
        {
            sal_Int32 nFirstContent = 0;

            while (nFirstContent < nLastContent)
            {
                nFirstContent++;
                if (lcl_IsSep(aCurrentStr[nFirstContent]))
                    break;
            }

            while (nFirstContent < nLastContent && lcl_IsSep(aCurrentStr[nFirstContent]))
            {
                nFirstContent++;
            }

            if (nFirstContent < nLastContent)
            {
                if (nFirstContent > 4)
                    nFirstContent = 4;

                OUString aFirstStr = aCurrentStr.copy(0, nFirstContent) + aEllipsisStr;
                OUString aTempStr = aFirstStr + aLastStr;

                if (rfnGetTextWidth(aTempStr) <= nMaxWidth)
                {
                    do
                    {
                        aCurrentStr = aTempStr;
                        if (nLastContent > aCurrentStr.getLength())
                            nLastContent = aCurrentStr.getLength();

                        while (nFirstContent < nLastContent)
                        {
                            nLastContent--;
                            if (lcl_IsSep(aCurrentStr[nLastContent]))
                                break;
                        }

                        while (nFirstContent < nLastContent
                               && lcl_IsSep(aCurrentStr[nLastContent - 1]))
                        {
                            nLastContent--;
                        }

                        if (nFirstContent < nLastContent)
                        {
                            aTempStr = aFirstStr + aCurrentStr.copy(nLastContent);

                            if (rfnGetTextWidth(aTempStr) > nMaxWidth)
                                break;
                        }
                    } while (nFirstContent < nLastContent);

                    return aCurrentStr;
                }
            }
        }
    }

    // End Ellipsis
    for (sal_Int32 i = nLen - 1; i >= 1; --i)
    {
        if (i > 1)
        {
            OUString aWithDots = rStr.copy(0, i) + aEllipsisStr;
            if (rfnGetTextWidth(aWithDots) <= nMaxWidth)
                return aWithDots;
        }
        else
        {
            OUString aFirstChar = rStr.copy(0, 1);
            if (rfnGetTextWidth(aFirstChar) <= nMaxWidth)
                return aFirstChar;
        }
    }

    if (bClipText)
        return rStr.copy(0, 1);

    return aEllipsisStr;
}

sal_Int32 MultiLineEngine::GetTextBreak(const LayoutResources& rRes,
                                        const vcl::text::TextSpan& rSpan, tools::Long nMaxLineWidth,
                                        tools::Long nCharExtra,
                                        const vcl::text::LayoutCacheData& rCache)
{
    const vcl::text::LayoutConstraints aConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE };
    std::unique_ptr<SalLayout> pSalLayout
        = TextLayoutEngine::Layout(rRes, rSpan, aConstraints, rCache, {});

    if (!pSalLayout)
        return -1;

    const tools::Long nSubPixelFactor = GetSubPixelFactor(rRes.rMapper);
    const double nTextPixelWidth
        = GetLayoutPixelWidth(rRes.rMapper, nMaxLineWidth, nSubPixelFactor);

    double nExtraPixelWidth = 0;
    if (nCharExtra != 0)
        nExtraPixelWidth = GetLayoutPixelWidth(rRes.rMapper, nCharExtra, nSubPixelFactor);

    return pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);
}

sal_Int32 MultiLineEngine::GetTextBreakArray(
    const LayoutResources& rRes, const vcl::text::TextSpan& rSpan, tools::Long nTextWidth,
    std::optional<sal_Unicode> nHyphenChar, std::optional<sal_Int32*> pHyphenPos,
    tools::Long nCharExtra, KernArraySpan aKernArray, const vcl::text::LayoutCacheData& rCache)
{
    if (pHyphenPos.has_value())
        **pHyphenPos = -1;

    const vcl::text::LayoutConstraints aConstraints{
        Point(0, 0), 0, aKernArray, {}, SalLayoutFlags::NONE
    };
    std::unique_ptr<SalLayout> pSalLayout
        = TextLayoutEngine::Layout(rRes, rSpan, aConstraints, rCache, {});

    if (!pSalLayout)
        return -1;

    const tools::Long nSubPixelFactor = GetSubPixelFactor(rRes.rMapper);
    double nTextPixelWidth = GetLayoutPixelWidth(rRes.rMapper, nTextWidth, nSubPixelFactor);
    double nExtraPixelWidth
        = (nCharExtra != 0) ? GetLayoutPixelWidth(rRes.rMapper, nCharExtra, nSubPixelFactor) : 0;

    sal_Int32 nRetVal
        = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);

    if (!nHyphenChar.has_value())
        return nRetVal;

    OUString aHyphenStr(*nHyphenChar);
    vcl::text::TextSpan aHyphenSpan{ aHyphenStr, 0, 1 };
    std::unique_ptr<SalLayout> pHyphenLayout
        = TextLayoutEngine::Layout(rRes, aHyphenSpan, {}, {}, {});

    if (!pHyphenLayout)
        return nRetVal;

    double nHyphenPixelWidth = pHyphenLayout->GetTextWidth() * nSubPixelFactor;
    nTextPixelWidth -= nHyphenPixelWidth;
    if (nExtraPixelWidth > 0)
        nTextPixelWidth -= nExtraPixelWidth;

    if (pHyphenPos.has_value())
    {
        **pHyphenPos = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);
        if (**pHyphenPos > nRetVal)
            **pHyphenPos = nRetVal;
    }

    return nRetVal;
}

void MultiLineEngine::CalculateMultiLineLayout(vcl::TextLayoutCommon& rLayout,
                                               MultiLineLayout& rRes, const tools::Rectangle& rRect,
                                               tools::Long nTextHeight, tools::Long nWidth,
                                               tools::Long nHeight, const OUString& rStr,
                                               DrawTextFlags nStyle)
{
    rRes.nResultStyle = nStyle;

    tools::Long nMaxTextWidth
        = rLayout.GetTextLines(rRect, nTextHeight, rRes.aLineInfo, nWidth, rStr, nStyle);
    sal_Int32 nLines = static_cast<sal_Int32>(nHeight / nTextHeight);
    rRes.nFormatLines = rRes.aLineInfo.Count();

    if (nLines <= 0)
        nLines = 1;

    if (rRes.nFormatLines > nLines)
    {
        if (nStyle & DrawTextFlags::EndEllipsis)
        {
            rRes.nFormatLines = nLines - 1;

            ImplTextLineInfo& rLineInfo = rRes.aLineInfo.GetLine(rRes.nFormatLines);
            OUString aLastLine = convertLineEnd(rStr.copy(rLineInfo.GetIndex()), LINEEND_LF);

            OUStringBuffer aLastLineBuffer(aLastLine);
            sal_Int32 nLastLineLen = aLastLineBuffer.getLength();
            for (sal_Int32 i = 0; i < nLastLineLen; i++)
            {
                if (aLastLineBuffer[i] == '\n')
                    aLastLineBuffer[i] = ' ';
            }
            aLastLine = aLastLineBuffer.makeStringAndClear();

            rRes.aLastLine = rLayout.GetEllipsisString(aLastLine, nWidth, nStyle);

            rRes.nResultStyle &= ~DrawTextFlags(DrawTextFlags::VCenter | DrawTextFlags::Bottom);
            rRes.nResultStyle |= DrawTextFlags::Top;
        }
    }
    else
    {
        if (nMaxTextWidth <= nWidth)
            rRes.nResultStyle &= ~DrawTextFlags::Clip;
    }

    if (rRes.nFormatLines * nTextHeight > nHeight)
        rRes.nResultStyle |= DrawTextFlags::Clip;
}

} // namespace vcl::text
