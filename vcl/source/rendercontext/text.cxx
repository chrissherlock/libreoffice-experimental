/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <config_vcl.h>
#include <config_fuzzers.h>

#include <rtl/ustrbuf.hxx>
#include <tools/debug.hxx>
#include <tools/helpers.hxx>
#include <tools/lineend.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/polygon/WaveLine.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/toolkit/controllayout.hxx>
#include <vcl/virdev.hxx>

#include <ImplMultiTextLineInfo.hxx>
#include <ImplOutDevData.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <text.hxx>
#include <textlayout.hxx>
#include <textlineinfo.hxx>

#define UNDERLINE_LAST LINESTYLE_BOLDWAVE
#define STRIKEOUT_LAST STRIKEOUT_X

#if !ENABLE_FUZZERS
const SalLayoutFlags eDefaultLayout = SalLayoutFlags::NONE;
#else
// ofz#23573 skip detecting bidi directions
const SalLayoutFlags eDefaultLayout = SalLayoutFlags::BiDiStrong;
#endif

void RenderContext2::DrawText(Point const& rStartPt, OUString const& rStr, sal_Int32 nIndex,
                              sal_Int32 nLen, std::vector<tools::Rectangle>* pVector,
                              OUString* pDisplayText, SalLayoutGlyphs const* pLayoutCache)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
        nLen = rStr.getLength() - nIndex;

    if (mpOutDevData->mpRecordLayout)
    {
        pVector = &mpOutDevData->mpRecordLayout->m_aUnicodeBoundRects;
        pDisplayText = &mpOutDevData->mpRecordLayout->m_aDisplayText;
    }

#if OSL_DEBUG_LEVEL > 2
    SAL_INFO("vcl.gdi", "RenderContext2::DrawText(\"" << rStr << "\")");
#endif

    if (pVector)
    {
        vcl::Region aClip(GetOutputBoundsClipRegion());

        if (mpOutDevData->mpRecordLayout)
        {
            mpOutDevData->mpRecordLayout->m_aLineIndices.push_back(
                mpOutDevData->mpRecordLayout->m_aDisplayText.getLength());
            aClip.Intersect(mpOutDevData->maRecordRect);
        }
        if (!aClip.IsNull())
        {
            std::vector<tools::Rectangle> aTmp;
            GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, aTmp);

            bool bInserted = false;
            for (std::vector<tools::Rectangle>::const_iterator it = aTmp.begin(); it != aTmp.end();
                 ++it, nIndex++)
            {
                bool bAppend = false;

                if (aClip.IsOver(*it))
                    bAppend = true;
                else if (rStr[nIndex] == ' ' && bInserted)
                {
                    std::vector<tools::Rectangle>::const_iterator next = it;
                    ++next;
                    if (next != aTmp.end() && aClip.IsOver(*next))
                        bAppend = true;
                }

                if (bAppend)
                {
                    pVector->push_back(*it);
                    if (pDisplayText)
                        *pDisplayText += OUStringChar(rStr[nIndex]);
                    bInserted = true;
                }
            }
        }
        else
        {
            GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, *pVector);
            if (pDisplayText)
                *pDisplayText += rStr.subView(nIndex, nLen);
        }
    }

    if (!IsDeviceOutputNecessary() || pVector)
        return;

    if (mpFontInstance)
    {
        // do not use cache with modified string
        if (mpFontInstance->CanRecodeString())
            pLayoutCache = nullptr;
    }

    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rStr, nIndex, nLen, rStartPt, 0, nullptr,
                                                       eDefaultLayout, nullptr, pLayoutCache);
    if (pSalLayout)
    {
        ImplDrawText(*pSalLayout);
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawText(rStartPt, rStr, nIndex, nLen, pVector, pDisplayText);
}

void RenderContext2::DrawText(tools::Rectangle const& rRect, OUString const& rOrigStr,
                              DrawTextFlags nStyle, std::vector<tools::Rectangle>* pVector,
                              OUString* pDisplayText, vcl::ITextLayout* _pTextLayout)
{
    assert(!is_double_buffered_window());

    if (mpOutDevData->mpRecordLayout)
    {
        pVector = &mpOutDevData->mpRecordLayout->m_aUnicodeBoundRects;
        pDisplayText = &mpOutDevData->mpRecordLayout->m_aDisplayText;
    }

    bool bDecomposeTextRectAction
        = (_pTextLayout != nullptr) && _pTextLayout->DecomposeTextRectAction();

    if ((!IsDeviceOutputNecessary() && !pVector && !bDecomposeTextRectAction) || rOrigStr.isEmpty()
        || rRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped && !bDecomposeTextRectAction)
        return;

    // #i47157# Factored out to ImplDrawText(), to be used also
    // from AddTextRectActions()
    vcl::DefaultTextLayout aDefaultLayout(*this);
    ImplDrawText(rRect, rOrigStr, nStyle, pVector, pDisplayText,
                 _pTextLayout ? *_pTextLayout : aDefaultLayout);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawText(rRect, rOrigStr, nStyle, pVector, pDisplayText);
}

void RenderContext2::DrawTextArray(Point const& rStartPt, OUString const& rStr,
                                   tools::Long const* pDXAry, sal_Int32 nIndex, sal_Int32 nLen,
                                   SalLayoutFlags flags, SalLayoutGlyphs const* pSalLayoutCache)
{
    assert(!is_double_buffered_window());

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    std::unique_ptr<SalLayout> pSalLayout
        = ImplLayout(rStr, nIndex, nLen, rStartPt, 0, pDXAry, flags, nullptr, pSalLayoutCache);

    if (pSalLayout)
        ImplDrawText(*pSalLayout);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawTextArray(rStartPt, rStr, pDXAry, nIndex, nLen, flags);
}

void RenderContext2::DrawStretchText(Point const& rStartPt, sal_uLong nWidth, OUString const& rStr,
                                     sal_Int32 nIndex, sal_Int32 nLen)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
        nLen = rStr.getLength() - nIndex;

    if (!IsDeviceOutputNecessary())
        return;

    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rStr, nIndex, nLen, rStartPt, nWidth);

    if (pSalLayout)
        ImplDrawText(*pSalLayout);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawStretchText(rStartPt, nWidth, rStr, nIndex, nLen);
}

void RenderContext2::DrawTextLine(Point const& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                                  FontLineStyle eUnderline, FontLineStyle eOverline,
                                  bool bUnderlineAbove)
{
    assert(!is_double_buffered_window());

    if (((eUnderline == LINESTYLE_NONE) || (eUnderline == LINESTYLE_DONTKNOW))
        && ((eOverline == LINESTYLE_NONE) || (eOverline == LINESTYLE_DONTKNOW))
        && ((eStrikeout == STRIKEOUT_NONE) || (eStrikeout == STRIKEOUT_DONTKNOW)))
    {
        return;
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    // initialize font if needed to get text offsets
    // TODO: only needed for mnTextOff!=(0,0)
    if (!InitFont())
        return;

    DeviceCoordinate fWidth = LogicWidthToDeviceCoordinate(nWidth);
    Point aPos = ImplLogicToDevicePixel(rPos);
    aPos += Point(mnTextOffX, mnTextOffY);

    ImplDrawTextLine(aPos.X(), aPos.X(), 0, fWidth, eStrikeout, eUnderline, eOverline,
                     bUnderlineAbove);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawTextLine(rPos, nWidth, eStrikeout, eUnderline, eOverline, bUnderlineAbove);
}

void RenderContext2::DrawWaveLine(Point const& rStartPos, Point const& rEndPos,
                                  tools::Long nLineWidth)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary() || ImplIsRecordLayout())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (!InitFont())
        return;

    Point aStartPt = ImplLogicToDevicePixel(rStartPos);
    Point aEndPt = ImplLogicToDevicePixel(rEndPos);

    tools::Long nStartX = aStartPt.X();
    tools::Long nStartY = aStartPt.Y();
    tools::Long nEndX = aEndPt.X();
    tools::Long nEndY = aEndPt.Y();
    double fOrientation = 0.0;

    // handle rotation
    if (nStartY != nEndY || nStartX > nEndX)
    {
        tools::Long nLengthX = nEndX - nStartX;
        fOrientation = std::atan2(nStartY - nEndY, (nLengthX == 0 ? 0.000000001 : nLengthX));
        fOrientation /= F_PI180;
        // un-rotate the end point
        aStartPt.RotateAround(nEndX, nEndY, Degree10(static_cast<sal_Int16>(-fOrientation * 10.0)));
    }

    tools::Long nWaveHeight = 3;

    // Handle HiDPI
    float fScaleFactor = GetDPIScaleFactor();
    if (fScaleFactor > 1.0f)
    {
        nWaveHeight *= fScaleFactor;

        nStartY
            += fScaleFactor - 1; // Shift down additional pixel(s) to create more visual separation.

        // odd heights look better than even
        if (nWaveHeight % 2 == 0)
            nWaveHeight--;
    }

    // #109280# make sure the waveline does not exceed the descent to avoid paint problems
    FontInstance* pFontInstance = mpFontInstance.get();
    if (nWaveHeight > pFontInstance->mxFontMetric->GetWavelineUnderlineSize())
    {
        nWaveHeight = pFontInstance->mxFontMetric->GetWavelineUnderlineSize();
        // tdf#124848 hairline
        nLineWidth = 0;
    }

    const basegfx::B2DRectangle aWaveLineRectangle(nStartX, nStartY, nEndX, nEndY + nWaveHeight);
    const basegfx::B2DPolygon aWaveLinePolygon = basegfx::createWaveLinePolygon(aWaveLineRectangle);
    const basegfx::B2DHomMatrix aRotationMatrix = basegfx::utils::createRotateAroundPoint(
        nStartX, nStartY, basegfx::deg2rad(-fOrientation));
    const bool bPixelSnapHairline(mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    mpGraphics->SetLineColor(GetLineColor());
    mpGraphics->DrawPolyLine(aRotationMatrix, aWaveLinePolygon, 0.0, nLineWidth,
                             nullptr, // MM01
                             basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                             basegfx::deg2rad(15.0), bPixelSnapHairline, *this);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawWaveLine(rStartPos, rEndPos, nLineWidth);
}

void RenderContext2::ImplInitTextColor()
{
    DBG_TESTSOLARMUTEX();

    if (mbInitTextColor)
    {
        mpGraphics->SetTextColor(GetTextColor());
        mbInitTextColor = false;
    }
}

tools::Long RenderContext2::GetTextHeight() const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return 0;

    tools::Long nHeight = mpFontInstance->mnLineHeight + mpFontInstance->GetEmphasisAscent() + mpFontInstance->GetEmphasisDescent();

    if (maGeometry.IsMapModeEnabled())
        nHeight = ImplDevicePixelToLogicHeight(nHeight);

    return nHeight;
}

float RenderContext2::approximate_char_width() const
{
    //note pango uses "The quick brown fox jumps over the lazy dog." for english
    //and has a bunch of per-language strings which corresponds somewhat with
    //makeRepresentativeText in include/svtools/sampletext.hxx
    return GetTextWidth("aemnnxEM") / 8.0;
}

float RenderContext2::approximate_digit_width() const { return GetTextWidth("0123456789") / 10.0; }

tools::Long RenderContext2::GetTextWidth(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                         vcl::TextLayoutCache const* const pLayoutCache,
                                         SalLayoutGlyphs const* const pSalLayoutCache) const
{
    tools::Long nWidth = GetTextArray(rStr, nullptr, nIndex, nLen, pLayoutCache, pSalLayoutCache);

    return nWidth;
}

bool RenderContext2::GetTextBoundRect(tools::Rectangle& rRect, OUString const& rStr,
                                      sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                      sal_uLong nLayoutWidth, tools::Long const* pDXAry,
                                      SalLayoutGlyphs const* pGlyphs) const
{
    bool bRet = false;
    rRect.SetEmpty();

    std::unique_ptr<SalLayout> pSalLayout;
    const Point aPoint;
    // calculate offset when nBase!=nIndex
    tools::Long nXOffset = 0;

    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;
        pSalLayout = ImplLayout(rStr, nStart, nOfsLen, aPoint, nLayoutWidth, pDXAry);

        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            nXOffset /= pSalLayout->GetUnitsPerPixel();

            // TODO: fix offset calculation for Bidi case
            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = ImplLayout(rStr, nIndex, nLen, aPoint, nLayoutWidth, pDXAry, eDefaultLayout,
                            nullptr, pGlyphs);

    if (pSalLayout)
    {
        tools::Rectangle aPixelRect;
        bRet = pSalLayout->GetBoundRect(aPixelRect);

        if (bRet)
        {
            int nWidthFactor = pSalLayout->GetUnitsPerPixel();

            if (nWidthFactor > 1)
            {
                double fFactor = 1.0 / nWidthFactor;
                aPixelRect.SetLeft(static_cast<tools::Long>(aPixelRect.Left() * fFactor));
                aPixelRect.SetRight(static_cast<tools::Long>(aPixelRect.Right() * fFactor));
                aPixelRect.SetTop(static_cast<tools::Long>(aPixelRect.Top() * fFactor));
                aPixelRect.SetBottom(static_cast<tools::Long>(aPixelRect.Bottom() * fFactor));
            }

            Point aRotatedOfs(mnTextOffX, mnTextOffY);
            aRotatedOfs -= pSalLayout->GetDrawPosition(Point(nXOffset, 0));
            aPixelRect += aRotatedOfs;
            rRect = PixelToLogic(aPixelRect);

            if (maGeometry.IsMapModeEnabled())
                rRect += Point(maMapRes.mnMapOfsX, maMapRes.mnMapOfsY);
        }
    }

    return bRet;
}

tools::Long RenderContext2::GetTextArray(const OUString& rStr, tools::Long* pDXAry,
                                         sal_Int32 nIndex, sal_Int32 nLen,
                                         vcl::TextLayoutCache const* const pLayoutCache,
                                         SalLayoutGlyphs const* const pSalLayoutCache) const
{
    if (nIndex >= rStr.getLength())
        return 0; // TODO: this looks like a buggy caller?

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    // do layout
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(
        rStr, nIndex, nLen, Point(0, 0), 0, nullptr, eDefaultLayout, pLayoutCache, pSalLayoutCache);

    if (!pSalLayout)
    {
        // The caller expects this to init the elements of pDXAry.
        // Adapting all the callers to check that GetTextArray succeeded seems
        // too much work.
        // Init here to 0 only in the (rare) error case, so that any missing
        // element init in the happy case will still be found by tools,
        // and hope that is sufficient.
        if (pDXAry)
            memset(pDXAry, 0, nLen * sizeof(*pDXAry));

        return 0;
    }

#if VCL_FLOAT_DEVICE_PIXEL
    std::unique_ptr<DeviceCoordinate[]> pDXPixelArray;
    if (pDXAry)
        pDXPixelArray.reset(new DeviceCoordinate[nLen]);

    DeviceCoordinate nWidth = pSalLayout->FillDXArray(pDXPixelArray.get());
    int nWidthFactor = pSalLayout->GetUnitsPerPixel();

    // convert virtual char widths to virtual absolute positions
    if (pDXPixelArray)
    {
        for (int i = 1; i < nLen; ++i)
        {
            pDXPixelArray[i] += pDXPixelArray[i - 1];
        }
    }
    if (maGeometry.IsMapModeEnabled())
    {
        if (pDXPixelArray)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] = ImplDevicePixelToLogicWidth(pDXPixelArray[i]);
            }
        }
        nWidth = ImplDevicePixelToLogicWidth(nWidth);
    }
    if (nWidthFactor > 1)
    {
        if (pDXPixelArray)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] /= nWidthFactor;
            }
        }
        nWidth /= nWidthFactor;
    }
    if (pDXAry)
    {
        for (int i = 0; i < nLen; ++i)
        {
            pDXAry[i] = basegfx::fround(pDXPixelArray[i]);
        }
    }
    return basegfx::fround(nWidth);

#else /* ! VCL_FLOAT_DEVICE_PIXEL */

    tools::Long nWidth = pSalLayout->FillDXArray(pDXAry);
    int nWidthFactor = pSalLayout->GetUnitsPerPixel();

    // convert virtual char widths to virtual absolute positions
    if (pDXAry)
    {
        for (int i = 1; i < nLen; ++i)
        {
            pDXAry[i] += pDXAry[i - 1];
        }
    }

    // convert from font units to logical units
    if (maGeometry.IsMapModeEnabled())
    {
        if (pDXAry)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXAry[i] = ImplDevicePixelToLogicWidth(pDXAry[i]);
            }
        }

        nWidth = ImplDevicePixelToLogicWidth(nWidth);
    }

    if (nWidthFactor > 1)
    {
        if (pDXAry)
        {
            for (int i = 0; i < nLen; ++i)
            {
                pDXAry[i] /= nWidthFactor;
            }
        }

        nWidth /= nWidthFactor;
    }

    return nWidth;
#endif /* VCL_FLOAT_DEVICE_PIXEL */
}
ComplexTextLayoutFlags RenderContext2::GetLayoutMode() const { return mnTextLayoutMode; }

void RenderContext2::SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode)
{
    mnTextLayoutMode = nTextLayoutMode;

    if (mpAlphaVDev)
        mpAlphaVDev->SetLayoutMode(nTextLayoutMode);
}

LanguageType RenderContext2::GetDigitLanguage() const { return meTextLanguage; }

void RenderContext2::SetDigitLanguage(LanguageType eTextLanguage)
{
    meTextLanguage = eTextLanguage;

    if (mpAlphaVDev)
        mpAlphaVDev->SetDigitLanguage(eTextLanguage);
}

Color const& RenderContext2::GetTextColor() const { return maTextColor; }

void RenderContext2::SetTextColor(Color const& rColor)
{
    Color aColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (maTextColor != aColor)
    {
        maTextColor = aColor;
        mbInitTextColor = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextColor(COL_BLACK);
}

void RenderContext2::SetSystemTextColor(DrawFlags nFlags, bool bEnabled)
{
    if (nFlags & DrawFlags::Mono)
    {
        SetTextColor(COL_BLACK);
    }
    else
    {
        if (!bEnabled)
        {
            const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
            SetTextColor(rStyleSettings.GetDisableColor());
        }
        else
        {
            SetTextColor(GetTextColor());
        }
    }
}

bool RenderContext2::IsTextFillColor() const { return !maFont.IsTransparent(); }

Color RenderContext2::GetTextFillColor() const
{
    if (maFont.IsTransparent())
        return COL_TRANSPARENT;
    else
        return maFont.GetFillColor();
}

void RenderContext2::SetTextFillColor()
{
    if (maFont.GetColor() != COL_TRANSPARENT)
        maFont.SetFillColor(COL_TRANSPARENT);

    if (!maFont.IsTransparent())
        maFont.SetTransparent(true);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor();
}

void RenderContext2::SetTextFillColor(Color const& rColor)
{
    Color aColor(rColor);
    bool bTransFill = aColor.IsTransparent();

    if (!bTransFill)
    {
        if (mnDrawMode
            & (DrawModeFlags::BlackFill | DrawModeFlags::WhiteFill | DrawModeFlags::GrayFill
               | DrawModeFlags::NoFill | DrawModeFlags::SettingsFill))
        {
            if (mnDrawMode & DrawModeFlags::BlackFill)
            {
                aColor = COL_BLACK;
            }
            else if (mnDrawMode & DrawModeFlags::WhiteFill)
            {
                aColor = COL_WHITE;
            }
            else if (mnDrawMode & DrawModeFlags::GrayFill)
            {
                const sal_uInt8 cLum = aColor.GetLuminance();
                aColor = Color(cLum, cLum, cLum);
            }
            else if (mnDrawMode & DrawModeFlags::SettingsFill)
            {
                aColor = GetSettings().GetStyleSettings().GetWindowColor();
            }
            else if (mnDrawMode & DrawModeFlags::NoFill)
            {
                aColor = COL_TRANSPARENT;
                bTransFill = true;
            }
        }
    }

    if (maFont.GetFillColor() != aColor)
        maFont.SetFillColor(aColor);

    if (maFont.IsTransparent() != bTransFill)
        maFont.SetTransparent(bTransFill);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor(COL_BLACK);
}

bool RenderContext2::IsTextLineColor() const { return !maTextLineColor.IsTransparent(); }

Color const& RenderContext2::GetTextLineColor() const { return maTextLineColor; }

void RenderContext2::SetTextLineColor()
{
    maTextLineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor();
}

void RenderContext2::SetTextLineColor(Color const& rColor)
{
    maTextLineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor(COL_BLACK);
}

bool RenderContext2::IsOverlineColor() const { return !maOverlineColor.IsTransparent(); }

Color const& RenderContext2::GetOverlineColor() const { return maOverlineColor; }

void RenderContext2::SetOverlineColor()
{
    maOverlineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor();
}

void RenderContext2::SetOverlineColor(Color const& rColor)
{
    maOverlineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor(COL_BLACK);
}

TextAlign RenderContext2::GetTextAlign() const { return maFont.GetAlignment(); }

void RenderContext2::SetTextAlign(TextAlign eAlign)
{
    if (maFont.GetAlignment() != eAlign)
    {
        maFont.SetAlignment(eAlign);
        mbNewFont = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextAlign(eAlign);
}

SalLayoutGlyphs* RenderContext2::GetLayoutGlyphs(OUString const& rText,
                                                 std::unique_ptr<SalLayout>& rLayout)
{
    SalLayout* pLayoutCache = rLayout.get();

    if (!pLayoutCache)
    {
        // update cache
        rLayout = ImplLayout(rText, 0, -1);
        pLayoutCache = rLayout.get();
    }

    SalLayoutGlyphs glyphs = pLayoutCache ? pLayoutCache->GetGlyphs() : SalLayoutGlyphs();
    SalLayoutGlyphs* pGlyphs = pLayoutCache ? &glyphs : nullptr;

    return pGlyphs;
}

SalLayoutGlyphs* RenderContext2::GetLayoutGlyphs(OUString const& rText, sal_Int32 nIndex,
                                                 sal_Int32 nLength)
{
    if (nIndex >= rText.getLength())
    {
        // Same as in OutputDevice::GetTextArray().
        return nullptr;
    }

    std::unique_ptr<SalLayout> pLayout = ImplLayout(rText, nIndex, nLength, Point(0, 0), 0, nullptr,
                                                    SalLayoutFlags::GlyphItemsOnly);

    if (!pLayout)
        return nullptr;

    return new SalLayoutGlyphs(pLayout->GetGlyphs());
}

SalLayoutGlyphs const*
RenderContext2::GetLayoutGlyphs(OUString const& rString,
                                o3tl::lru_map<OUString, SalLayoutGlyphs>& rCachedGlyphs)
{
    auto it = rCachedGlyphs.find(rString);
    if (it != rCachedGlyphs.end() && it->second.IsValid())
        return &it->second;

    std::unique_ptr<SalLayout> layout = ImplLayout(rString, 0, rString.getLength(), Point(0, 0), 0,
                                                   nullptr, SalLayoutFlags::GlyphItemsOnly);

    if (layout)
    {
        rCachedGlyphs.insert(std::make_pair(rString, layout->GetGlyphs()));
        assert(rCachedGlyphs.find(rString)
               == rCachedGlyphs.begin()); // newly inserted item is first
        return &rCachedGlyphs.begin()->second;
    }

    return nullptr;
}

SalLayoutGlyphs* RenderContext2::GetLayoutGlyphs(OUString const& rText,
                                                 SalLayoutGlyphs& rTextGlyphs)
{
    if (rTextGlyphs.IsValid())
        // Use pre-calculated result.
        return &rTextGlyphs;

    // Calculate glyph items.

    std::unique_ptr<SalLayout> pLayout = ImplLayout(rText, 0, rText.getLength(), Point(0, 0), 0,
                                                    nullptr, SalLayoutFlags::GlyphItemsOnly);

    if (!pLayout)
        return nullptr;

    // Remember the calculation result.
    rTextGlyphs = pLayout->GetGlyphs();

    return &rTextGlyphs;
}

std::unique_ptr<SalLayout> RenderContext2::ImplLayout(
    OUString const& rOrigStr, sal_Int32 nMinIndex, sal_Int32 nLen, Point const& rLogicalPos,
    tools::Long nLogicalWidth, tools::Long const* pDXArray, SalLayoutFlags flags,
    vcl::TextLayoutCache const* pLayoutCache, SalLayoutGlyphs const* pGlyphs) const
{
    if (pGlyphs && !pGlyphs->IsValid())
    {
        SAL_WARN("vcl", "Trying to setup invalid cached glyphs - falling back to relayout!");
        pGlyphs = nullptr;
    }

    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return nullptr;

    // check string index and length
    if (nLen == -1 || nMinIndex + nLen > rOrigStr.getLength())
    {
        const sal_Int32 nNewLen = rOrigStr.getLength() - nMinIndex;
        if (nNewLen <= 0)
            return nullptr;
        nLen = nNewLen;
    }

    OUString aStr = rOrigStr;

    // convert from logical units to physical units
    // recode string if needed
    if (mpFontInstance->CanRecodeString())
    {
        mpFontInstance->RecodeString(aStr);
        pLayoutCache = nullptr; // don't use cache with modified string!
        pGlyphs = nullptr;
    }

    DeviceCoordinate nPixelWidth = static_cast<DeviceCoordinate>(nLogicalWidth);
    if (nLogicalWidth && maGeometry.IsMapModeEnabled())
        nPixelWidth = LogicWidthToDeviceCoordinate(nLogicalWidth);

    std::unique_ptr<DeviceCoordinate[]> xDXPixelArray;
    DeviceCoordinate* pDXPixelArray(nullptr);
    if (pDXArray)
    {
        if (maGeometry.IsMapModeEnabled())
        {
            // convert from logical units to font units using a temporary array
            xDXPixelArray.reset(new DeviceCoordinate[nLen]);
            pDXPixelArray = xDXPixelArray.get();
            // using base position for better rounding a.k.a. "dancing characters"
            DeviceCoordinate nPixelXOfs = LogicWidthToDeviceCoordinate(rLogicalPos.X());
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i]
                    = LogicWidthToDeviceCoordinate(rLogicalPos.X() + pDXArray[i]) - nPixelXOfs;
            }
        }
        else
        {
#if VCL_FLOAT_DEVICE_PIXEL
            xDXPixelArray.reset(new DeviceCoordinate[nLen]);
            pDXPixelArray = xDXPixelArray.get();
            for (int i = 0; i < nLen; ++i)
            {
                pDXPixelArray[i] = pDXArray[i];
            }
#else /* !VCL_FLOAT_DEVICE_PIXEL */
            pDXPixelArray = const_cast<DeviceCoordinate*>(pDXArray);
#endif /* !VCL_FLOAT_DEVICE_PIXEL */
        }
    }

    ImplLayoutArgs aLayoutArgs = ImplPrepareLayoutArgs(aStr, nMinIndex, nLen, nPixelWidth,
                                                       pDXPixelArray, flags, pLayoutCache);

    // get matching layout object for base font
    std::unique_ptr<SalLayout> pSalLayout = mpGraphics->GetTextLayout(0);

    // layout text
    if (pSalLayout && !pSalLayout->LayoutText(aLayoutArgs, pGlyphs ? pGlyphs->Impl(0) : nullptr))
        pSalLayout.reset();

    if (!pSalLayout)
        return nullptr;

    // do glyph fallback if needed
    // #105768# avoid fallback for very small font sizes
    if (aLayoutArgs.NeedFallback() && mpFontInstance->GetFontSelectPattern().mnHeight >= 3)
        pSalLayout = ImplGlyphFallbackLayout(std::move(pSalLayout), aLayoutArgs, pGlyphs);

    // Return glyph items only after fallback handling. Otherwise they may contain invalid
    // glyph IDs.
    if (flags & SalLayoutFlags::GlyphItemsOnly)
        return pSalLayout;

    // position, justify, etc. the layout
    pSalLayout->AdjustLayout(aLayoutArgs);
    pSalLayout->DrawBase() = ImplLogicToDevicePixel(rLogicalPos);
    // adjust to right alignment if necessary
    if (aLayoutArgs.mnFlags & SalLayoutFlags::RightAlign)
    {
        DeviceCoordinate nRTLOffset;

        if (pDXPixelArray)
            nRTLOffset = pDXPixelArray[nLen - 1];
        else if (nPixelWidth)
            nRTLOffset = nPixelWidth;
        else
            nRTLOffset = pSalLayout->GetTextWidth() / pSalLayout->GetUnitsPerPixel();

        pSalLayout->DrawOffset().setX(1 - nRTLOffset);
    }

    return pSalLayout;
}

ImplLayoutArgs
RenderContext2::ImplPrepareLayoutArgs(OUString& rStr, const sal_Int32 nMinIndex,
                                      const sal_Int32 nLen, DeviceCoordinate nPixelWidth,
                                      DeviceCoordinate const* pDXArray, SalLayoutFlags nLayoutFlags,
                                      vcl::TextLayoutCache const* const pLayoutCache) const
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

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl)
        nLayoutFlags |= SalLayoutFlags::BiDiRtl;

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiStrong)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }
    else if (!(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl))
    {
        // Disable Bidi if no RTL hint and only known LTR codes used.
        bool bAllLtr = true;
        for (sal_Int32 i = nMinIndex; i < nEndIndex; i++)
        {
            // [0x0000, 0x052F] are Latin, Greek and Cyrillic.
            // [0x0370, 0x03FF] has a few holes as if Unicode 10.0.0, but
            //                  hopefully no RTL character will be encoded there.
            if (rStr[i] > 0x052F)
            {
                bAllLtr = false;
                break;
            }
        }

        if (bAllLtr)
            nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }

    if (!maFont.IsKerning())
        nLayoutFlags |= SalLayoutFlags::DisableKerning;

    if (maFont.GetKerning() & FontKerning::Asian)
        nLayoutFlags |= SalLayoutFlags::KerningAsian;

    if (maFont.IsVertical())
        nLayoutFlags |= SalLayoutFlags::Vertical;

    if (meTextLanguage) //TODO: (mnTextLayoutMode & ComplexTextLayoutFlags::SubstituteDigits)
    {
        // disable character localization when no digits used
        const sal_Unicode* pBase = rStr.getStr();
        const sal_Unicode* pStr = pBase + nMinIndex;
        const sal_Unicode* pEnd = pBase + nEndIndex;
        std::optional<OUStringBuffer> xTmpStr;
        for (; pStr < pEnd; ++pStr)
        {
            // TODO: are there non-digit localizations?
            if ((*pStr >= '0') && (*pStr <= '9'))
            {
                // translate characters to local preference
                sal_UCS4 cChar = GetLocalizedChar(*pStr, meTextLanguage);
                if (cChar != *pStr)
                {
                    if (!xTmpStr)
                        xTmpStr = OUStringBuffer(rStr);

                    // TODO: are the localized digit surrogates?
                    (*xTmpStr)[pStr - pBase] = cChar;
                }
            }
        }
        if (xTmpStr)
            rStr = (*xTmpStr).makeStringAndClear();
    }

    // right align for RTL text, DRAWPOS_REVERSED, RTL window style
    bool bRightAlign = bool(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl);

    if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginLeft)
        bRightAlign = false;
    else if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginRight)
        bRightAlign = true;

    // SSA: hack for western office, ie text get right aligned
    //      for debugging purposes of mirrored UI
    bool bRTLWindow = IsRTLEnabled();
    bRightAlign ^= bRTLWindow;

    if (bRightAlign)
        nLayoutFlags |= SalLayoutFlags::RightAlign;

    // set layout options
    ImplLayoutArgs aLayoutArgs(rStr, nMinIndex, nEndIndex, nLayoutFlags, maFont.GetLanguageTag(),
                               pLayoutCache);

    Degree10 nOrientation = mpFontInstance ? mpFontInstance->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);
    aLayoutArgs.SetDXArray(pDXArray);

    return aLayoutArgs;
}

void RenderContext2::ImplDrawTextRect(tools::Long nBaseX, tools::Long nBaseY, tools::Long nDistX,
                                      tools::Long nDistY, tools::Long nWidth, tools::Long nHeight)
{
    tools::Long nX = nDistX;
    tools::Long nY = nDistY;

    Degree10 nOrientation = mpFontInstance->mnOrientation;
    if (nOrientation)
    {
        // Rotate rect without rounding problems for 90 degree rotations
        if (!(nOrientation % 900_deg10))
        {
            if (nOrientation == 900_deg10)
            {
                tools::Long nTemp = nX;
                nX = nY;
                nY = -nTemp;
                nTemp = nWidth;
                nWidth = nHeight;
                nHeight = nTemp;
                nY -= nHeight;
            }
            else if (nOrientation == 1800_deg10)
            {
                nX = -nX;
                nY = -nY;
                nX -= nWidth;
                nY -= nHeight;
            }
            else /* ( nOrientation == 2700 ) */
            {
                tools::Long nTemp = nX;
                nX = -nY;
                nY = nTemp;
                nTemp = nWidth;
                nWidth = nHeight;
                nHeight = nTemp;
                nX -= nWidth;
            }
        }
        else
        {
            nX += nBaseX;
            nY += nBaseY;
            // inflate because polygons are drawn smaller
            tools::Rectangle aRect(Point(nX, nY), Size(nWidth + 1, nHeight + 1));
            tools::Polygon aPoly(aRect);
            aPoly.Rotate(Point(nBaseX, nBaseY), mpFontInstance->mnOrientation);
            ImplDrawPolygon(aPoly);
            return;
        }
    }

    nX += nBaseX;
    nY += nBaseY;
    mpGraphics->DrawRect(nX, nY, nWidth, nHeight, *this); // original code
}

void RenderContext2::ImplDrawTextBackground(const SalLayout& rSalLayout)
{
    const tools::Long nWidth = rSalLayout.GetTextWidth() / rSalLayout.GetUnitsPerPixel();
    const Point aBase = rSalLayout.DrawBase();
    const tools::Long nX = aBase.X();
    const tools::Long nY = aBase.Y();

    if (mbLineColor || mbInitLineColor)
    {
        mpGraphics->SetLineColor();
        mbInitLineColor = true;
    }
    mpGraphics->SetFillColor(GetTextFillColor());
    mbInitFillColor = true;

    ImplDrawTextRect(nX, nY, 0, -(mpFontInstance->mxFontMetric->GetAscent() + mpFontInstance->GetEmphasisAscent()),
                     nWidth, mpFontInstance->mnLineHeight + mpFontInstance->GetEmphasisAscent() + mpFontInstance->GetEmphasisDescent());
}

bool RenderContext2::ImplDrawRotateText(SalLayout& rSalLayout)
{
    tools::Long nX = rSalLayout.DrawBase().X();
    tools::Long nY = rSalLayout.DrawBase().Y();

    tools::Rectangle aBoundRect;
    rSalLayout.DrawBase() = Point(0, 0);
    rSalLayout.DrawOffset() = Point(0, 0);
    if (!rSalLayout.GetBoundRect(aBoundRect))
    {
        // guess vertical text extents if GetBoundRect failed
        tools::Long nRight = rSalLayout.GetTextWidth();
        tools::Long nTop = mpFontInstance->mxFontMetric->GetAscent() + mpFontInstance->GetEmphasisAscent();
        tools::Long nHeight = mpFontInstance->mnLineHeight + mpFontInstance->GetEmphasisAscent() + mpFontInstance->GetEmphasisDescent();
        aBoundRect = tools::Rectangle(0, -nTop, nRight, nHeight - nTop);
    }

    // cache virtual device for rotation
    if (!mpOutDevData->mpRotateDev)
        mpOutDevData->mpRotateDev = VclPtr<VirtualDevice>::Create(*this, DeviceFormat::BITMASK);
    VirtualDevice* pVDev = mpOutDevData->mpRotateDev;

    // size it accordingly
    if (!pVDev->SetOutputSizePixel(aBoundRect.GetSize()))
        return false;

    const FontSelectPattern& rPattern = mpFontInstance->GetFontSelectPattern();
    vcl::Font aFont(GetFont());
    aFont.SetOrientation(0_deg10);
    aFont.SetFontSize(Size(rPattern.mnWidth, rPattern.mnHeight));
    pVDev->SetFont(aFont);
    pVDev->SetTextColor(COL_BLACK);
    pVDev->SetTextFillColor();
    if (!pVDev->InitFont())
        return false;
    pVDev->ImplInitTextColor();

    // draw text into upper left corner
    rSalLayout.DrawBase() -= aBoundRect.TopLeft();
    rSalLayout.DrawText(*pVDev->mpGraphics);

    Bitmap aBmp = pVDev->GetBitmap(Point(), aBoundRect.GetSize());
    if (aBmp.IsEmpty() || !aBmp.Rotate(mpFontInstance->mnOwnOrientation, COL_WHITE))
        return false;

    // calculate rotation offset
    tools::Polygon aPoly(aBoundRect);
    aPoly.Rotate(Point(), mpFontInstance->mnOwnOrientation);
    Point aPoint = aPoly.GetBoundRect().TopLeft();
    aPoint += Point(nX, nY);

    // mask output with text colored bitmap
    tools::Long nOldOffX = maGeometry.GetXFrameOffset();
    tools::Long nOldOffY = maGeometry.GetYFrameOffset();
    bool bOldMap = maGeometry.IsMapModeEnabled();

    maGeometry.SetXFrameOffset(0);
    maGeometry.SetYFrameOffset(0);
    EnableMapMode(false);

    DrawMask(aPoint, aBmp, GetTextColor());

    EnableMapMode(bOldMap);
    maGeometry.SetXFrameOffset(nOldOffX);
    maGeometry.SetYFrameOffset(nOldOffY);

    return true;
}

void RenderContext2::ImplDrawTextDirect(SalLayout& rSalLayout)
{
    bool bTextLines = HasTextLines();

    if (mpFontInstance->mnOwnOrientation)
        if (ImplDrawRotateText(rSalLayout))
            return;

    tools::Long nOldX = rSalLayout.DrawBase().X();
    if (HasMirroredGraphics())
    {
        tools::Long w
            = IsVirtual() ? maGeometry.GetWidthInPixels() : mpGraphics->GetGraphicsWidth();
        tools::Long x = rSalLayout.DrawBase().X();
        rSalLayout.DrawBase().setX(w - 1 - x);
        if (!IsRTLEnabled())
        {
            RenderContext2* pOutDevRef = this;
            // mirror this window back
            tools::Long devX
                = w - pOutDevRef->maGeometry.GetWidthInPixels()
                  - pOutDevRef->GetFrameOffset().X(); // re-mirrored maGeometry.GetFrameOffset().X()
            rSalLayout.DrawBase().setX(devX
                                       + (pOutDevRef->maGeometry.GetWidthInPixels() - 1
                                          - (rSalLayout.DrawBase().X() - devX)));
        }
    }
    else if (IsRTLEnabled())
    {
        RenderContext2* pOutDevRef = this;

        // mirror this window back
        tools::Long devX
            = pOutDevRef->GetFrameOffset().X(); // re-mirrored maGeometry.GetFrameOffset().X()
        rSalLayout.DrawBase().setX(pOutDevRef->GetWidth() - 1 - (rSalLayout.DrawBase().X() - devX)
                                   + devX);
    }

    rSalLayout.DrawText(*mpGraphics);
    rSalLayout.DrawBase().setX(nOldX);

    if (bTextLines)
        ImplDrawTextLines(rSalLayout, maFont.GetStrikeout(), maFont.GetUnderline(),
                          maFont.GetOverline(), maFont.IsWordLineMode(), maFont.IsUnderlineAbove());

    // emphasis marks
    if (maFont.GetEmphasisMark() & FontEmphasisMark::Style)
        ImplDrawEmphasisMarks(rSalLayout);
}

void RenderContext2::ImplDrawSpecialText(SalLayout& rSalLayout)
{
    Color aOldColor = GetTextColor();
    Color aOldTextLineColor = GetTextLineColor();
    Color aOldOverlineColor = GetOverlineColor();
    FontRelief eRelief = maFont.GetRelief();

    Point aOrigPos = rSalLayout.DrawBase();
    if (eRelief != FontRelief::NONE)
    {
        Color aReliefColor(COL_LIGHTGRAY);
        Color aTextColor(aOldColor);

        Color aTextLineColor(aOldTextLineColor);
        Color aOverlineColor(aOldOverlineColor);

        // we don't have an automatic color, so black is always drawn on white
        if (aTextColor == COL_BLACK)
            aTextColor = COL_WHITE;
        if (aTextLineColor == COL_BLACK)
            aTextLineColor = COL_WHITE;
        if (aOverlineColor == COL_BLACK)
            aOverlineColor = COL_WHITE;

        // relief-color is black for white text, in all other cases
        // we set this to LightGray
        // coverity[copy_paste_error: FALSE] - this is intentional
        if (aTextColor == COL_WHITE)
            aReliefColor = COL_BLACK;
        SetTextLineColor(aReliefColor);
        SetOverlineColor(aReliefColor);
        SetTextColor(aReliefColor);
        ImplInitTextColor();

        // calculate offset - for high resolution printers the offset
        // should be greater so that the effect is visible
        tools::Long nOff = 1;
        nOff += GetDPIX() / 300;

        if (eRelief == FontRelief::Engraved)
            nOff = -nOff;
        rSalLayout.DrawOffset() += Point(nOff, nOff);
        ImplDrawTextDirect(rSalLayout);
        rSalLayout.DrawOffset() -= Point(nOff, nOff);

        SetTextLineColor(aTextLineColor);
        SetOverlineColor(aOverlineColor);
        SetTextColor(aTextColor);
        ImplInitTextColor();
        ImplDrawTextDirect(rSalLayout);

        SetTextLineColor(aOldTextLineColor);
        SetOverlineColor(aOldOverlineColor);

        if (aTextColor != aOldColor)
        {
            SetTextColor(aOldColor);
            ImplInitTextColor();
        }
    }
    else
    {
        if (maFont.IsShadow())
        {
            tools::Long nOff = 1 + ((mpFontInstance->mnLineHeight - 24) / 24);
            if (maFont.IsOutline())
                nOff++;
            SetTextLineColor();
            SetOverlineColor();
            if ((GetTextColor() == COL_BLACK) || (GetTextColor().GetLuminance() < 8))
                SetTextColor(COL_LIGHTGRAY);
            else
                SetTextColor(COL_BLACK);
            ImplInitTextColor();
            rSalLayout.DrawBase() += Point(nOff, nOff);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() -= Point(nOff, nOff);
            SetTextColor(aOldColor);
            SetTextLineColor(aOldTextLineColor);
            SetOverlineColor(aOldOverlineColor);
            ImplInitTextColor();

            if (!maFont.IsOutline())
                ImplDrawTextDirect(rSalLayout);
        }

        if (maFont.IsOutline())
        {
            rSalLayout.DrawBase() = aOrigPos + Point(-1, -1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(+1, +1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(-1, +0);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(-1, +1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(+0, +1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(+0, -1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(+1, -1);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos + Point(+1, +0);
            ImplDrawTextDirect(rSalLayout);
            rSalLayout.DrawBase() = aOrigPos;

            SetTextColor(COL_WHITE);
            SetTextLineColor(COL_WHITE);
            SetOverlineColor(COL_WHITE);
            ImplInitTextColor();
            ImplDrawTextDirect(rSalLayout);
            SetTextColor(aOldColor);
            SetTextLineColor(aOldTextLineColor);
            SetOverlineColor(aOldOverlineColor);
            ImplInitTextColor();
        }
    }
}

void RenderContext2::ImplDrawText(SalLayout& rSalLayout)
{
    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (mbInitTextColor)
        ImplInitTextColor();

    rSalLayout.DrawBase() += Point(mnTextOffX, mnTextOffY);

    if (IsTextFillColor())
        ImplDrawTextBackground(rSalLayout);

    if (IsTextSpecial())
        ImplDrawSpecialText(rSalLayout);
    else
        ImplDrawTextDirect(rSalLayout);
}

void RenderContext2::ImplDrawTextLines(SalLayout& rSalLayout, FontStrikeout eStrikeout,
                                       FontLineStyle eUnderline, FontLineStyle eOverline,
                                       bool bWordLine, bool bUnderlineAbove)
{
    if (bWordLine)
    {
        // draw everything relative to the layout base point
        const Point aStartPt = rSalLayout.DrawBase();

        // calculate distance of each word from the base point
        Point aPos;
        DeviceCoordinate nDist = 0;
        DeviceCoordinate nWidth = 0;
        const GlyphItem* pGlyph;
        int nStart = 0;
        while (rSalLayout.GetNextGlyph(&pGlyph, aPos, nStart))
        {
            // calculate the boundaries of each word
            if (!pGlyph->IsSpacing())
            {
                if (!nWidth)
                {
                    // get the distance to the base point (as projected to baseline)
                    nDist = aPos.X() - aStartPt.X();
                    if (mpFontInstance->mnOrientation)
                    {
                        const tools::Long nDY = aPos.Y() - aStartPt.Y();
                        const double fRad = mpFontInstance->mnOrientation.get() * F_PI1800;
                        nDist = FRound(nDist * cos(fRad) - nDY * sin(fRad));
                    }
                }

                // update the length of the textline
                nWidth += pGlyph->m_nNewWidth;
            }
            else if (nWidth > 0)
            {
                // draw the textline for each word
                ImplDrawTextLine(aStartPt.X(), aStartPt.Y(), nDist, nWidth, eStrikeout, eUnderline,
                                 eOverline, bUnderlineAbove);
                nWidth = 0;
            }
        }

        // draw textline for the last word
        if (nWidth > 0)
        {
            ImplDrawTextLine(aStartPt.X(), aStartPt.Y(), nDist, nWidth, eStrikeout, eUnderline,
                             eOverline, bUnderlineAbove);
        }
    }
    else
    {
        Point aStartPt = rSalLayout.GetDrawPosition();
        ImplDrawTextLine(aStartPt.X(), aStartPt.Y(), 0,
                         rSalLayout.GetTextWidth() / rSalLayout.GetUnitsPerPixel(), eStrikeout,
                         eUnderline, eOverline, bUnderlineAbove);
    }
}

void RenderContext2::ImplDrawTextLine(tools::Long nX, tools::Long nY, tools::Long nDistX,
                                      DeviceCoordinate nWidth, FontStrikeout eStrikeout,
                                      FontLineStyle eUnderline, FontLineStyle eOverline,
                                      bool bUnderlineAbove)
{
    if (!nWidth)
        return;

    Color aStrikeoutColor = GetTextColor();
    Color aUnderlineColor = GetTextLineColor();
    Color aOverlineColor = GetOverlineColor();
    bool bStrikeoutDone = false;
    bool bUnderlineDone = false;
    bool bOverlineDone = false;

    if (IsRTLEnabled())
    {
        tools::Long nXAdd = nWidth - nDistX;
        if (mpFontInstance->mnOrientation)
            nXAdd = FRound(nXAdd * cos(mpFontInstance->mnOrientation.get() * F_PI1800));

        nX += nXAdd - 1;
    }

    if (!IsTextLineColor())
        aUnderlineColor = GetTextColor();

    if (!IsOverlineColor())
        aOverlineColor = GetTextColor();

    if ((eUnderline == LINESTYLE_SMALLWAVE) || (eUnderline == LINESTYLE_WAVE)
        || (eUnderline == LINESTYLE_DOUBLEWAVE) || (eUnderline == LINESTYLE_BOLDWAVE))
    {
        ImplDrawWaveTextLine(nX, nY, nDistX, 0, nWidth, eUnderline, aUnderlineColor,
                             bUnderlineAbove);
        bUnderlineDone = true;
    }
    if ((eOverline == LINESTYLE_SMALLWAVE) || (eOverline == LINESTYLE_WAVE)
        || (eOverline == LINESTYLE_DOUBLEWAVE) || (eOverline == LINESTYLE_BOLDWAVE))
    {
        ImplDrawWaveTextLine(nX, nY, nDistX, 0, nWidth, eOverline, aOverlineColor, true);
        bOverlineDone = true;
    }

    if ((eStrikeout == STRIKEOUT_SLASH) || (eStrikeout == STRIKEOUT_X))
    {
        ImplDrawStrikeoutChar(nX, nY, nDistX, 0, nWidth, eStrikeout, aStrikeoutColor);
        bStrikeoutDone = true;
    }

    if (!bUnderlineDone)
        ImplDrawStraightTextLine(nX, nY, nDistX, 0, nWidth, eUnderline, aUnderlineColor,
                                 bUnderlineAbove);

    if (!bOverlineDone)
        ImplDrawStraightTextLine(nX, nY, nDistX, 0, nWidth, eOverline, aOverlineColor, true);

    if (!bStrikeoutDone)
        ImplDrawStrikeoutLine(nX, nY, nDistX, 0, nWidth, eStrikeout, aStrikeoutColor);
}

void RenderContext2::GetCaretPositions(const OUString& rStr, tools::Long* pCaretXArray,
                                       sal_Int32 nIndex, sal_Int32 nLen,
                                       const SalLayoutGlyphs* pGlyphs) const
{
    if (nIndex >= rStr.getLength())
        return;
    if (nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    // layout complex text
    std::unique_ptr<SalLayout> pSalLayout
        = ImplLayout(rStr, nIndex, nLen, Point(0, 0), 0, nullptr, eDefaultLayout, nullptr, pGlyphs);
    if (!pSalLayout)
        return;

    int nWidthFactor = pSalLayout->GetUnitsPerPixel();
    pSalLayout->GetCaretPositions(2 * nLen, pCaretXArray);
    tools::Long nWidth = pSalLayout->GetTextWidth();

    // fixup unknown caret positions
    int i;
    for (i = 0; i < 2 * nLen; ++i)
        if (pCaretXArray[i] >= 0)
            break;
    tools::Long nXPos = pCaretXArray[i];
    for (i = 0; i < 2 * nLen; ++i)
    {
        if (pCaretXArray[i] >= 0)
            nXPos = pCaretXArray[i];
        else
            pCaretXArray[i] = nXPos;
    }

    // handle window mirroring
    if (IsRTLEnabled())
    {
        for (i = 0; i < 2 * nLen; ++i)
            pCaretXArray[i] = nWidth - pCaretXArray[i] - 1;
    }

    // convert from font units to logical units
    if (maGeometry.IsMapModeEnabled())
    {
        for (i = 0; i < 2 * nLen; ++i)
            pCaretXArray[i] = ImplDevicePixelToLogicWidth(pCaretXArray[i]);
    }

    if (nWidthFactor != 1)
    {
        for (i = 0; i < 2 * nLen; ++i)
            pCaretXArray[i] /= nWidthFactor;
    }
}

std::shared_ptr<vcl::TextLayoutCache> RenderContext2::CreateTextLayoutCache(OUString const& rString)
{
    return GenericSalLayout::CreateTextLayoutCache(rString);
}

bool RenderContext2::GetTextIsRTL(const OUString& rString, sal_Int32 nIndex, sal_Int32 nLen) const
{
    OUString aStr(rString);
    ImplLayoutArgs aArgs = ImplPrepareLayoutArgs(aStr, nIndex, nLen, 0, nullptr);
    bool bRTL = false;
    int nCharPos = -1;
    if (!aArgs.GetNextPos(&nCharPos, &bRTL))
        return false;
    return (nCharPos != nIndex);
}

sal_Int32 RenderContext2::GetTextBreak(const OUString& rStr, tools::Long nTextWidth,
                                       sal_Int32 nIndex, sal_Int32 nLen, tools::Long nCharExtra,
                                       vcl::TextLayoutCache const* const pLayoutCache,
                                       const SalLayoutGlyphs* pGlyphs) const
{
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rStr, nIndex, nLen, Point(0, 0), 0, nullptr,
                                                       eDefaultLayout, pLayoutCache, pGlyphs);
    sal_Int32 nRetVal = -1;
    if (pSalLayout)
    {
        // convert logical widths into layout units
        // NOTE: be very careful to avoid rounding errors for nCharExtra case
        // problem with rounding errors especially for small nCharExtras
        // TODO: remove when layout units have subpixel granularity
        tools::Long nWidthFactor = pSalLayout->GetUnitsPerPixel();
        tools::Long nSubPixelFactor = (nWidthFactor < 64) ? 64 : 1;
        nTextWidth *= nWidthFactor * nSubPixelFactor;
        DeviceCoordinate nTextPixelWidth = LogicWidthToDeviceCoordinate(nTextWidth);
        DeviceCoordinate nExtraPixelWidth = 0;
        if (nCharExtra != 0)
        {
            nCharExtra *= nWidthFactor * nSubPixelFactor;
            nExtraPixelWidth = LogicWidthToDeviceCoordinate(nCharExtra);
        }
        nRetVal = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);
    }

    return nRetVal;
}

sal_Int32 RenderContext2::GetTextBreak(const OUString& rStr, tools::Long nTextWidth,
                                       sal_Unicode nHyphenChar, sal_Int32& rHyphenPos,
                                       sal_Int32 nIndex, sal_Int32 nLen, tools::Long nCharExtra,
                                       vcl::TextLayoutCache const* const pLayoutCache) const
{
    rHyphenPos = -1;

    std::unique_ptr<SalLayout> pSalLayout
        = ImplLayout(rStr, nIndex, nLen, Point(0, 0), 0, nullptr, eDefaultLayout, pLayoutCache);
    sal_Int32 nRetVal = -1;
    if (pSalLayout)
    {
        // convert logical widths into layout units
        // NOTE: be very careful to avoid rounding errors for nCharExtra case
        // problem with rounding errors especially for small nCharExtras
        // TODO: remove when layout units have subpixel granularity
        tools::Long nWidthFactor = pSalLayout->GetUnitsPerPixel();
        tools::Long nSubPixelFactor = (nWidthFactor < 64) ? 64 : 1;

        nTextWidth *= nWidthFactor * nSubPixelFactor;
        DeviceCoordinate nTextPixelWidth = LogicWidthToDeviceCoordinate(nTextWidth);
        DeviceCoordinate nExtraPixelWidth = 0;
        if (nCharExtra != 0)
        {
            nCharExtra *= nWidthFactor * nSubPixelFactor;
            nExtraPixelWidth = LogicWidthToDeviceCoordinate(nCharExtra);
        }

        // calculate un-hyphenated break position
        nRetVal = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);

        // calculate hyphenated break position
        OUString aHyphenStr(nHyphenChar);
        std::unique_ptr<SalLayout> pHyphenLayout = ImplLayout(aHyphenStr, 0, 1);
        if (pHyphenLayout)
        {
            // calculate subpixel width of hyphenation character
            tools::Long nHyphenPixelWidth = pHyphenLayout->GetTextWidth() * nSubPixelFactor;

            // calculate hyphenated break position
            nTextPixelWidth -= nHyphenPixelWidth;
            if (nExtraPixelWidth > 0)
                nTextPixelWidth -= nExtraPixelWidth;

            rHyphenPos
                = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);

            if (rHyphenPos > nRetVal)
                rHyphenPos = nRetVal;
        }
    }

    return nRetVal;
}

void RenderContext2::ImplDrawText(tools::Rectangle const& rRect, OUString const& rOrigStr,
                                  DrawTextFlags nStyle, std::vector<tools::Rectangle>* pVector,
                                  OUString* pDisplayText, vcl::ITextLayout& _rLayout)
{
    Color aOldTextColor;
    Color aOldTextFillColor;
    bool bRestoreFillColor = false;
    if ((nStyle & DrawTextFlags::Disable) && !pVector)
    {
        bool bHighContrastBlack = false;
        bool bHighContrastWhite = false;
        const StyleSettings& rStyleSettings(GetSettings().GetStyleSettings());
        if (rStyleSettings.GetHighContrastMode())
        {
            Color aCol;
            if (IsBackground())
                aCol = GetBackground().GetColor();
            else
                // best guess is the face color here
                // but it may be totally wrong. the background color
                // was typically already reset
                aCol = rStyleSettings.GetFaceColor();

            bHighContrastBlack = aCol.IsDark();
            bHighContrastWhite = aCol.IsBright();
        }

        aOldTextColor = GetTextColor();
        if (IsTextFillColor())
        {
            bRestoreFillColor = true;
            aOldTextFillColor = GetTextFillColor();
        }

        if (bHighContrastBlack)
        {
            SetTextColor(COL_GREEN);
        }
        else if (bHighContrastWhite)
        {
            SetTextColor(COL_LIGHTGREEN);
        }
        else
        {
            // draw disabled text always without shadow
            // as it fits better with native look
            SetTextColor(GetSettings().GetStyleSettings().GetDisableColor());
        }
    }

    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();

    if (((nWidth <= 0) || (nHeight <= 0)) && (nStyle & DrawTextFlags::Clip))
        return;

    Point aPos = rRect.TopLeft();

    tools::Long nTextHeight = GetTextHeight();
    TextAlign eAlign = GetTextAlign();
    sal_Int32 nMnemonicPos = -1;

    OUString aStr = rOrigStr;
    if (nStyle & DrawTextFlags::Mnemonic)
        aStr = GetNonMnemonicString(aStr, nMnemonicPos);

    const bool bDrawMnemonics
        = !(GetSettings().GetStyleSettings().GetOptions() & StyleSettingsOptions::NoMnemonics)
          && !pVector;

    // We treat multiline text differently
    if (nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        ImplTextLineInfo* pLineInfo;
        sal_Int32 i;
        sal_Int32 nFormatLines;

        if (nTextHeight)
        {
            tools::Long nMaxTextWidth
                = ImplGetTextLines(aMultiLineInfo, nWidth, aStr, nStyle, _rLayout);
            sal_Int32 nLines = static_cast<sal_Int32>(nHeight / nTextHeight);
            OUString aLastLine;
            nFormatLines = aMultiLineInfo.Count();
            if (nLines <= 0)
                nLines = 1;
            if (nFormatLines > nLines)
            {
                if (nStyle & DrawTextFlags::EndEllipsis)
                {
                    // Create last line and shorten it
                    nFormatLines = nLines - 1;

                    pLineInfo = aMultiLineInfo.GetLine(nFormatLines);
                    aLastLine = convertLineEnd(aStr.copy(pLineInfo->GetIndex()), LINEEND_LF);
                    // Replace all LineFeeds with Spaces
                    OUStringBuffer aLastLineBuffer(aLastLine);
                    sal_Int32 nLastLineLen = aLastLineBuffer.getLength();
                    for (i = 0; i < nLastLineLen; i++)
                    {
                        if (aLastLineBuffer[i] == '\n')
                            aLastLineBuffer[i] = ' ';
                    }
                    aLastLine = aLastLineBuffer.makeStringAndClear();
                    aLastLine = ImplGetEllipsisString(aLastLine, nWidth, nStyle, _rLayout);
                    nStyle &= ~DrawTextFlags(DrawTextFlags::VCenter | DrawTextFlags::Bottom);
                    nStyle |= DrawTextFlags::Top;
                }
            }
            else
            {
                if (nMaxTextWidth <= nWidth)
                    nStyle &= ~DrawTextFlags::Clip;
            }

            // Do we need to clip the height?
            if (nFormatLines * nTextHeight > nHeight)
                nStyle |= DrawTextFlags::Clip;

            // Set clipping
            if (nStyle & DrawTextFlags::Clip)
            {
                Push(PushFlags::CLIPREGION);
                IntersectClipRegion(vcl::Region(rRect));
            }

            // Vertical alignment
            if (nStyle & DrawTextFlags::Bottom)
                aPos.AdjustY(nHeight - (nFormatLines * nTextHeight));
            else if (nStyle & DrawTextFlags::VCenter)
                aPos.AdjustY((nHeight - (nFormatLines * nTextHeight)) / 2);

            // Font alignment
            if (eAlign == ALIGN_BOTTOM)
                aPos.AdjustY(nTextHeight);
            else if (eAlign == ALIGN_BASELINE)
                aPos.AdjustY(GetFontMetric().GetAscent());

            // Output all lines except for the last one
            for (i = 0; i < nFormatLines; i++)
            {
                pLineInfo = aMultiLineInfo.GetLine(i);
                if (nStyle & DrawTextFlags::Right)
                    aPos.AdjustX(nWidth - pLineInfo->GetWidth());
                else if (nStyle & DrawTextFlags::Center)
                    aPos.AdjustX((nWidth - pLineInfo->GetWidth()) / 2);
                sal_Int32 nIndex = pLineInfo->GetIndex();
                sal_Int32 nLineLen = pLineInfo->GetLen();
                _rLayout.DrawText(aPos, aStr, nIndex, nLineLen, pVector, pDisplayText);
                if (bDrawMnemonics)
                {
                    if ((nMnemonicPos >= nIndex) && (nMnemonicPos < nIndex + nLineLen))
                    {
                        tools::Long nMnemonicX;
                        tools::Long nMnemonicY;
                        DeviceCoordinate nMnemonicWidth;

                        std::unique_ptr<tools::Long[]> const pCaretXArray(
                            new tools::Long[2 * nLineLen]);
                        /*sal_Bool bRet =*/_rLayout.GetCaretPositions(aStr, pCaretXArray.get(),
                                                                      nIndex, nLineLen);
                        tools::Long lc_x1 = pCaretXArray[2 * (nMnemonicPos - nIndex)];
                        tools::Long lc_x2 = pCaretXArray[2 * (nMnemonicPos - nIndex) + 1];
                        nMnemonicWidth = LogicWidthToDeviceCoordinate(std::abs(lc_x1 - lc_x2));

                        Point aTempPos = LogicToPixel(aPos);
                        nMnemonicX = GetFrameOffset().X() + aTempPos.X()
                                     + ImplLogicWidthToDevicePixel(std::min(lc_x1, lc_x2));
                        nMnemonicY = GetFrameOffset().Y() + aTempPos.Y()
                                     + ImplLogicWidthToDevicePixel(GetFontMetric().GetAscent());
                        ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
                    }
                }
                aPos.AdjustY(nTextHeight);
                aPos.setX(rRect.Left());
            }

            // If there still is a last line, we output it left-aligned as the line would be clipped
            if (!aLastLine.isEmpty())
                _rLayout.DrawText(aPos, aLastLine, 0, aLastLine.getLength(), pVector, pDisplayText);

            // Reset clipping
            if (nStyle & DrawTextFlags::Clip)
                Pop();
        }
    }
    else
    {
        tools::Long nTextWidth = _rLayout.GetTextWidth(aStr, 0, -1);

        // Clip text if needed
        if (nTextWidth > nWidth)
        {
            if (nStyle & TEXT_DRAW_ELLIPSIS)
            {
                aStr = ImplGetEllipsisString(aStr, nWidth, nStyle, _rLayout);
                nStyle &= ~DrawTextFlags(DrawTextFlags::Center | DrawTextFlags::Right);
                nStyle |= DrawTextFlags::Left;
                nTextWidth = _rLayout.GetTextWidth(aStr, 0, aStr.getLength());
            }
        }
        else
        {
            if (nTextHeight <= nHeight)
                nStyle &= ~DrawTextFlags::Clip;
        }

        // horizontal text alignment
        if (nStyle & DrawTextFlags::Right)
            aPos.AdjustX(nWidth - nTextWidth);
        else if (nStyle & DrawTextFlags::Center)
            aPos.AdjustX((nWidth - nTextWidth) / 2);

        // vertical font alignment
        if (eAlign == ALIGN_BOTTOM)
            aPos.AdjustY(nTextHeight);
        else if (eAlign == ALIGN_BASELINE)
            aPos.AdjustY(GetFontMetric().GetAscent());

        if (nStyle & DrawTextFlags::Bottom)
            aPos.AdjustY(nHeight - nTextHeight);
        else if (nStyle & DrawTextFlags::VCenter)
            aPos.AdjustY((nHeight - nTextHeight) / 2);

        tools::Long nMnemonicX = 0;
        tools::Long nMnemonicY = 0;
        DeviceCoordinate nMnemonicWidth = 0;
        if (nMnemonicPos != -1)
        {
            std::unique_ptr<tools::Long[]> const pCaretXArray(
                new tools::Long[2 * aStr.getLength()]);
            /*sal_Bool bRet =*/_rLayout.GetCaretPositions(aStr, pCaretXArray.get(), 0,
                                                          aStr.getLength());
            tools::Long lc_x1 = pCaretXArray[2 * nMnemonicPos];
            tools::Long lc_x2 = pCaretXArray[2 * nMnemonicPos + 1];
            nMnemonicWidth = LogicWidthToDeviceCoordinate(std::abs(lc_x1 - lc_x2));

            Point aTempPos = LogicToPixel(aPos);
            nMnemonicX = GetFrameOffset().X() + aTempPos.X()
                         + ImplLogicWidthToDevicePixel(std::min(lc_x1, lc_x2));
            nMnemonicY = GetFrameOffset().Y() + aTempPos.Y()
                         + ImplLogicWidthToDevicePixel(GetFontMetric().GetAscent());
        }

        if (nStyle & DrawTextFlags::Clip)
        {
            Push(PushFlags::CLIPREGION);
            IntersectClipRegion(vcl::Region(rRect));
            _rLayout.DrawText(aPos, aStr, 0, aStr.getLength(), pVector, pDisplayText);
            if (bDrawMnemonics && nMnemonicPos != -1)
                ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
            Pop();
        }
        else
        {
            _rLayout.DrawText(aPos, aStr, 0, aStr.getLength(), pVector, pDisplayText);
            if (bDrawMnemonics && nMnemonicPos != -1)
                ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
        }
    }

    if (nStyle & DrawTextFlags::Disable && !pVector)
    {
        SetTextColor(aOldTextColor);
        if (bRestoreFillColor)
            SetTextFillColor(aOldTextFillColor);
    }
}

tools::Rectangle RenderContext2::GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                             DrawTextFlags nStyle, TextRectInfo* pInfo,
                                             const vcl::ITextLayout* _pTextLayout) const
{
    tools::Rectangle aRect = rRect;
    sal_Int32 nLines;
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nMaxWidth;
    tools::Long nTextHeight = GetTextHeight();

    OUString aStr = rStr;
    if (nStyle & DrawTextFlags::Mnemonic)
        aStr = GetNonMnemonicString(aStr);

    if (nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        ImplTextLineInfo* pLineInfo;
        sal_Int32 nFormatLines;
        sal_Int32 i;

        nMaxWidth = 0;
        vcl::DefaultTextLayout aDefaultLayout(*const_cast<RenderContext2*>(this));
        ImplGetTextLines(aMultiLineInfo, nWidth, aStr, nStyle,
                         _pTextLayout ? *_pTextLayout : aDefaultLayout);
        nFormatLines = aMultiLineInfo.Count();
        if (!nTextHeight)
            nTextHeight = 1;
        nLines = static_cast<sal_uInt16>(aRect.GetHeight() / nTextHeight);
        if (pInfo)
            pInfo->mnLineCount = nFormatLines;
        if (!nLines)
            nLines = 1;
        if (nFormatLines <= nLines)
            nLines = nFormatLines;
        else
        {
            if (!(nStyle & DrawTextFlags::EndEllipsis))
                nLines = nFormatLines;
            else
            {
                if (pInfo)
                    pInfo->mbEllipsis = true;
                nMaxWidth = nWidth;
            }
        }
        if (pInfo)
        {
            bool bMaxWidth = nMaxWidth == 0;
            pInfo->mnMaxWidth = 0;
            for (i = 0; i < nLines; i++)
            {
                pLineInfo = aMultiLineInfo.GetLine(i);
                if (bMaxWidth && (pLineInfo->GetWidth() > nMaxWidth))
                    nMaxWidth = pLineInfo->GetWidth();
                if (pLineInfo->GetWidth() > pInfo->mnMaxWidth)
                    pInfo->mnMaxWidth = pLineInfo->GetWidth();
            }
        }
        else if (!nMaxWidth)
        {
            for (i = 0; i < nLines; i++)
            {
                pLineInfo = aMultiLineInfo.GetLine(i);
                if (pLineInfo->GetWidth() > nMaxWidth)
                    nMaxWidth = pLineInfo->GetWidth();
            }
        }
    }
    else
    {
        nLines = 1;
        nMaxWidth = _pTextLayout ? _pTextLayout->GetTextWidth(aStr, 0, aStr.getLength())
                                 : GetTextWidth(aStr);

        if (pInfo)
        {
            pInfo->mnLineCount = 1;
            pInfo->mnMaxWidth = nMaxWidth;
        }

        if ((nMaxWidth > nWidth) && (nStyle & TEXT_DRAW_ELLIPSIS))
        {
            if (pInfo)
                pInfo->mbEllipsis = true;
            nMaxWidth = nWidth;
        }
    }

    if (nStyle & DrawTextFlags::Right)
        aRect.SetLeft(aRect.Right() - nMaxWidth + 1);
    else if (nStyle & DrawTextFlags::Center)
    {
        aRect.AdjustLeft((nWidth - nMaxWidth) / 2);
        aRect.SetRight(aRect.Left() + nMaxWidth - 1);
    }
    else
        aRect.SetRight(aRect.Left() + nMaxWidth - 1);

    if (nStyle & DrawTextFlags::Bottom)
        aRect.SetTop(aRect.Bottom() - (nTextHeight * nLines) + 1);
    else if (nStyle & DrawTextFlags::VCenter)
    {
        aRect.AdjustTop((aRect.GetHeight() - (nTextHeight * nLines)) / 2);
        aRect.SetBottom(aRect.Top() + (nTextHeight * nLines) - 1);
    }
    else
        aRect.SetBottom(aRect.Top() + (nTextHeight * nLines) - 1);

    // #99188# get rid of rounding problems when using this rect later
    if (nStyle & DrawTextFlags::Right)
        aRect.AdjustLeft(-1);
    else
        aRect.AdjustRight(1);
    return aRect;
}

OUString RenderContext2::GetEllipsisString(const OUString& rOrigStr, tools::Long nMaxWidth,
                                           DrawTextFlags nStyle) const
{
    vcl::DefaultTextLayout aTextLayout(*const_cast<RenderContext2*>(this));
    return ImplGetEllipsisString(rOrigStr, nMaxWidth, nStyle, aTextLayout);
}

void RenderContext2::DrawCtrlText(const Point& rPos, const OUString& rStr, sal_Int32 nIndex,
                                  sal_Int32 nLen, DrawTextFlags nStyle,
                                  std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                                  const SalLayoutGlyphs* pGlyphs)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
    {
        nLen = rStr.getLength() - nIndex;
    }

    if (!IsDeviceOutputNecessary() || (nIndex >= rStr.getLength()))
        return;

    // better get graphics here because ImplDrawMnemonicLine() will not
    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (nIndex >= rStr.getLength())
        return;

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
    {
        nLen = rStr.getLength() - nIndex;
    }
    OUString aStr = rStr;
    sal_Int32 nMnemonicPos = -1;

    tools::Long nMnemonicX = 0;
    tools::Long nMnemonicY = 0;
    tools::Long nMnemonicWidth = 0;
    if ((nStyle & DrawTextFlags::Mnemonic) && nLen > 1)
    {
        aStr = GetNonMnemonicString(aStr, nMnemonicPos);
        if (nMnemonicPos != -1)
        {
            if (nMnemonicPos < nIndex)
            {
                --nIndex;
            }
            else
            {
                if (nMnemonicPos < (nIndex + nLen))
                    --nLen;
                SAL_WARN_IF(nMnemonicPos >= (nIndex + nLen), "vcl",
                            "Mnemonic underline marker after last character");
            }
            bool bInvalidPos = false;

            if (nMnemonicPos >= nLen)
            {
                // may occur in BiDi-Strings: the '~' is sometimes found behind the last char
                // due to some strange BiDi text editors
                // -> place the underline behind the string to indicate a failure
                bInvalidPos = true;
                nMnemonicPos = nLen - 1;
            }

            std::unique_ptr<tools::Long[]> const pCaretXArray(new tools::Long[2 * nLen]);
            /*sal_Bool bRet =*/GetCaretPositions(aStr, pCaretXArray.get(), nIndex, nLen, pGlyphs);
            tools::Long lc_x1 = pCaretXArray[2 * (nMnemonicPos - nIndex)];
            tools::Long lc_x2 = pCaretXArray[2 * (nMnemonicPos - nIndex) + 1];
            nMnemonicWidth = ::abs(static_cast<int>(lc_x1 - lc_x2));

            Point aTempPos(std::min(lc_x1, lc_x2), GetFontMetric().GetAscent());
            if (bInvalidPos) // #106952#, place behind the (last) character
                aTempPos = Point(std::max(lc_x1, lc_x2), GetFontMetric().GetAscent());

            aTempPos += rPos;
            aTempPos = LogicToPixel(aTempPos);
            nMnemonicX = GetFrameOffset().X() + aTempPos.X();
            nMnemonicY = GetFrameOffset().Y() + aTempPos.Y();
        }
    }

    bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;

    if (nStyle & DrawTextFlags::Disable && !pVector)
    {
        Color aOldTextColor;
        Color aOldTextFillColor;
        bool bRestoreFillColor;
        bool bHighContrastBlack = false;
        bool bHighContrastWhite = false;
        const StyleSettings& rStyleSettings(GetSettings().GetStyleSettings());
        if (rStyleSettings.GetHighContrastMode())
        {
            if (IsBackground())
            {
                Wallpaper aWall = GetBackground();
                Color aCol = aWall.GetColor();
                bHighContrastBlack = aCol.IsDark();
                bHighContrastWhite = aCol.IsBright();
            }
        }

        aOldTextColor = GetTextColor();
        if (IsTextFillColor())
        {
            bRestoreFillColor = true;
            aOldTextFillColor = GetTextFillColor();
        }
        else
            bRestoreFillColor = false;

        if (bHighContrastBlack)
            SetTextColor(COL_GREEN);
        else if (bHighContrastWhite)
            SetTextColor(COL_LIGHTGREEN);
        else
            SetTextColor(GetSettings().GetStyleSettings().GetDisableColor());

        DrawText(rPos, aStr, nIndex, nLen, pVector, pDisplayText);
        if (!(GetSettings().GetStyleSettings().GetOptions() & StyleSettingsOptions::NoMnemonics)
            && (!autoacc || !(nStyle & DrawTextFlags::HideMnemonic)))
        {
            if (nMnemonicPos != -1)
                ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
        }
        SetTextColor(aOldTextColor);
        if (bRestoreFillColor)
            SetTextFillColor(aOldTextFillColor);
    }
    else
    {
        DrawText(rPos, aStr, nIndex, nLen, pVector, pDisplayText, pGlyphs);
        if (!(GetSettings().GetStyleSettings().GetOptions() & StyleSettingsOptions::NoMnemonics)
            && !pVector && (!autoacc || !(nStyle & DrawTextFlags::HideMnemonic)))
        {
            if (nMnemonicPos != -1)
                ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
        }
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawCtrlText(rPos, rStr, nIndex, nLen, nStyle, pVector, pDisplayText);
}

tools::Long RenderContext2::GetCtrlTextWidth(const OUString& rStr,
                                             const SalLayoutGlyphs* pGlyphs) const
{
    sal_Int32 nLen = rStr.getLength();
    sal_Int32 nIndex = 0;

    sal_Int32 nMnemonicPos;
    OUString aStr = GetNonMnemonicString(rStr, nMnemonicPos);
    if (nMnemonicPos != -1)
    {
        if (nMnemonicPos < nIndex)
            nIndex--;
        else if (static_cast<sal_uLong>(nMnemonicPos) < static_cast<sal_uLong>(nIndex + nLen))
            nLen--;
    }
    return GetTextWidth(aStr, nIndex, nLen, nullptr, pGlyphs);
}

bool RenderContext2::GetTextOutlines(basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                                     sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                     sal_uLong nLayoutWidth, const tools::Long* pDXArray) const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return false;

    bool bRet = false;

    rVector.clear();

    if (nLen < 0)
        nLen = rStr.getLength() - nIndex;

    rVector.reserve(nLen);

    // we want to get the Rectangle in logical units, so to
    // avoid rounding errors we just size the font in logical units
    bool bOldMap = maGeometry.IsMapModeEnabled();
    if (bOldMap)
    {
        pRC->EnableMapMode(false);
        pRC->mbNewFont = true;
    }

    std::unique_ptr<SalLayout> pSalLayout;

    // calculate offset when nBase!=nIndex
    tools::Long nXOffset = 0;
    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;
        pSalLayout = ImplLayout(rStr, nStart, nOfsLen, Point(0, 0), nLayoutWidth, pDXArray);
        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            pSalLayout.reset();
            // TODO: fix offset calculation for Bidi case
            if (nBase > nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = ImplLayout(rStr, nIndex, nLen, Point(0, 0), nLayoutWidth, pDXArray);
    if (pSalLayout)
    {
        bRet = pSalLayout->GetOutline(rVector);
        if (bRet)
        {
            // transform polygon to pixel units
            basegfx::B2DHomMatrix aMatrix;

            int nWidthFactor = pSalLayout->GetUnitsPerPixel();
            if (nXOffset | mnTextOffX | mnTextOffY)
            {
                Point aRotatedOfs(mnTextOffX * nWidthFactor, mnTextOffY * nWidthFactor);
                aRotatedOfs -= pSalLayout->GetDrawPosition(Point(nXOffset, 0));
                aMatrix.translate(aRotatedOfs.X(), aRotatedOfs.Y());
            }

            if (nWidthFactor > 1)
            {
                double fFactor = 1.0 / nWidthFactor;
                aMatrix.scale(fFactor, fFactor);
            }

            if (!aMatrix.isIdentity())
            {
                for (auto& elem : rVector)
                    elem.transform(aMatrix);
            }
        }

        pSalLayout.reset();
    }

    if (bOldMap)
    {
        // restore original font size and map mode
        pRC->EnableMapMode(bOldMap);
        pRC->mbNewFont = true;
    }

    return bRet;
}

bool RenderContext2::GetTextOutlines(PolyPolyVector& rResultVector, const OUString& rStr,
                                     sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                     sal_uLong nLayoutWidth, const tools::Long* pDXArray) const
{
    rResultVector.clear();

    // get the basegfx polypolygon vector
    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;
    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, nBase, nIndex, nLen, nLayoutWidth, pDXArray))
        return false;

    // convert to a tool polypolygon vector
    rResultVector.reserve(aB2DPolyPolyVector.size());
    for (auto const& elem : aB2DPolyPolyVector)
        rResultVector.emplace_back(elem); // #i76339#

    return true;
}

bool RenderContext2::GetTextOutline(tools::PolyPolygon& rPolyPoly, const OUString& rStr) const
{
    rPolyPoly.Clear();

    // get the basegfx polypolygon vector
    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;
    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, 0 /*nBase*/, 0 /*nIndex*/, /*nLen*/ -1,
                         /*nLayoutWidth*/ 0, /*pDXArray*/ nullptr))
        return false;

    // convert and merge into a tool polypolygon
    for (auto const& elem : aB2DPolyPolyVector)
        for (auto const& rB2DPolygon : elem)
            rPolyPoly.Insert(tools::Polygon(rB2DPolygon)); // #i76339#

    return true;
}

void RenderContext2::ImplDrawMnemonicLine(tools::Long nX, tools::Long nY, tools::Long nWidth)
{
    tools::Long nBaseX = nX;
    if (/*HasMirroredGraphics() &&*/ IsRTLEnabled())
    {
        // add some strange offset
        nX += 2;
        // revert the hack that will be done later in ImplDrawTextLine
        nX = nBaseX - nWidth - (nX - nBaseX - 1);
    }

    ImplDrawTextLine(nX, nY, 0, nWidth, STRIKEOUT_NONE, LINESTYLE_SINGLE, LINESTYLE_NONE, false);
}

void RenderContext2::ImplDrawWaveTextLine(tools::Long nBaseX, tools::Long nBaseY,
                                          tools::Long nDistX, tools::Long nDistY,
                                          tools::Long nWidth, FontLineStyle eTextLine, Color aColor,
                                          bool bIsAbove)
{
    FontInstance* pFontInstance = mpFontInstance.get();
    tools::Long nLineHeight;
    tools::Long nLinePos;

    if (bIsAbove)
    {
        nLineHeight = pFontInstance->mxFontMetric->GetAboveWavelineUnderlineSize();
        nLinePos = pFontInstance->mxFontMetric->GetAboveWavelineUnderlineOffset();
    }
    else
    {
        nLineHeight = pFontInstance->mxFontMetric->GetWavelineUnderlineSize();
        nLinePos = pFontInstance->mxFontMetric->GetWavelineUnderlineOffset();
    }
    if ((eTextLine == LINESTYLE_SMALLWAVE) && (nLineHeight > 3))
        nLineHeight = 3;

    tools::Long nLineWidth = GetDPIX() / 300;
    if (!nLineWidth)
        nLineWidth = 1;

    if (eTextLine == LINESTYLE_BOLDWAVE)
        nLineWidth *= 2;

    nLinePos += nDistY - (nLineHeight / 2);

    tools::Long nLineWidthHeight = ((nLineWidth * GetDPIX()) + (GetDPIY() / 2)) / GetDPIY();
    if (eTextLine == LINESTYLE_DOUBLEWAVE)
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

        nLinePos -= nLineWidthHeight - nLineDY2;
        ImplDrawWaveLine(nBaseX, nBaseY, nDistX, nLinePos, nWidth, nLineHeight, nLineWidth,
                         mpFontInstance->mnOrientation, aColor);
        nLinePos += nLineWidthHeight + nLineDY;
        ImplDrawWaveLine(nBaseX, nBaseY, nDistX, nLinePos, nWidth, nLineHeight, nLineWidth,
                         mpFontInstance->mnOrientation, aColor);
    }
    else
    {
        nLinePos -= nLineWidthHeight / 2;
        ImplDrawWaveLine(nBaseX, nBaseY, nDistX, nLinePos, nWidth, nLineHeight, nLineWidth,
                         mpFontInstance->mnOrientation, aColor);
    }
}

void RenderContext2::ImplDrawStraightTextLine(tools::Long nBaseX, tools::Long nBaseY,
                                              tools::Long nDistX, tools::Long nDistY,
                                              tools::Long nWidth, FontLineStyle eTextLine,
                                              Color aColor, bool bIsAbove)
{
    FontInstance* pFontInstance = mpFontInstance.get();
    tools::Long nLineHeight = 0;
    tools::Long nLinePos = 0;
    tools::Long nLinePos2 = 0;

    const tools::Long nY = nDistY;

    if (eTextLine > UNDERLINE_LAST)
        eTextLine = LINESTYLE_SINGLE;

    switch (eTextLine)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_DOTTED:
        case LINESTYLE_DASH:
        case LINESTYLE_LONGDASH:
        case LINESTYLE_DASHDOT:
        case LINESTYLE_DASHDOTDOT:
            if (bIsAbove)
            {
                nLineHeight = pFontInstance->mxFontMetric->GetAboveUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetAboveUnderlineOffset();
            }
            else
            {
                nLineHeight = pFontInstance->mxFontMetric->GetUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetUnderlineOffset();
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
                nLineHeight = pFontInstance->mxFontMetric->GetAboveBoldUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetAboveBoldUnderlineOffset();
            }
            else
            {
                nLineHeight = pFontInstance->mxFontMetric->GetBoldUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetBoldUnderlineOffset();
            }
            break;
        case LINESTYLE_DOUBLE:
            if (bIsAbove)
            {
                nLineHeight = pFontInstance->mxFontMetric->GetAboveDoubleUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetAboveDoubleUnderlineOffset1();
                nLinePos2 = nY + pFontInstance->mxFontMetric->GetAboveDoubleUnderlineOffset2();
            }
            else
            {
                nLineHeight = pFontInstance->mxFontMetric->GetDoubleUnderlineSize();
                nLinePos = nY + pFontInstance->mxFontMetric->GetDoubleUnderlineOffset1();
                nLinePos2 = nY + pFontInstance->mxFontMetric->GetDoubleUnderlineOffset2();
            }
            break;
        default:
            break;
    }

    if (!nLineHeight)
        return;

    if (mbLineColor || mbInitLineColor)
    {
        mpGraphics->SetLineColor();
        mbInitLineColor = true;
    }
    mpGraphics->SetFillColor(aColor);
    mbInitFillColor = true;

    tools::Long nLeft = nDistX;

    switch (eTextLine)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_BOLD:
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nWidth, nLineHeight);
            break;
        case LINESTYLE_DOUBLE:
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nWidth, nLineHeight);
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos2, nWidth, nLineHeight);
            break;
        case LINESTYLE_DOTTED:
        case LINESTYLE_BOLDDOTTED:
        {
            tools::Long nDotWidth = nLineHeight * GetDPIY();
            nDotWidth += GetDPIY() / 2;
            nDotWidth /= GetDPIY();

            tools::Long nTempWidth = nDotWidth;
            tools::Long nEnd = nLeft + nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempWidth > nEnd)
                    nTempWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempWidth, nLineHeight);
                nLeft += nDotWidth * 2;
            }
        }
        break;
        case LINESTYLE_DASH:
        case LINESTYLE_LONGDASH:
        case LINESTYLE_BOLDDASH:
        case LINESTYLE_BOLDLONGDASH:
        {
            tools::Long nDotWidth = nLineHeight * GetDPIY();
            nDotWidth += GetDPIY() / 2;
            nDotWidth /= GetDPIY();

            tools::Long nMinDashWidth;
            tools::Long nMinSpaceWidth;
            tools::Long nSpaceWidth;
            tools::Long nDashWidth;
            if ((eTextLine == LINESTYLE_LONGDASH) || (eTextLine == LINESTYLE_BOLDLONGDASH))
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
            nDashWidth = ((nDashWidth * GetDPIX()) + 1270) / 2540;
            nSpaceWidth = ((nSpaceWidth * GetDPIX()) + 1270) / 2540;
            // DashWidth will be increased if the line is getting too thick
            // in proportion to the line's length
            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;
            if (nSpaceWidth < nMinSpaceWidth)
                nSpaceWidth = nMinSpaceWidth;

            tools::Long nTempWidth = nDashWidth;
            tools::Long nEnd = nLeft + nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempWidth > nEnd)
                    nTempWidth = nEnd - nLeft;
                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempWidth, nLineHeight);
                nLeft += nDashWidth + nSpaceWidth;
            }
        }
        break;
        case LINESTYLE_DASHDOT:
        case LINESTYLE_BOLDDASHDOT:
        {
            tools::Long nDotWidth = nLineHeight * GetDPIY();
            nDotWidth += GetDPIY() / 2;
            nDotWidth /= GetDPIY();

            tools::Long nDashWidth = ((100 * GetDPIX()) + 1270) / 2540;
            tools::Long nMinDashWidth = nDotWidth * 4;
            // DashWidth will be increased if the line is getting too thick
            // in proportion to the line's length
            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;

            tools::Long nTempDotWidth = nDotWidth;
            tools::Long nTempDashWidth = nDashWidth;
            tools::Long nEnd = nLeft + nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempDotWidth, nLineHeight);
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDashWidth > nEnd)
                    nTempDashWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempDashWidth, nLineHeight);
                nLeft += nDashWidth + nDotWidth;
            }
        }
        break;
        case LINESTYLE_DASHDOTDOT:
        case LINESTYLE_BOLDDASHDOTDOT:
        {
            tools::Long nDotWidth = nLineHeight * GetDPIY();
            nDotWidth += GetDPIY() / 2;
            nDotWidth /= GetDPIY();

            tools::Long nDashWidth = ((100 * GetDPIX()) + 1270) / 2540;
            tools::Long nMinDashWidth = nDotWidth * 4;
            // DashWidth will be increased if the line is getting too thick
            // in proportion to the line's length
            if (nDashWidth < nMinDashWidth)
                nDashWidth = nMinDashWidth;

            tools::Long nTempDotWidth = nDotWidth;
            tools::Long nTempDashWidth = nDashWidth;
            tools::Long nEnd = nLeft + nWidth;
            while (nLeft < nEnd)
            {
                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempDotWidth, nLineHeight);
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDotWidth > nEnd)
                    nTempDotWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempDotWidth, nLineHeight);
                nLeft += nDotWidth * 2;
                if (nLeft > nEnd)
                    break;

                if (nLeft + nTempDashWidth > nEnd)
                    nTempDashWidth = nEnd - nLeft;

                ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nTempDashWidth, nLineHeight);
                nLeft += nDashWidth + nDotWidth;
            }
        }
        break;
        default:
            break;
    }
}

void RenderContext2::ImplDrawWaveLine(tools::Long nBaseX, tools::Long nBaseY, tools::Long nDistX,
                                      tools::Long nDistY, tools::Long nWidth, tools::Long nHeight,
                                      tools::Long nLineWidth, Degree10 nOrientation,
                                      Color const& rColor)
{
    if (!nHeight)
        return;
    tools::Long nStartX = nBaseX + nDistX;
    tools::Long nStartY = nBaseY + nDistY;
    // If the height is 1 pixel, it's enough output a line
    if ((nLineWidth == 1) && (nHeight == 1))
    {
        mpGraphics->SetLineColor(rColor);
        mbInitLineColor = true;
        tools::Long nEndX = nStartX + nWidth;
        tools::Long nEndY = nStartY;
        if (nOrientation)
        {
            Point aOriginPt(nBaseX, nBaseY);
            aOriginPt.RotateAround(nStartX, nStartY, nOrientation);
            aOriginPt.RotateAround(nEndX, nEndY, nOrientation);
        }
        mpGraphics->DrawLine(nStartX, nStartY, nEndX, nEndY, *this);
    }
    else
    {
        tools::Long nCurX = nStartX;
        tools::Long nCurY = nStartY;
        tools::Long nDiffX = 2;
        tools::Long nDiffY = nHeight - 1;
        tools::Long nCount = nWidth;
        tools::Long nOffY = -1;
        SetWaveLineColors(rColor, nLineWidth);
        Size aSize(GetWaveLineSize(nLineWidth));
        tools::Long nPixWidth = aSize.Width();
        tools::Long nPixHeight = aSize.Height();
        if (!nDiffY)
        {
            while (nWidth)
            {
                ImplDrawWavePixel(nBaseX, nBaseY, nCurX, nCurY, nLineWidth, nOrientation,
                                  mpGraphics, *this, nPixWidth, nPixHeight);
                nCurX++;
                nWidth--;
            }
        }
        else
        {
            nCurY += nDiffY;
            tools::Long nFreq = nCount / (nDiffX + nDiffY);
            while (nFreq--)
            {
                for (tools::Long i = nDiffY; i; --i)
                {
                    ImplDrawWavePixel(nBaseX, nBaseY, nCurX, nCurY, nLineWidth, nOrientation,
                                      mpGraphics, *this, nPixWidth, nPixHeight);
                    nCurX++;
                    nCurY += nOffY;
                }
                for (tools::Long i = nDiffX; i; --i)
                {
                    ImplDrawWavePixel(nBaseX, nBaseY, nCurX, nCurY, nLineWidth, nOrientation,
                                      mpGraphics, *this, nPixWidth, nPixHeight);
                    nCurX++;
                }
                nOffY = -nOffY;
            }
            nFreq = nCount % (nDiffX + nDiffY);
            if (nFreq)
            {
                for (tools::Long i = nDiffY; i && nFreq; --i, --nFreq)
                {
                    ImplDrawWavePixel(nBaseX, nBaseY, nCurX, nCurY, nLineWidth, nOrientation,
                                      mpGraphics, *this, nPixWidth, nPixHeight);
                    nCurX++;
                    nCurY += nOffY;
                }
                for (tools::Long i = nDiffX; i && nFreq; --i, --nFreq)
                {
                    ImplDrawWavePixel(nBaseX, nBaseY, nCurX, nCurY, nLineWidth, nOrientation,
                                      mpGraphics, *this, nPixWidth, nPixHeight);
                    nCurX++;
                }
            }
        }
    }
}

void RenderContext2::ImplDrawWavePixel(tools::Long nOriginX, tools::Long nOriginY,
                                       tools::Long nCurX, tools::Long nCurY, tools::Long nWidth,
                                       Degree10 nOrientation, SalGraphics* pGraphics,
                                       RenderContext2 const& rOutDev, tools::Long nPixWidth,
                                       tools::Long nPixHeight)
{
    if (nOrientation)
    {
        Point aPoint(nOriginX, nOriginY);
        aPoint.RotateAround(nCurX, nCurY, nOrientation);
    }

    if (DrawWavePixelAsRect(nWidth))
    {
        pGraphics->DrawRect(nCurX, nCurY, nPixWidth, nPixHeight, rOutDev);
    }
    else
    {
        pGraphics->DrawPixel(nCurX, nCurY, rOutDev);
    }
}

bool RenderContext2::DrawWavePixelAsRect(tools::Long nLineWidth) const
{
    if (nLineWidth > 1)
        return true;

    return false;
}

void RenderContext2::SetWaveLineColors(Color const& rColor, tools::Long nLineWidth)
{
    // On printers that output pixel via DrawRect()
    if (nLineWidth > 1)
    {
        if (mbLineColor || mbInitLineColor)
        {
            mpGraphics->SetLineColor();
            mbInitLineColor = true;
        }
        mpGraphics->SetFillColor(rColor);
        mbInitFillColor = true;
    }
    else
    {
        mpGraphics->SetLineColor(rColor);
        mbInitLineColor = true;
    }
}

Size RenderContext2::GetWaveLineSize(tools::Long nLineWidth) const
{
    if (nLineWidth > 1)
        return Size(nLineWidth, ((nLineWidth * GetDPIX()) + (GetDPIY() / 2)) / GetDPIY());

    return Size(1, 1);
}
void RenderContext2::ImplDrawStrikeoutLine(tools::Long nBaseX, tools::Long nBaseY,
                                           tools::Long nDistX, tools::Long nDistY,
                                           tools::Long nWidth, FontStrikeout eStrikeout,
                                           Color aColor)
{
    FontInstance* pFontInstance = mpFontInstance.get();
    tools::Long nLineHeight = 0;
    tools::Long nLinePos = 0;
    tools::Long nLinePos2 = 0;

    tools::Long nY = nDistY;

    if (eStrikeout > STRIKEOUT_LAST)
        eStrikeout = STRIKEOUT_SINGLE;

    switch (eStrikeout)
    {
        case STRIKEOUT_SINGLE:
            nLineHeight = pFontInstance->mxFontMetric->GetStrikeoutSize();
            nLinePos = nY + pFontInstance->mxFontMetric->GetStrikeoutOffset();
            break;
        case STRIKEOUT_BOLD:
            nLineHeight = pFontInstance->mxFontMetric->GetBoldStrikeoutSize();
            nLinePos = nY + pFontInstance->mxFontMetric->GetBoldStrikeoutOffset();
            break;
        case STRIKEOUT_DOUBLE:
            nLineHeight = pFontInstance->mxFontMetric->GetDoubleStrikeoutSize();
            nLinePos = nY + pFontInstance->mxFontMetric->GetDoubleStrikeoutOffset1();
            nLinePos2 = nY + pFontInstance->mxFontMetric->GetDoubleStrikeoutOffset2();
            break;
        default:
            break;
    }

    if (!nLineHeight)
        return;

    if (mbLineColor || mbInitLineColor)
    {
        mpGraphics->SetLineColor();
        mbInitLineColor = true;
    }
    mpGraphics->SetFillColor(aColor);
    mbInitFillColor = true;

    const tools::Long& nLeft = nDistX;

    switch (eStrikeout)
    {
        case STRIKEOUT_SINGLE:
        case STRIKEOUT_BOLD:
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nWidth, nLineHeight);
            break;
        case STRIKEOUT_DOUBLE:
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos, nWidth, nLineHeight);
            ImplDrawTextRect(nBaseX, nBaseY, nLeft, nLinePos2, nWidth, nLineHeight);
            break;
        default:
            break;
    }
}

void RenderContext2::ImplDrawStrikeoutChar(tools::Long nBaseX, tools::Long nBaseY,
                                           tools::Long nDistX, tools::Long nDistY,
                                           tools::Long nWidth, FontStrikeout eStrikeout,
                                           Color aColor)
{
    // See qadevOOo/testdocs/StrikeThrough.odt for examples if you need
    // to tweak this
    if (!nWidth)
        return;

    // prepare string for strikeout measurement
    const char cStrikeoutChar = eStrikeout == STRIKEOUT_SLASH ? '/' : 'X';
    static const int nTestStrLen = 4;
    static const int nMaxStrikeStrLen = 2048;
    sal_Unicode aChars[nMaxStrikeStrLen + 1]; // +1 for valgrind...

    for (int i = 0; i < nTestStrLen; ++i)
    {
        aChars[i] = cStrikeoutChar;
    }

    const OUString aStrikeoutTest(aChars, nTestStrLen);

    // calculate approximation of strikeout atom size
    tools::Long nStrikeoutWidth = 0;
    std::unique_ptr<SalLayout> pLayout = ImplLayout(aStrikeoutTest, 0, nTestStrLen);
    if (pLayout)
        nStrikeoutWidth = pLayout->GetTextWidth() / (nTestStrLen * pLayout->GetUnitsPerPixel());

    if (nStrikeoutWidth <= 0) // sanity check
        return;

    int nStrikeStrLen = (nWidth + (nStrikeoutWidth - 1)) / nStrikeoutWidth;
    if (nStrikeStrLen > nMaxStrikeStrLen)
        nStrikeStrLen = nMaxStrikeStrLen;

    // build the strikeout string
    for (int i = nTestStrLen; i < nStrikeStrLen; ++i)
    {
        aChars[i] = cStrikeoutChar;
    }

    const OUString aStrikeoutText(aChars, nStrikeStrLen);

    if (mpFontInstance->mnOrientation)
    {
        Point aOriginPt(0, 0);
        aOriginPt.RotateAround(nDistX, nDistY, mpFontInstance->mnOrientation);
    }

    nBaseX += nDistX;
    nBaseY += nDistY;

    // strikeout text has to be left aligned
    ComplexTextLayoutFlags nOrigTLM = mnTextLayoutMode;
    mnTextLayoutMode = ComplexTextLayoutFlags::BiDiStrong;
    pLayout = ImplLayout(aStrikeoutText, 0, aStrikeoutText.getLength());
    mnTextLayoutMode = nOrigTLM;

    if (!pLayout)
        return;

    // draw the strikeout text
    const Color aOldColor = GetTextColor();
    SetTextColor(aColor);
    ImplInitTextColor();

    pLayout->DrawBase() = Point(nBaseX + mnTextOffX, nBaseY + mnTextOffY);

    tools::Rectangle aPixelRect;
    aPixelRect.SetLeft(nBaseX + mnTextOffX);
    aPixelRect.SetRight(aPixelRect.Left() + nWidth);
    aPixelRect.SetBottom(nBaseY + mpFontInstance->mxFontMetric->GetDescent());
    aPixelRect.SetTop(nBaseY - mpFontInstance->mxFontMetric->GetAscent());

    if (mpFontInstance->mnOrientation)
    {
        tools::Polygon aPoly(aPixelRect);
        aPoly.Rotate(Point(nBaseX + mnTextOffX, nBaseY + mnTextOffY),
                     mpFontInstance->mnOrientation);
        aPixelRect = aPoly.GetBoundRect();
    }

    Push(PushFlags::CLIPREGION);
    IntersectClipRegion(vcl::Region(PixelToLogic(aPixelRect)));
    if (mbInitClipRegion)
        InitClipRegion();

    pLayout->DrawText(*mpGraphics);

    Pop();

    SetTextColor(aOldColor);
    ImplInitTextColor();
}

void RenderContext2::InitWaveLineColor(Color const& rColor, tools::Long)
{
    mpGraphics->SetLineColor(rColor);
    mbInitLineColor = true;
}

void RenderContext2::ImplInitTextLineSize()
{
    mpFontInstance->mxFontMetric->ImplInitTextLineSize(this);
}

void RenderContext2::ImplInitAboveTextLineSize()
{
    mpFontInstance->mxFontMetric->ImplInitAboveTextLineSize();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
