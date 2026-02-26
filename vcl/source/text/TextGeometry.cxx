/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/degree.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <o3tl/unit_conversion.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/metric.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/CaretManager.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/text/TextSpan.hxx>

#include <font/FontController.hxx>
#include <sallayout.hxx>
#include <textlineinfo.hxx>
#include <textlayout.hxx>
#include <text/TextAnalyzer.hxx>
#include <text/TextLayoutPositioning.hxx>
#include <text/TextJustifier.hxx>
#include <text/TextLayoutEngine.hxx>
#include <CoordinateMapper.hxx>

#include <cmath>

namespace vcl::text
{
tools::Rectangle TextGeometry::AlignAndRotateTextRect(const tools::Rectangle& rTargetRect,
                                                      tools::Long nContentWidth,
                                                      tools::Long nContentHeight,
                                                      DrawTextFlags nStyle, Degree10 nOrientation)
{
    tools::Rectangle aRect = rTargetRect;

    // Horizontal Alignment
    if (nStyle & DrawTextFlags::Right)
    {
        aRect.SetLeft(aRect.Right() - nContentWidth + 1);
    }
    else if (nStyle & DrawTextFlags::Center)
    {
        aRect.AdjustLeft((rTargetRect.GetWidth() - nContentWidth) / 2);
        aRect.SetRight(aRect.Left() + nContentWidth - 1);
    }
    else
    {
        aRect.SetRight(aRect.Left() + nContentWidth - 1);
    }

    // Vertical Alignment
    if (nStyle & DrawTextFlags::Bottom)
    {
        aRect.SetTop(aRect.Bottom() - nContentHeight + 1);
    }
    else if (nStyle & DrawTextFlags::VCenter)
    {
        aRect.AdjustTop((aRect.GetHeight() - nContentHeight) / 2);
        aRect.SetBottom(aRect.Top() + nContentHeight - 1);
    }
    else
    {
        aRect.SetBottom(aRect.Top() + nContentHeight - 1);
    }

    // Rounding adjustment (legacy behavior)
    if (nStyle & DrawTextFlags::Right)
        aRect.AdjustLeft(-1);
    else
        aRect.AdjustRight(1);

    // Rotation
    if (nOrientation != 0_deg10)
    {
        tools::Polygon aRotatedPolygon(aRect);
        aRotatedPolygon.Rotate(Point(aRect.GetWidth() / 2, aRect.GetHeight() / 2), nOrientation);
        return aRotatedPolygon.GetBoundRect();
    }

    return aRect;
}

Point TextGeometry::GetRotationOrigin(const Point& rPos, const Size& rTextSize,
                                      Degree10 nOrientation, TextAlign eAlign)
{
    if (nOrientation == 0_deg10)
        return rPos;

    tools::Long nX = rPos.X();
    tools::Long nY = rPos.Y();
    tools::Long nAlignOfs = 0;

    if (eAlign == ALIGN_BOTTOM)
        nAlignOfs = -rTextSize.Height();
    else if (eAlign == ALIGN_TOP)
        nAlignOfs = rTextSize.Height();

    double fRad = toRadians(nOrientation);
    double fCos = cos(fRad);
    double fSin = sin(fRad);

    nX += basegfx::fround<tools::Long>(-nAlignOfs * fSin);
    nY += basegfx::fround<tools::Long>(nAlignOfs * fCos);

    return Point(nX, nY);
}

RotatedGeometry TextGeometry::GetRotatedGeometry(const Point& rBase,
                                                 const tools::Rectangle& rLocalRect,
                                                 Degree10 nOrientation)
{
    RotatedGeometry aGeo;
    aGeo.mbIsPolygon = false;

    // Optimization: Handle orthogonal rotations (0, 90, 180, 270)
    if (nOrientation.get() % 900 == 0)
    {
        tools::Long nX = rLocalRect.Left();
        tools::Long nY = rLocalRect.Top();
        tools::Long nW = rLocalRect.GetWidth();
        tools::Long nH = rLocalRect.GetHeight();

        // Dimension Swap for 90 and 270
        if (nOrientation.get() % 1800 != 0)
            std::swap(nW, nH);

        // Coordinate Transformation
        if (nOrientation == 900_deg10)
        {
            tools::Long nOrigX = nX;
            nX = nY;
            nY = -nOrigX - nH;
        }
        else if (nOrientation == 1800_deg10)
        {
            nX = -nX - nW;
            nY = -nY - nH;
        }
        else if (nOrientation == 2700_deg10)
        {
            tools::Long nOrigX = nX;
            nX = -nY - nW;
            nY = nOrigX;
        }

        // Apply Base Translation
        nX += rBase.X();
        nY += rBase.Y();

        aGeo.maRect = tools::Rectangle(Point(nX, nY), Size(nW, nH));
        return aGeo;
    }

    // Fallback: Arbitrary rotation
    tools::Long nX = rLocalRect.Left() + rBase.X();
    tools::Long nY = rLocalRect.Top() + rBase.Y();

    tools::Rectangle aRotRect(Point(nX, nY),
                              Size(rLocalRect.GetWidth() + 1, rLocalRect.GetHeight() + 1));

    aGeo.maPoly = tools::Polygon(aRotRect);
    aGeo.maPoly.Rotate(rBase, nOrientation);
    aGeo.mbIsPolygon = true;

    return aGeo;
}

Point TextGeometry::GetRotatedImageOrigin(const Point& rBase, const tools::Rectangle& rLocalBounds,
                                          Degree10 nOrientation)
{
    tools::Polygon aPoly(rLocalBounds);
    aPoly.Rotate(Point(0, 0), nOrientation);
    return rBase + aPoly.GetBoundRect().TopLeft();
}

tools::Long TextGeometry::GetMirroredX(const MirroringContext& rCtx)
{
    tools::Long nMirroredX = rCtx.nX;

    if (rCtx.bHasMirroredGraphics)
    {
        nMirroredX = rCtx.nGraphicsWidth - 1 - rCtx.nX;

        if (!rCtx.bIsRTL)
        {
            tools::Long nDevX = rCtx.nGraphicsWidth - rCtx.nOutputWidth - rCtx.nOutOffX;
            nMirroredX = nDevX + (rCtx.nOutputWidth - 1 - (nMirroredX - nDevX));
        }
    }
    else if (rCtx.bIsRTL)
    {
        tools::Long nDevX = rCtx.nOutOffX;
        nMirroredX = rCtx.nOutputWidth - 1 - (rCtx.nX - nDevX) + nDevX;
    }

    return nMirroredX;
}

tools::Long TextGeometry::GetReliefOffset(sal_Int32 nDPIX, FontRelief eRelief)
{
    const tools::Long nOff = 1 + (nDPIX / 300);
    return (eRelief == FontRelief::Engraved) ? -nOff : nOff;
}

tools::Long TextGeometry::GetShadowOffset(tools::Long nLineHeight, bool bIsOutline)
{
    tools::Long nOff = 1 + ((nLineHeight - 24) / 24);
    if (bIsOutline)
        nOff++;
    return nOff;
}

const std::vector<basegfx::B2DPoint>& TextGeometry::GetOutlineOffsets()
{
    static const std::vector<basegfx::B2DPoint> aOffsets{ { -1, -1 }, { +1, +1 }, { -1, 0 },
                                                          { -1, +1 }, { +0, +1 }, { +0, -1 },
                                                          { +1, -1 }, { +1, +0 } };
    return aOffsets;
}

MnemonicGeometry TextGeometry::GetMnemonicGeometry(
    std::function<double(tools::Long)> const& fnLogicWidthToDeviceSubPixel,
    std::function<tools::Long(tools::Long)> const& fnLogicWidthToDevicePixel,
    std::function<Point(const Point&)> const& fnLogicToPixel, const MnemonicDeviceParams& rParams,
    KernArraySpan aDXArray, sal_Int32 nRelPos, const Point& rLinePos, bool bTrailing)
{
    MnemonicGeometry aGeo;

    sal_Int32 lc_x1 = nRelPos ? static_cast<sal_Int32>(aDXArray[nRelPos - 1]) : 0;
    sal_Int32 lc_x2 = static_cast<sal_Int32>(aDXArray[nRelPos]);

    aGeo.nWidth = static_cast<tools::Long>(fnLogicWidthToDeviceSubPixel(std::abs(lc_x1 - lc_x2)));

    Point aTempPos = fnLogicToPixel(rLinePos);

    aGeo.nY = rParams.nOutOffY + aTempPos.Y() + fnLogicWidthToDevicePixel(rParams.nLogicalAscent);

    sal_Int32 nCharOffset = bTrailing ? std::max(lc_x1, lc_x2) : std::min(lc_x1, lc_x2);

    aGeo.nX = rParams.nOutOffX + aTempPos.X() + fnLogicWidthToDevicePixel(nCharOffset);

    return aGeo;
}

Point TextGeometry::CalculateLayoutOrigin(const OutputDevice& rDev, const tools::Rectangle& rRect,
                                          tools::Long nTextWidth, tools::Long nTextHeight,
                                          DrawTextFlags nStyle, TextAlign eAlign)
{
    Point aPos = rRect.TopLeft();
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();

    // Horizontal text alignment
    if (nStyle & DrawTextFlags::Right)
        aPos.AdjustX(nWidth - nTextWidth);
    else if (nStyle & DrawTextFlags::Center)
        aPos.AdjustX((nWidth - nTextWidth) / 2);

    // Vertical font alignment
    if (eAlign == ALIGN_BOTTOM)
        aPos.AdjustY(nTextHeight);
    else if (eAlign == ALIGN_BASELINE)
        aPos.AdjustY(rDev.GetFontMetric().GetAscent());

    if (nStyle & DrawTextFlags::Bottom)
        aPos.AdjustY(nHeight - nTextHeight);
    else if (nStyle & DrawTextFlags::VCenter)
        aPos.AdjustY((nHeight - nTextHeight) / 2);

    return aPos;
}

bool TextGeometry::GetTextOutlines(const LayoutResources& rResources,
                                   basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                                   sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                   sal_uLong nLayoutWidth, std::span<const double> pDXArray,
                                   std::span<const sal_Bool> pKashidaArray)
{
    bool bRet = false;
    rVector.clear();

    if (nLen < 0)
        nLen = rStr.getLength() - nIndex;

    rVector.reserve(nLen);

    std::unique_ptr<SalLayout> pSalLayout;
    double nXOffset = 0;

    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;

        pSalLayout = TextLayoutEngine::Layout(
            rResources, vcl::text::TextSpan{ rStr, nStart, nOfsLen },
            vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                          pDXArray, pKashidaArray, SalLayoutFlags::NONE },
            vcl::text::LayoutCacheData{ nullptr, nullptr }, {});

        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            pSalLayout.reset();
            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = TextLayoutEngine::Layout(
        rResources, vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth), pDXArray,
                                      pKashidaArray, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, {});

    if (pSalLayout)
    {
        bRet = pSalLayout->GetOutline(rVector);

        if (bRet)
        {
            basegfx::B2DHomMatrix aMatrix = TextGeometry::CalculateOutlineTransform(
                *pSalLayout, rResources.rFontRealization, nXOffset);

            if (!aMatrix.isIdentity())
            {
                for (auto& elem : rVector)
                {
                    elem.transform(aMatrix);
                }
            }
        }
    }

    return bRet;
}

bool TextGeometry::GetLogicalTextBoundRect(const LayoutResources& rRes,
                                           basegfx::B2DRectangle& rRect, const OUString& rStr,
                                           sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                           sal_uLong nLayoutWidth, std::span<const double> pDXArray,
                                           std::span<const sal_Bool> pKashidaArray,
                                           const SalLayoutGlyphs* pGlyphs)
{
    bool bRet = false;
    rRect.reset();

    double nXOffset = 0;
    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;

        std::unique_ptr<SalLayout> pOfsLayout = TextLayoutEngine::Layout(
            rRes, vcl::text::TextSpan{ rStr, nStart, nOfsLen },
            vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                          pDXArray, pKashidaArray, SalLayoutFlags::NONE },
            vcl::text::LayoutCacheData{ nullptr, nullptr }, {});

        if (pOfsLayout)
        {
            nXOffset = pOfsLayout->GetTextWidth();

            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    std::unique_ptr<SalLayout> pSalLayout = TextLayoutEngine::Layout(
        rRes, vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth), pDXArray,
                                      pKashidaArray, SalLayoutFlags::NONE },
        vcl::text::LayoutCacheData{ nullptr, pGlyphs }, {});

    if (pSalLayout)
    {
        basegfx::B2DRectangle aPixelRect;
        bRet = pSalLayout->GetBoundRect(aPixelRect);

        if (bRet)
        {
            basegfx::B2DPoint aPos = pSalLayout->GetDrawPosition(basegfx::B2DPoint(nXOffset, 0));
            aPixelRect.translate(rRes.rFontRealization.nXOffset - aPos.getX(),
                                 rRes.rFontRealization.nYOffset - aPos.getY());
            rRect = rRes.rMapper.PixelToLogic(aPixelRect);

            if (rRes.rMapper.IsMapModeEnabled())
                rRect.translate(rRes.rMapper.GetMappingXOffset(), rRes.rMapper.GetMappingYOffset());
        }
    }

    return bRet;
}

void TextGeometry::GetWordLineSegments(const SalLayout& rSalLayout,
                                       const vcl::font::FontRealization& rRealization,
                                       std::vector<std::pair<double, double>>& rSegments)
{
    rSegments.clear();
    const basegfx::B2DPoint aStartPt = rSalLayout.DrawBase();
    const LogicalFontInstance* pFont = rRealization.mxFont.get();
    const Degree10 nOrientation = pFont ? pFont->mnOrientation : 0_deg10;

    basegfx::B2DPoint aPos;
    double nDist = 0;
    double nWidth = 0;
    const GlyphItem* pGlyph = nullptr;
    int nStart = 0;

    while (rSalLayout.GetNextGlyph(&pGlyph, aPos, nStart))
    {
        if (!pGlyph->IsSpacing())
        {
            if (nWidth == 0)
            {
                nDist = aPos.getX() - aStartPt.getX();
                if (nOrientation)
                {
                    const double nDY = aPos.getY() - aStartPt.getY();
                    const double fRad = toRadians(nOrientation);
                    nDist = nDist * cos(fRad) - nDY * sin(fRad);
                }
            }
            nWidth += pGlyph->newWidth();
        }
        else if (nWidth > 0)
        {
            rSegments.push_back({ nDist, nWidth });
            nWidth = 0;
        }
    }
    if (nWidth > 0)
        rSegments.push_back({ nDist, nWidth });
}

void TextGeometry::GetGlyphRectsFromLayout(const SalLayout& rLayout, const Point& rStartPt,
                                           const OUString& rStr, sal_Int32 nLen,
                                           std::vector<tools::Rectangle>& rRects)
{
    rRects.clear();
    rRects.reserve(nLen);

    if (nLen <= 0)
        return;

    // Fix 1: Use double vector (as per vcllayout.hxx) and pass string
    std::vector<double> aDXArray;
    rLayout.FillDXArray(&aDXArray, rStr);

    tools::Long nX = rStartPt.X();

    // Fix 2: GetBoundRect returns bool and takes B2DRectangle ref
    basegfx::B2DRectangle aBounds;
    rLayout.GetBoundRect(aBounds);

    // Convert B2DRectangle to integers relative to start point
    tools::Long nTop = rStartPt.Y() + static_cast<tools::Long>(aBounds.getMinY());
    tools::Long nBottom = rStartPt.Y() + static_cast<tools::Long>(aBounds.getMaxY());

    // Safety check for empty bounds
    if (aBounds.isEmpty())
    {
        nTop = rStartPt.Y();
        nBottom = rStartPt.Y() + 10; // Fallback height
    }

    tools::Long nPrevX = 0;
    size_t nArraySize = aDXArray.size();

    for (int i = 0; i < nLen; ++i)
    {
        if (static_cast<size_t>(i) >= nArraySize)
            break;

        // FillDXArray returns absolute positions
        tools::Long nCurrX = static_cast<tools::Long>(aDXArray[i]);

        rRects.emplace_back(nX + nPrevX, nTop, nX + nCurrX, nBottom);

        nPrevX = nCurrX;
    }
}

TextGeometry::LayoutResult TextGeometry::CalculateLayout(const CoordinateMapper& rMapper,
                                                         const LayoutRequest& rReq,
                                                         const vcl::TextLayoutCommon& rLayout)
{
    LayoutResult aRes;
    OUString aCleanText = rReq.aText;

    if (rReq.nStyle & DrawTextFlags::Mnemonic)
        aCleanText = removeMnemonicFromString(aCleanText);

    aRes.aDisplayText = aCleanText;
    tools::Long nAvailableWidth = rReq.aTargetRect.GetWidth();
    tools::Long nLineHeight = rReq.nFontHeight ? rReq.nFontHeight : 1;

    if (rReq.nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        rLayout.GetTextLines(rReq.aTargetRect, nLineHeight, aMultiLineInfo, nAvailableWidth,
                             aCleanText, rReq.nStyle);

        aRes.nLineCount = aMultiLineInfo.Count();
        for (sal_Int32 i = 0; i < aRes.nLineCount; ++i)
        {
            aRes.nMaxWidth = std::max(aRes.nMaxWidth, aMultiLineInfo.GetLine(i).GetWidth());
        }

        // Multi-line Ellipsis Check
        sal_Int32 nMaxLines = static_cast<sal_Int32>(rReq.aTargetRect.GetHeight() / nLineHeight);
        if (nMaxLines > 0 && aRes.nLineCount > nMaxLines
            && (rReq.nStyle & DrawTextFlags::EndEllipsis))
        {
            aRes.bEllipsisGenerated = true;
            aRes.nLineCount = nMaxLines; // Clamp height to visible area
        }
    }
    else
    {
        aRes.nMaxWidth = rLayout.GetTextWidth(aCleanText, 0, -1);

        if (aRes.nMaxWidth > nAvailableWidth && (rReq.nStyle & TEXT_DRAW_ELLIPSIS))
        {
            aRes.aDisplayText = rLayout.GetEllipsisString(aCleanText, nAvailableWidth, rReq.nStyle);
            aRes.nMaxWidth = nAvailableWidth;
            aRes.bEllipsisGenerated = true;
        }
    }

    // Coordinate Calculation (Alignment & Rotation)
    // Account for the full block height in multi-line scenarios
    aRes.aTextRect = TextGeometry::AlignAndRotateTextRect(rReq.aTargetRect, aRes.nMaxWidth,
                                                          nLineHeight * aRes.nLineCount,
                                                          rReq.nStyle, rReq.nFontOrientation);

    aRes.aDrawPosition = aRes.aTextRect.TopLeft();

    // Mnemonic Geometry
    if (rReq.nMnemonicPos != -1
        && TextAnalyzer::IsMnemonicInRange(rReq.nMnemonicPos, 0, aRes.aDisplayText.getLength()))
    {
        MnemonicDeviceParams aParams{ rReq.nFontAscent, 0, 0 };
        KernArray aDXArray;
        rLayout.GetTextArray(aRes.aDisplayText, &aDXArray, 0, -1, true);

        aRes.aMnemonic = vcl::text::TextGeometry::GetMnemonicGeometry(
            [&](tools::Long w) { return rMapper.LogicWidthToDeviceSubPixel(w); },
            [&](tools::Long w) { return rMapper.LogicWidthToDevicePixel(w); },
            [&](const Point& p) { return rMapper.LogicToPixel(p); }, aParams, aDXArray,
            rReq.nMnemonicPos, aRes.aDrawPosition);
        aRes.bHasMnemonic = true;
    }

    return aRes;
}

std::unique_ptr<SalLayout> TextGeometry::GetStrikeoutCharLayout(const LayoutResources& rRes,
                                                                tools::Long nTargetWidth,
                                                                FontStrikeout eStrikeout)
{
    if (nTargetWidth <= 0)
        return nullptr;

    const char cStrikeoutChar = (eStrikeout == STRIKEOUT_SLASH) ? '/' : 'X';
    static const int nTestStrLen = 4;
    static const int nMaxStrikeStrLen = 2048;

    sal_Unicode aChars[nMaxStrikeStrLen + 1]; // +1 for safety
    for (int i = 0; i < nTestStrLen; ++i)
        aChars[i] = cStrikeoutChar;

    OUString aStrikeoutTest(aChars, nTestStrLen);

    // Measure the width of the strikeout character
    vcl::text::TextSpan aSpan(aStrikeoutTest, 0, nTestStrLen);
    vcl::text::LayoutConstraints aConstraints(Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE);
    vcl::text::LayoutCacheData aCache;
    vcl::text::RenderSelection aSel;

    std::unique_ptr<SalLayout> pLayout
        = TextLayoutEngine::Layout(rRes, aSpan, aConstraints, aCache, aSel);

    tools::Long nStrikeoutWidth = 0;

    if (pLayout)
        nStrikeoutWidth = pLayout->GetTextWidth() / nTestStrLen;

    if (nStrikeoutWidth <= 0)
        return nullptr;

    int nStrikeStrLen = (nTargetWidth + (nStrikeoutWidth - 1)) / nStrikeoutWidth;

    if (nStrikeStrLen > nMaxStrikeStrLen)
        nStrikeStrLen = nMaxStrikeStrLen;
    else if (nStrikeStrLen < 0)
        nStrikeStrLen = 0;

    // Build the full strikeout string
    for (int i = nTestStrLen; i < nStrikeStrLen; ++i)
        aChars[i] = cStrikeoutChar;

    const OUString aStrikeoutText(aChars, nStrikeStrLen);
    vcl::text::TextSpan aFinalSpan(aStrikeoutText, 0, nStrikeStrLen);

    // Create the final layout with BiDiStrong forced
    vcl::text::LayoutConstraints aFinalConstraints(Point(0, 0), 0, {}, {},
                                                   SalLayoutFlags::BiDiStrong);

    return TextLayoutEngine::Layout(rRes, aFinalSpan, aFinalConstraints, aCache, aSel);
}

tools::Rectangle TextGeometry::GetTextInkBounds(const SalLayout& rSalLayout,
                                                const vcl::font::FontRealization& rFontRealization,
                                                bool bApplyRotation)
{
    const basegfx::B2DPoint aPoint = rSalLayout.GetDrawPosition();
    tools::Long nX = aPoint.getX();
    tools::Long nY
        = aPoint.getY()
          - (rFontRealization.mxFont->mxFontMetric->GetAscent() + rFontRealization.nEmphasisAscent);

    double nWidth = rSalLayout.GetTextWidth();
    tools::Long nHeight = rFontRealization.mxFont->mnLineHeight + rFontRealization.nEmphasisAscent
                          + rFontRealization.nEmphasisDescent;

    basegfx::B2DRectangle aBoundRect;
    if (rSalLayout.GetBoundRect(aBoundRect))
        return SalLayout::BoundRect2Rectangle(aBoundRect);

    if (bApplyRotation && rFontRealization.mxFont->mnOrientation)
    {
        const tools::Long nBaseX = nX;
        const tools::Long nBaseY = nY;
        if (!(rFontRealization.mxFont->mnOrientation % 900_deg10))
        {
            tools::Long nX2 = nX + nWidth;
            tools::Long nY2 = nY + nHeight;
            Point aBasePt(nBaseX, nBaseY);
            aBasePt.RotateAround(nX, nY, rFontRealization.mxFont->mnOrientation);
            aBasePt.RotateAround(nX2, nY2, rFontRealization.mxFont->mnOrientation);
            nWidth = nX2 - nX;
            nHeight = nY2 - nY;
        }
        else
        {
            tools::Rectangle aRect(Point(nX, nY), Size(nWidth + 1, nHeight + 1));
            tools::Polygon aPoly(aRect);
            aPoly.Rotate(Point(nBaseX, nBaseY), rFontRealization.mxFont->mnOrientation);
            return aPoly.GetBoundRect();
        }
    }
    return tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight));
}

static void lcl_convertBoundRectToLogic(const SalLayout& rLayout, const CoordinateMapper& rMapper,
                                        std::optional<tools::Rectangle>* pBounds)
{
    if (!pBounds)
        return;

    basegfx::B2DRectangle aB2DRect;

    if (rLayout.GetBoundRect(aB2DRect))
    {
        tools::Rectangle aRect = SalLayout::BoundRect2Rectangle(aB2DRect);
        *pBounds = rMapper.DevicePixelToLogic(aRect);
    }
}

double TextGeometry::FillPartialTextArray(const LayoutResources& rRes, const SalLayout& rLayout,
                                          KernArray* pKernArray, sal_Int32 nIndex, sal_Int32 nLen,
                                          sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                          const OUString& rCaretStr)
{
    std::vector<double> aDXPixelArray;
    std::vector<double>* pDXPixelArray = nullptr;

    if (pKernArray)
    {
        aDXPixelArray.resize(nPartLen);
        pDXPixelArray = &aDXPixelArray;
    }

    double nWidth = 0.0;

    if (nIndex == nPartIndex && nLen == nPartLen)
        nWidth = rLayout.FillDXArray(pDXPixelArray, rCaretStr);
    else
        nWidth
            = rLayout.FillPartialDXArray(pDXPixelArray, rCaretStr, nPartIndex - nIndex, nPartLen);

    if (pDXPixelArray)
    {
        for (int i = 1; i < nPartLen; ++i)
            (*pDXPixelArray)[i] += (*pDXPixelArray)[i - 1];

        if (rRes.rMapper.IsMapModeEnabled())
        {
            for (int i = 0; i < nPartLen; ++i)
            {
                (*pDXPixelArray)[i]
                    = rRes.rMapper.DevicePixelToLogicWidthDouble((*pDXPixelArray)[i]);
            }
        }

        pKernArray->resize(nPartLen);

        for (int i = 0; i < nPartLen; ++i)
        {
            (*pKernArray)[i] = (*pDXPixelArray)[i];
        }
    }

    return rRes.rMapper.DevicePixelToLogicWidthDouble(nWidth);
}

double TextGeometry::GetPartialTextArray(const LayoutResources& rRes,
                                         const vcl::text::TextSpan& rSpan, KernArray* pKernArray,
                                         sal_Int32 nPartIndex, sal_Int32 nPartLen, bool bCaret,
                                         const vcl::text::LayoutCacheData& rCache,
                                         std::optional<tools::Rectangle>* pBounds)
{
    if (rSpan.Index >= rSpan.Text.getLength())
        return 0.0;

    sal_Int32 nLen = TextAnalyzer::GetNormalizedLength(rSpan.Text, rSpan.Index, rSpan.Length);
    sal_Int32 nNormalizedPartLen
        = TextAnalyzer::GetNormalizedLength(rSpan.Text, nPartIndex, nPartLen);

    vcl::text::TextSpan aNormalizedSpan{ rSpan.Text, rSpan.Index, nLen };
    vcl::text::LayoutConstraints aConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE };

    vcl::text::RenderSelection aSelection;

    if (rSpan.Index != nPartIndex || nLen != nNormalizedPartLen)
    {
        aSelection
            = vcl::text::RenderSelection{ nPartIndex, nPartIndex, nPartIndex + nNormalizedPartLen };
    }

    std::unique_ptr<SalLayout> pSalLayout
        = TextLayoutEngine::Layout(rRes, aNormalizedSpan, aConstraints, rCache, aSelection);

    if (!pSalLayout)
    {
        TextJustifier::ZeroFillKernArray(pKernArray, nNormalizedPartLen);
        return 0.0;
    }

    lcl_convertBoundRectToLogic(*pSalLayout, rRes.rMapper, pBounds);

    return FillPartialTextArray(rRes, *pSalLayout, pKernArray, rSpan.Index, nLen, nPartIndex,
                                nNormalizedPartLen, bCaret ? rSpan.Text : OUString());
}

void TextGeometry::GetCaretPositions(const LayoutResources& rRes, const vcl::text::TextSpan& rSpan,
                                     std::vector<double>& rCaretPositions,
                                     const LayoutCacheData& rCache)
{
    vcl::text::LayoutConstraints aConstraints;
    vcl::text::RenderSelection aSelection;
    std::unique_ptr<SalLayout> pLayout
        = TextLayoutEngine::Layout(rRes, rSpan, aConstraints, rCache, aSelection);

    if (pLayout)
        CaretManager::GetCaretPositions(rRes, rSpan, rCaretPositions, *pLayout);
}

basegfx::B2DHomMatrix TextGeometry::CalculateOutlineTransform(
    const SalLayout& rLayout, const vcl::font::FontRealization& rRealization, double nXOffset)
{
    basegfx::B2DHomMatrix aMatrix;

    if (nXOffset != 0 || rRealization.nXOffset != 0 || rRealization.nYOffset != 0)
    {
        basegfx::B2DPoint aRotatedOfs(rRealization.nXOffset, rRealization.nYOffset);
        aRotatedOfs -= rLayout.GetDrawPosition(basegfx::B2DPoint(nXOffset, 0));
        aMatrix.translate(aRotatedOfs.getX(), aRotatedOfs.getY());
    }

    return aMatrix;
}

basegfx::B2DPoint TextGeometry::MapLogicalToDevicePos(const LayoutResources& rRes,
                                                      const Point& rLogicalPos)
{
    // Use subpixel precision if MapMode is on OR if we were explicitly told to (e.g. PDF/Subpixel flag)
    if (rRes.rMapper.IsMapModeEnabled() || rRes.bSubpixelPositioning)
        return rRes.rMapper.LogicToDeviceSubPixel(rLogicalPos);

    Point aDevicePos = rRes.rMapper.LogicToDevicePixel(rLogicalPos);
    return basegfx::B2DPoint(aDevicePos.X(), aDevicePos.Y());
}

void TextGeometry::ApplyPositioning(const LayoutResources& rRes, SalLayout& rLayout,
                                    vcl::text::TextLayoutRequest& rArgs, const Point& rLogicalPos,
                                    double nEndGlyphCoord)
{
    TextLayoutPositioning aPos;
    aPos.bSubpixelPositioning = rRes.bSubpixelPositioning;

    aPos.bRightAlign = bool(rArgs.mnFlags & SalLayoutFlags::RightAlign);
    aPos.nEndGlyphCoord = nEndGlyphCoord;
    aPos.aDrawBase = MapLogicalToDevicePos(rRes, rLogicalPos);

    TextJustifier::JustifyLayout(rLayout, rArgs);
    TextJustifier::ApplyHorizontalOffset(rLayout, rArgs, aPos);
    TextJustifier::SetAnchorPoint(rLayout, aPos);
}

double TextGeometry::GetTextHeightPixel(const vcl::font::FontRealization& rRealization)
{
    if (!rRealization.mxFont)
        return 0.0;

    return static_cast<double>(rRealization.mxFont->mnLineHeight + rRealization.nEmphasisAscent
                               + rRealization.nEmphasisDescent);
}

double TextGeometry::CalculateWaveLineOrientation(const Point& rStartPt, const Point& rEndPt)
{
    tools::Long nStartX = rStartPt.X();
    tools::Long nStartY = rStartPt.Y();
    tools::Long nEndX = rEndPt.X();
    tools::Long nEndY = rEndPt.Y();

    if (nStartY != nEndY || nStartX > nEndX)
        return basegfx::rad2deg(std::atan2(nStartY - nEndY, nEndX - nStartX));

    return 0.0;
}

} // namespace vcl::text
