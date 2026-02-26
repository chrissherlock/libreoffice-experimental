/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <o3tl/unit_conversion.hxx>

#include <vcl/font.hxx>
#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/vcllayout.hxx>

#include <font/FontController.hxx>
#include <font/FontMetricData.hxx>

#define UNDERLINE_LAST LINESTYLE_BOLDWAVE

namespace vcl::text
{
TextLineOffsetInfo::TextLineOffsetInfo(const FontMetricData& rMetric, FontLineStyle eUnderline,
                                       FontLineStyle eOverline, bool bUnderlineAbove)
{
    bUnderlineIsWave = (eUnderline == LINESTYLE_WAVE || eUnderline == LINESTYLE_BOLDWAVE
                        || eUnderline == LINESTYLE_DOUBLEWAVE);
    nUnderlineOffset = bUnderlineAbove ? rMetric.GetAscent() : rMetric.GetDescent();

    bOverlineIsWave = (eOverline == LINESTYLE_WAVE || eOverline == LINESTYLE_BOLDWAVE
                       || eOverline == LINESTYLE_DOUBLEWAVE);
    nOverlineOffset = rMetric.GetAscent();

    nStrikeoutOffset = rMetric.GetAscent() / 2;
}

StraightLineMetrics::StraightLineMetrics(const FontMetricData& rMetric, FontLineStyle eInUnderline,
                                         tools::Long nY, bool bIsAbove)
{
    eUnderline = eInUnderline;
    if (eUnderline > UNDERLINE_LAST)
        eUnderline = LINESTYLE_SINGLE;

    switch (eUnderline)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_DOTTED:
        case LINESTYLE_DASH:
        case LINESTYLE_LONGDASH:
        case LINESTYLE_DASHDOT:
        case LINESTYLE_DASHDOTDOT:
            if (bIsAbove)
            {
                nLineHeight = rMetric.GetAboveUnderlineSize();
                nLinePos = nY + rMetric.GetAboveUnderlineOffset();
            }
            else
            {
                nLineHeight = rMetric.GetUnderlineSize();
                nLinePos = nY + rMetric.GetUnderlineOffset();
            }
            break;
        case LINESTYLE_BOLD:
        case LINESTYLE_BOLDDOTTED:
        case LINESTYLE_BOLDDASH:
        case LINESTYLE_BOLDLONGDASH:
        case LINESTYLE_BOLDDASHDOT:
        case LINESTYLE_BOLDDASHDOTDOT:
            if (bIsAbove)
            {
                nLineHeight = rMetric.GetAboveBoldUnderlineSize();
                nLinePos = nY + rMetric.GetAboveBoldUnderlineOffset();
            }
            else
            {
                nLineHeight = rMetric.GetBoldUnderlineSize();
                nLinePos = nY + rMetric.GetBoldUnderlineOffset();
            }
            break;
        case LINESTYLE_DOUBLE:
            if (bIsAbove)
            {
                nLineHeight = rMetric.GetAboveDoubleUnderlineSize();
                nLinePos = nY + rMetric.GetAboveDoubleUnderlineOffset1();
                nLinePos2 = nY + rMetric.GetAboveDoubleUnderlineOffset2();
            }
            else
            {
                nLineHeight = rMetric.GetDoubleUnderlineSize();
                nLinePos = nY + rMetric.GetDoubleUnderlineOffset1();
                nLinePos2 = nY + rMetric.GetDoubleUnderlineOffset2();
            }
            break;
        default:
            break;
    }
}

std::vector<TextDashSegment> TextDecorator::CalculateTextLineSegments(tools::Long nWidth,
                                                                      FontLineStyle eStyle,
                                                                      tools::Long nLineHeight,
                                                                      tools::Long nDPIX,
                                                                      tools::Long nDPIY)
{
    std::vector<TextDashSegment> aSegments;
    tools::Long nLeft = 0;

    switch (eStyle)
    {
        case LINESTYLE_DOTTED:
        case LINESTYLE_BOLDDOTTED:
        {
            tools::Long nDotWidth = nLineHeight * nDPIY;
            nDotWidth += nDPIY / 2;
            nDotWidth /= nDPIY;

            tools::Long nTempWidth = nDotWidth;
            tools::Long nEnd = nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempWidth > nEnd)
                    nTempWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempWidth });
                nLeft += nDotWidth * 2;
            }
        }
        break;
        case LINESTYLE_DASH:
        case LINESTYLE_LONGDASH:
        case LINESTYLE_BOLDDASH:
        case LINESTYLE_BOLDLONGDASH:
        {
            tools::Long nDotWidth = nLineHeight * nDPIY;
            nDotWidth += nDPIY / 2;
            nDotWidth /= nDPIY;

            tools::Long nMinDashWidth;
            tools::Long nMinSpaceWidth;
            tools::Long nSpaceWidth;
            tools::Long nDashWidth;
            if ((eStyle == LINESTYLE_LONGDASH) || (eStyle == LINESTYLE_BOLDLONGDASH))
            {
                nMinDashWidth = nDotWidth * 6;
                nMinSpaceWidth = nDotWidth * 2;
                nDashWidth = 200;
                nSpaceWidth = 100;
            }
            else
            {
                nMinDashWidth = nDotWidth * 4;
                nMinSpaceWidth = (nDotWidth * 150) / 100;
                nDashWidth = 100;
                nSpaceWidth = 50;
            }
            nDashWidth = o3tl::convert(nDashWidth * nDPIX, o3tl::Length::mm100, o3tl::Length::in);
            nSpaceWidth = o3tl::convert(nSpaceWidth * nDPIX, o3tl::Length::mm100, o3tl::Length::in);

            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;
            if (nSpaceWidth < nMinSpaceWidth)
                nSpaceWidth = nMinSpaceWidth;

            tools::Long nTempWidth = nDashWidth;
            tools::Long nEnd = nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempWidth > nEnd)
                    nTempWidth = nEnd - nLeft;
                aSegments.push_back({ nLeft, nTempWidth });
                nLeft += nDashWidth + nSpaceWidth;
            }
        }
        break;
        case LINESTYLE_DASHDOT:
        case LINESTYLE_BOLDDASHDOT:
        {
            tools::Long nDotWidth = nLineHeight * nDPIY;
            nDotWidth += nDPIY / 2;
            nDotWidth /= nDPIY;

            tools::Long nDashWidth
                = o3tl::convert(100 * nDPIX, o3tl::Length::mm100, o3tl::Length::in);
            tools::Long nMinDashWidth = nDotWidth * 4;

            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;

            tools::Long nTempDotWidth = nDotWidth;
            tools::Long nTempDashWidth = nDashWidth;
            tools::Long nEnd = nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempDotWidth });
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDashWidth > nEnd)
                    nTempDashWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempDashWidth });
                nLeft += nDashWidth + nDotWidth;
            }
        }
        break;
        case LINESTYLE_DASHDOTDOT:
        case LINESTYLE_BOLDDASHDOTDOT:
        {
            tools::Long nDotWidth = nLineHeight * nDPIY;
            nDotWidth += nDPIY / 2;
            nDotWidth /= nDPIY;

            tools::Long nDashWidth
                = o3tl::convert(100 * nDPIX, o3tl::Length::mm100, o3tl::Length::in);
            tools::Long nMinDashWidth = nDotWidth * 4;

            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;

            tools::Long nTempDotWidth = nDotWidth;
            tools::Long nTempDashWidth = nDashWidth;
            tools::Long nEnd = nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempDotWidth });
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempDotWidth });
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDashWidth > nEnd)
                    nTempDashWidth = nEnd - nLeft;

                aSegments.push_back({ nLeft, nTempDashWidth });
                nLeft += nDashWidth + nDotWidth;
            }
        }
        break;
        default:
            break;
    }
    return aSegments;
}

WaveLineGeometry TextDecorator::CalculateWaveLineGeometry(const FontMetricData& rMetric,
                                                          FontLineStyle eStyle, bool bIsAbove,
                                                          tools::Long nDistY, tools::Long nDPIX,
                                                          tools::Long nDPIY)
{
    WaveLineGeometry aGeo;
    tools::Long nLineHeight;
    tools::Long nLinePos;

    if (bIsAbove)
    {
        nLineHeight = rMetric.GetAboveWavelineUnderlineSize();
        nLinePos = rMetric.GetAboveWavelineUnderlineOffset();
    }
    else
    {
        nLineHeight = rMetric.GetWavelineUnderlineSize();
        nLinePos = rMetric.GetWavelineUnderlineOffset();
    }

    if ((eStyle == LINESTYLE_SMALLWAVE) && (nLineHeight > 3))
        nLineHeight = 3;

    tools::Long nLineWidth = nDPIX / 300;
    if (!nLineWidth)
        nLineWidth = 1;

    if (eStyle == LINESTYLE_BOLDWAVE)
        nLineWidth *= 2;

    nLinePos += nDistY - (nLineHeight / 2);

    tools::Long nLineWidthHeight = ((nLineWidth * nDPIX) + (nDPIY / 2)) / nDPIY;

    if (eStyle == LINESTYLE_DOUBLEWAVE)
    {
        tools::Long nOrgLineHeight = nLineHeight;
        nLineHeight /= 3;
        if (nLineHeight < 2)
        {
            if (nOrgLineHeight > 1)
                nLineHeight = 2;
            else
                nLineHeight = 1;
        }

        tools::Long nLineDY = nOrgLineHeight - (nLineHeight * 2);
        if (nLineDY < nLineWidthHeight)
            nLineDY = nLineWidthHeight;

        tools::Long nLineDY2 = nLineDY / 2;
        if (!nLineDY2)
            nLineDY2 = 1;

        aGeo.aSegments.push_back({ nLinePos - (nLineWidthHeight - nLineDY2), nLineHeight });
        aGeo.aSegments.push_back(
            { nLinePos + (nLineWidthHeight - nLineDY2) + (nLineWidthHeight + nLineDY),
              nLineHeight });
    }
    else
    {
        aGeo.aSegments.push_back({ nLinePos - (nLineWidthHeight / 2), nLineHeight });
    }

    aGeo.nLineWidth = nLineWidth;
    return aGeo;
}

StrikeoutGeometry TextDecorator::CalculateStrikeoutGeometry(const FontMetricData& rMetric,
                                                            FontStrikeout eStrikeout,
                                                            tools::Long nDistY)
{
    StrikeoutGeometry aGeo;
    tools::Long nLineHeight = 0;
    tools::Long nLinePos = 0;
    tools::Long nLinePos2 = 0;

    if (eStrikeout > STRIKEOUT_X)
        eStrikeout = STRIKEOUT_SINGLE;

    switch (eStrikeout)
    {
        case STRIKEOUT_SINGLE:
            nLineHeight = rMetric.GetStrikeoutSize();
            nLinePos = nDistY + rMetric.GetStrikeoutOffset();
            if (nLineHeight > 0)
                aGeo.aSegments.push_back({ nLinePos, nLineHeight });
            break;
        case STRIKEOUT_BOLD:
            nLineHeight = rMetric.GetBoldStrikeoutSize();
            nLinePos = nDistY + rMetric.GetBoldStrikeoutOffset();
            if (nLineHeight > 0)
                aGeo.aSegments.push_back({ nLinePos, nLineHeight });
            break;
        case STRIKEOUT_DOUBLE:
            nLineHeight = rMetric.GetDoubleStrikeoutSize();
            nLinePos = nDistY + rMetric.GetDoubleStrikeoutOffset1();
            nLinePos2 = nDistY + rMetric.GetDoubleStrikeoutOffset2();
            if (nLineHeight > 0)
            {
                aGeo.aSegments.push_back({ nLinePos, nLineHeight });
                aGeo.aSegments.push_back({ nLinePos2, nLineHeight });
            }
            break;
        default:
            break;
    }

    return aGeo;
}

TextLineGeometry TextDecorator::GetTextLineGeometry(const TextLineRequest& rReq,
                                                    const FontMetricData& rMetric)
{
    TextLineGeometry aGeo;

    // Calculate Standard Pen Width (DPI / 300)
    aGeo.nLineWidth = rReq.nDPIX / 300;
    if (aGeo.nLineWidth < 1)
        aGeo.nLineWidth = 1;

    if (rReq.eUnderline != LINESTYLE_NONE)
    {
        if (rReq.eUnderline == LINESTYLE_DOUBLE || rReq.eUnderline == LINESTYLE_DOUBLEWAVE)
        {
            aGeo.nUnderlinePos1 = rReq.bUnderlineAbove ? rMetric.GetAboveDoubleUnderlineOffset1()
                                                       : rMetric.GetDoubleUnderlineOffset1();
            aGeo.nUnderlinePos2 = rReq.bUnderlineAbove ? rMetric.GetAboveDoubleUnderlineOffset2()
                                                       : rMetric.GetDoubleUnderlineOffset2();
        }
        else if (rReq.eUnderline >= LINESTYLE_BOLD && rReq.eUnderline <= LINESTYLE_BOLDDASHDOTDOT)
        {
            aGeo.nUnderlinePos1 = rReq.bUnderlineAbove ? rMetric.GetAboveBoldUnderlineOffset()
                                                       : rMetric.GetBoldUnderlineOffset();
        }
        else
        {
            aGeo.nUnderlinePos1 = rReq.bUnderlineAbove ? rMetric.GetAboveUnderlineOffset()
                                                       : rMetric.GetUnderlineOffset();
        }

        if (rReq.eUnderline == LINESTYLE_WAVE || rReq.eUnderline == LINESTYLE_BOLDWAVE
            || rReq.eUnderline == LINESTYLE_SMALLWAVE || rReq.eUnderline == LINESTYLE_DOUBLEWAVE)
        {
            aGeo.bUnderlineIsWave = true;
            tools::Long nHeight = rReq.bUnderlineAbove ? rMetric.GetAboveWavelineUnderlineSize()
                                                       : rMetric.GetWavelineUnderlineSize();

            if (rReq.eUnderline == LINESTYLE_SMALLWAVE && nHeight > 3)
                nHeight = 3;

            aGeo.nUnderlineWaveHeight = nHeight;

            if (rReq.eUnderline == LINESTYLE_BOLDWAVE)
                aGeo.nLineWidth *= 2;
        }
    }

    if (rReq.eOverline != LINESTYLE_NONE)
    {
        aGeo.nOverlinePos1 = rMetric.GetAboveUnderlineOffset();

        if (rReq.eOverline == LINESTYLE_WAVE || rReq.eOverline == LINESTYLE_BOLDWAVE
            || rReq.eOverline == LINESTYLE_SMALLWAVE || rReq.eOverline == LINESTYLE_DOUBLEWAVE)
        {
            aGeo.bOverlineIsWave = true;
            aGeo.nOverlineWaveHeight = rMetric.GetAboveWavelineUnderlineSize();
        }
    }

    if (rReq.eStrikeout != STRIKEOUT_NONE)
    {
        if (rReq.eStrikeout == STRIKEOUT_SLASH || rReq.eStrikeout == STRIKEOUT_X)
        {
            aGeo.bStrikeoutIsChar = true;
        }
        else if (rReq.eStrikeout == STRIKEOUT_DOUBLE)
        {
            aGeo.nStrikeoutPos1 = rMetric.GetDoubleStrikeoutOffset1();
            aGeo.nStrikeoutPos2 = rMetric.GetDoubleStrikeoutOffset2();
        }
        else if (rReq.eStrikeout == STRIKEOUT_BOLD)
        {
            aGeo.nStrikeoutPos1 = rMetric.GetBoldStrikeoutOffset();
        }
        else
        {
            aGeo.nStrikeoutPos1 = rMetric.GetStrikeoutOffset();
        }
    }

    return aGeo;
}

void TextDecorator::GetEmphasisMarkPositions(const SalLayout& rSalLayout,
                                             const vcl::font::FontRealization& rFontRealization,
                                             const vcl::font::EmphasisMark& rMark,
                                             bool bEmphasisBelow, std::vector<Point>& rPoints)
{
    rPoints.clear();
    if (!rFontRealization.mxFont)
        return;

    // Calculate base anchor (Ascent or Descent line)
    const tools::Long nBaseOffset
        = bEmphasisBelow ? rFontRealization.nEmphasisDescent : -rFontRealization.nEmphasisAscent;
    const basegfx::B2DPoint aDrawPos = rSalLayout.GetDrawPosition();
    const tools::Long nAnchorY = aDrawPos.getY() + nBaseOffset;

    // Prepare visual adjustments (centering and mark-specific offset)
    const tools::Long nXCenterOff = rMark.GetWidth() / 2;
    const tools::Long nYCenterOff
        = (bEmphasisBelow ? rFontRealization.nEmphasisDescent : rFontRealization.nEmphasisAscent)
          / 2;
    const tools::Long nShapeAdj = bEmphasisBelow ? rMark.GetYOffset() : -rMark.GetYOffset();

    int nStart = 0;
    const GlyphItem* pGlyph = nullptr;
    basegfx::B2DPoint aPos;

    while (rSalLayout.GetNextGlyph(&pGlyph, aPos, nStart))
    {
        if (!pGlyph)
            continue;

        Point aMarkPt(static_cast<tools::Long>(aPos.getX()) - nXCenterOff,
                      nAnchorY + nShapeAdj - nYCenterOff);

        rPoints.push_back(aMarkPt);
    }
}

// #109280# make sure the waveline does not exceed the descent to avoid paint problems
void TextDecorator::SanitizeWaveLineHeight(tools::Long& rWaveHeight, tools::Long& rLineWidth,
                                           const FontMetricData& rFontMetric)
{
    if (rWaveHeight > rFontMetric.GetWavelineUnderlineSize()
#ifdef MACOSX
        || !SkiaHelper::isVCLSkiaEnabled()
#endif
    )
    {
        rWaveHeight = rFontMetric.GetWavelineUnderlineSize();
        rLineWidth = 0; // tdf#124848 hairline
    }
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
