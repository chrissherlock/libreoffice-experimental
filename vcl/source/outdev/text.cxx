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

#include <sal/config.h>

#include <sal/log.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <tools/lineend.hxx>
#include <tools/debug.hxx>
#include <unotools/fontdefs.hxx>
#include <tools/fontenum.hxx>
#include <comphelper/configuration.hxx>

#include <vcl/ctrl.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/metaact.hxx>
#include <vcl/metric.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/textrectinfo.hxx>
#include <vcl/virdev.hxx>
#include <vcl/sysdata.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <text/TextLayoutRequest.hxx>
#include <ImplOutDevData.hxx>
#include <font/FontController.hxx>
#include <font/PhysicalFontFace.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <textlayout.hxx>
#include <textlineinfo.hxx>
#include <impglyphitem.hxx>
#include <TextLayoutCache.hxx>
#include <text/TextLayoutEngine.hxx>
#include <text/GraphicLayoutFactory.hxx>

#include <memory>
#include <comphelper/scopeguard.hxx>
#include <optional>

#define TEXT_DRAW_ELLIPSIS                                                                         \
    (DrawTextFlags::EndEllipsis | DrawTextFlags::PathEllipsis | DrawTextFlags::NewsEllipsis)

vcl::text::ComplexTextLayoutFlags OutputDevice::GetLayoutMode() const
{
    return mpFontRealization ? mpFontRealization->eLayoutMode : mpGraphicsState->mnTextLayoutMode;
}

void OutputDevice::SetLayoutMode(vcl::text::ComplexTextLayoutFlags nTextLayoutMode)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaLayoutModeAction(nTextLayoutMode));

    mpGraphicsState->mnTextLayoutMode = nTextLayoutMode;
    if (mpFontRealization)
        mpFontRealization->eLayoutMode = nTextLayoutMode;
}

LanguageType OutputDevice::GetDigitLanguage() const { return mpGraphicsState->meTextLanguage; }

void OutputDevice::SetDigitLanguage(LanguageType eTextLanguage)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLanguageAction(eTextLanguage));

    mpGraphicsState->meTextLanguage = eTextLanguage;
}

void OutputDevice::ImplInitTextColor()
{
    DBG_TESTSOLARMUTEX();

    if (mbInitTextColor)
    {
        mpGraphics->SetTextColor(GetTextColor());
        mbInitTextColor = false;
    }
}

OUString OutputDevice::GetEllipsisString(const OUString& rStr, tools::Long nMaxWidth,
                                         DrawTextFlags nStyle) const
{
    return vcl::text::TextLayoutEngine::GetEllipsisString(
        rStr, nMaxWidth, nStyle, [this](const OUString& s) { return GetTextWidth(s); });
}

void OutputDevice::ImplDrawTextRect(tools::Long nBaseX, tools::Long nBaseY, tools::Long nDistX,
                                    tools::Long nDistY, tools::Long nWidth, tools::Long nHeight)
{
    tools::Rectangle aLocalRect(Point(nDistX, nDistY), Size(nWidth, nHeight));

    auto aGeo = vcl::text::TextLayoutEngine::GetRotatedGeometry(
        Point(nBaseX, nBaseY), aLocalRect, mpFontRealization->mxFont->mnOrientation);

    if (aGeo.mbIsPolygon)
        ImplDrawPolygon(aGeo.maPoly);
    else
        mpGraphics->DrawRect(aGeo.maRect.Left(), aGeo.maRect.Top(), aGeo.maRect.GetWidth(),
                             aGeo.maRect.GetHeight(), *this);
}

void OutputDevice::ImplDrawTextBackground(const SalLayout& rSalLayout)
{
    tools::Rectangle aRect
        = vcl::text::TextLayoutEngine::GetTextInkBounds(rSalLayout, *mpFontRealization);

    if (mpGraphicsState->mbLineColor || mbLineColorDirty)
    {
        mpGraphics->SetLineColor();
        mbLineColorDirty = true;
    }

    mpGraphics->SetFillColor(GetTextFillColor());
    mbFillColorDirty = true;

    ImplDrawTextRect(aRect.Left(), aRect.Top(), 0, 0, aRect.GetWidth(), aRect.GetHeight());
}

bool OutputDevice::ImplDrawRotateText(SalLayout& rSalLayout)
{
    // Capture original state to ensure restoration on exit (success or failure)
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();
    basegfx::B2DPoint aOrigOffset = rSalLayout.DrawOffset();

    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        rSalLayout.DrawOffset() = aOrigOffset;
    });

    tools::Long nX = rSalLayout.DrawBase().getX();
    tools::Long nY = rSalLayout.DrawBase().getY();

    tools::Rectangle aBoundRect;
    rSalLayout.DrawBase() = basegfx::B2DPoint(0, 0);
    rSalLayout.DrawOffset() = basegfx::B2DPoint{ 0, 0 };

    // Get unrotated bounds (pass false for bApplyRotation) to size the virtual device.
    // This removes the need for manual heuristics if the layout fails to report bounds.
    aBoundRect
        = vcl::text::TextLayoutEngine::GetTextInkBounds(rSalLayout, *mpFontRealization, false);

    // cache virtual device for rotation
    if (!mpOutDevData->mpRotateDev)
        mpOutDevData->mpRotateDev = VclPtr<VirtualDevice>::Create(*this);

    VirtualDevice* pVDev = mpOutDevData->mpRotateDev;

    // size it accordingly
    if (!pVDev->SetOutputSizePixel(aBoundRect.GetSize()))
        return false;

    const vcl::font::FontSelectPattern& rPattern
        = mpFontRealization->mxFont->GetFontSelectPattern();

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
    rSalLayout.DrawBase().adjustX(-aBoundRect.Left());
    rSalLayout.DrawBase().adjustY(-aBoundRect.Top());
    rSalLayout.DrawText(*pVDev->mpGraphics);

    Bitmap aBmp = pVDev->GetBitmap(Point(), aBoundRect.GetSize());

    if (aBmp.IsEmpty() || !aBmp.Rotate(mpFontRealization->mxFont->mnOwnOrientation, COL_WHITE))
        return false;

    Point aPoint = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(
        Point(nX, nY), aBoundRect, mpFontRealization->mxFont->mnOwnOrientation);

    // mask output with text colored bitmap
    GDIMetaFile* pOldMetaFile = mpMetaFile;
    tools::Long nOldOffX = GetOutOffXPixel();
    tools::Long nOldOffY = GetOutOffYPixel();
    bool bOldMap = mpMapper->IsMapModeEnabled();

    SetDeviceOriginX(0);
    SetDeviceOriginY(0);

    mpMetaFile = nullptr;
    mpMapper->EnableMapMode(false);

    DrawMask(aPoint, aBmp, GetTextColor());

    mpMapper->EnableMapMode(bOldMap);
    SetDeviceOriginX(nOldOffX);
    SetDeviceOriginY(nOldOffY);
    mpMetaFile = pOldMetaFile;

    return true;
}

void OutputDevice::ImplRenderLayout(SalLayout& rSalLayout, bool bTextLines)
{
    if (mpFontRealization->mxFont->mnOwnOrientation)
    {
        if (ImplDrawRotateText(rSalLayout))
            return;
    }

    // Render Glyphs (with potential Mirroring/RTL)
    // We wrap this in a scope. The ScopeGuard ensures that when we leave this block
    // (and move to drawing lines), the X coordinate is restored to its original value.
    {
        tools::Long nOldX = rSalLayout.DrawBase().getX();
        comphelper::ScopeGuard aRestoreGuard([&]() { rSalLayout.DrawBase().setX(nOldX); });

        if (HasMirroredGraphics() || IsRTLEnabled())
        {
            vcl::text::MirroringContext aCtx{ nOldX,
                                              IsVirtual() ? GetOutputWidthPixel()
                                                          : mpGraphics->GetGraphicsWidth(),
                                              GetOutputWidthPixel(),
                                              GetOutOffXPixel(),
                                              HasMirroredGraphics(),
                                              IsRTLEnabled() };

            rSalLayout.DrawBase().setX(vcl::text::TextLayoutEngine::GetMirroredX(aCtx));
        }

        rSalLayout.DrawText(*mpGraphics);
    }

    // Render Decorations
    // X is now restored, so lines are drawn in the correct logical position.
    if (bTextLines)
    {
        const vcl::Font& rFont = mpGraphicsState->maFont;
        ImplDrawTextLines(rSalLayout, rFont.GetStrikeout(), rFont.GetUnderline(),
                          rFont.GetOverline(), rFont.IsWordLineMode(), rFont.IsUnderlineAbove());
    }

    if (mpGraphicsState->maFont.GetEmphasisMark() & FontEmphasisMark::Style)
        ImplDrawEmphasisMarks(rSalLayout);
}

void OutputDevice::ImplDrawSpecialText(SalLayout& rSalLayout)
{
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();
    basegfx::B2DPoint aOrigOffset = rSalLayout.DrawOffset();

    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        rSalLayout.DrawOffset() = aOrigOffset;
    });

    if (mpGraphicsState->maFont.GetRelief() != FontRelief::NONE)
    {
        ImplDrawReliefText(rSalLayout);
    }
    else
    {
        if (mpGraphicsState->maFont.IsShadow())
            ImplDrawShadowText(rSalLayout);

        if (mpGraphicsState->maFont.IsOutline())
            ImplDrawOutlineText(rSalLayout);
    }
}

void OutputDevice::ImplDrawReliefText(SalLayout& rSalLayout)
{
    Color aOldColor = GetTextColor();
    Color aOldTextLineColor = GetTextLineColor();
    Color aOldOverlineColor = GetOverlineColor();
    basegfx::B2DPoint aOrigOffset = rSalLayout.DrawOffset();

    Color aReliefColor(COL_LIGHTGRAY);
    Color aTextColor(aOldColor);

    // Black text is always drawn on white in VCL logic
    if (aTextColor == COL_BLACK) aTextColor = COL_WHITE;
    Color aEffectiveLineColor = (aOldTextLineColor == COL_BLACK) ? COL_WHITE : aOldTextLineColor;
    Color aEffectiveOverlineColor = (aOldOverlineColor == COL_BLACK) ? COL_WHITE : aOldOverlineColor;

    // Relief color is black for white text
    if (aTextColor == COL_WHITE) aReliefColor = COL_BLACK;

    // Draw Relief Shadow
    SetTextColor(aReliefColor);
    SetTextLineColor(aReliefColor);
    SetOverlineColor(aReliefColor);
    ImplInitTextColor();

    tools::Long nOff = vcl::text::TextLayoutEngine::GetReliefOffset(GetDPIX(), mpGraphicsState->maFont.GetRelief());
    rSalLayout.DrawOffset() += basegfx::B2DPoint(nOff, nOff);

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    // Draw Main Text
    rSalLayout.DrawOffset() = aOrigOffset;

    SetTextColor(aTextColor);
    SetTextLineColor(aEffectiveLineColor);
    SetOverlineColor(aEffectiveOverlineColor);
    ImplInitTextColor();

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    SetTextColor(aOldColor);
    SetTextLineColor(aOldTextLineColor);
    SetOverlineColor(aOldOverlineColor);
    ImplInitTextColor();
}

void OutputDevice::ImplDrawShadowText(SalLayout& rSalLayout)
{
    Color aOldColor = GetTextColor();
    Color aOldTextLineColor = GetTextLineColor();
    Color aOldOverlineColor = GetOverlineColor();
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();

    // Setup Shadow Colors
    SetTextLineColor();
    SetOverlineColor();

    if ((GetTextColor() == COL_BLACK) || (GetTextColor().GetLuminance() < 8))
        SetTextColor(COL_LIGHTGRAY);
    else
        SetTextColor(COL_BLACK);
    ImplInitTextColor();

    // Draw Shadow
    tools::Long nOff = vcl::text::TextLayoutEngine::GetShadowOffset(
        mpFontRealization->mxFont->mnLineHeight,
        mpGraphicsState->maFont.IsOutline()
    );

    rSalLayout.DrawBase() += basegfx::B2DPoint(nOff, nOff);
    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    rSalLayout.DrawBase() = aOrigBase;
    SetTextColor(aOldColor);
    SetTextLineColor(aOldTextLineColor);
    SetOverlineColor(aOldOverlineColor);
    ImplInitTextColor();

    // Draw Main Text
    // Only draw main text if we are NOT doing an outline (Outline handles its own body)
    if (!mpGraphicsState->maFont.IsOutline())
        ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);
}

void OutputDevice::ImplDrawOutlineText(SalLayout& rSalLayout)
{
    Color aOldColor = GetTextColor();
    Color aOldTextLineColor = GetTextLineColor();
    Color aOldOverlineColor = GetOverlineColor();
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();

    // Draw Outline (8 surrounding copies)
    // Uses current color (aOldColor)
    for (const auto& rOffset : vcl::text::TextLayoutEngine::GetOutlineOffsets())
    {
        rSalLayout.DrawBase() = aOrigBase + rOffset;
        ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);
    }

    // Draw Hollow Center (White)
    rSalLayout.DrawBase() = aOrigBase;

    SetTextColor(COL_WHITE);
    SetTextLineColor(COL_WHITE);
    SetOverlineColor(COL_WHITE);
    ImplInitTextColor();

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    SetTextColor(aOldColor);
    SetTextLineColor(aOldTextLineColor);
    SetOverlineColor(aOldOverlineColor);
    ImplInitTextColor();
}

void OutputDevice::ImplDrawText(SalLayout& rSalLayout)
{
    if (mpClippingController->IsDirty())
        InitClipRegion();

    if (IsOutputCulled())
        return;

    if (mbInitTextColor)
        ImplInitTextColor();

    rSalLayout.DrawBase()
        += basegfx::B2DPoint(mpFontRealization->nXOffset, mpFontRealization->nYOffset);

    if (IsTextFillColor())
        ImplDrawTextBackground(rSalLayout);

    if (mpFontRealization->bHasSpecialEffects)
        ImplDrawSpecialText(rSalLayout);
    else
        ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);
}

const Color& OutputDevice::GetTextColor() const { return mpGraphicsState->maTextColor; }

void OutputDevice::SetTextColor(const Color& rColor)
{
    Color aColor(
        vcl::drawmode::GetTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextColorAction(aColor));

    if (mpGraphicsState->maTextColor != aColor)
    {
        mpGraphicsState->maTextColor = aColor;
        mbInitTextColor = true;
    }
}

bool OutputDevice::IsTextFillColor() const { return !mpGraphicsState->maFont.IsTransparent(); }

void OutputDevice::SetTextFillColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextFillColorAction(Color(), false));

    if (mpGraphicsState->maFont.GetColor() != COL_TRANSPARENT)
    {
        mpGraphicsState->maFont.SetFillColor(COL_TRANSPARENT);
    }
    if (!mpGraphicsState->maFont.IsTransparent())
        mpGraphicsState->maFont.SetTransparent(true);
}

void OutputDevice::SetTextFillColor(const Color& rColor)
{
    Color aColor(
        vcl::drawmode::GetFillColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextFillColorAction(aColor, true));

    if (mpGraphicsState->maFont.GetFillColor() != aColor)
        mpGraphicsState->maFont.SetFillColor(aColor);
    if (mpGraphicsState->maFont.IsTransparent() != rColor.IsTransparent())
        mpGraphicsState->maFont.SetTransparent(rColor.IsTransparent());
}

Color OutputDevice::GetTextFillColor() const
{
    if (mpGraphicsState->maFont.IsTransparent())
        return COL_TRANSPARENT;
    else
        return mpGraphicsState->maFont.GetFillColor();
}

TextAlign OutputDevice::GetTextAlign() const { return mpGraphicsState->maFont.GetAlignment(); }

void OutputDevice::SetTextAlign(TextAlign eAlign)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextAlignAction(eAlign));

    if (mpGraphicsState->maFont.GetAlignment() != eAlign)
    {
        mpGraphicsState->maFont.SetAlignment(eAlign);
        mbNewFont = true;
    }
}

vcl::Region OutputDevice::GetOutputBoundsClipRegion() const { return GetClipRegion(); }

const SalLayoutFlags eDefaultLayout = SalLayoutFlags::NONE;

void OutputDevice::DrawText(const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                            sal_Int32 nLen, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, const SalLayoutGlyphs* pLayoutCache)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen > rStr.getLength()))
    {
        nLen = rStr.getLength() - nIndex;
    }

    if (mpOutDevData->mpRecordLayout)
    {
        pVector = &mpOutDevData->mpRecordLayout->m_aUnicodeBoundRects;
        pDisplayText = &mpOutDevData->mpRecordLayout->m_aDisplayText;
    }

#if OSL_DEBUG_LEVEL > 2
    SAL_INFO("vcl.gdi", "OutputDevice::DrawText(\"" << rStr << "\")");
#endif

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextAction(rStartPt, rStr, nIndex, nLen));
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

                if (aClip.Overlaps(*it))
                    bAppend = true;
                else if (rStr[nIndex] == ' ' && bInserted)
                {
                    std::vector<tools::Rectangle>::const_iterator next = it;
                    ++next;
                    if (next != aTmp.end() && aClip.Overlaps(*next))
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

    if (mpFontRealization->mxFont)
        // do not use cache with modified string
        if (mpFontRealization->mxFont->mpConversion)
            pLayoutCache = nullptr;

    std::unique_ptr<SalLayout> pSalLayout = LayoutText(
        vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ rStartPt, 0, {}, {}, eDefaultLayout },
        vcl::text::LayoutCacheData{ nullptr, pLayoutCache }, vcl::text::RenderSelection{});
    if (pSalLayout)
    {
        ImplDrawText(*pSalLayout);
    }
}

tools::Long OutputDevice::GetTextWidth(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                       vcl::text::TextLayoutCache const* const pLayoutCache,
                                       SalLayoutGlyphs const* const pSalLayoutCache) const
{
    double nWidth = GetTextWidthDouble(rStr, nIndex, nLen, pLayoutCache, pSalLayoutCache);
    return basegfx::fround<tools::Long>(nWidth);
}

double OutputDevice::GetTextWidthDouble(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                        vcl::text::TextLayoutCache const* const pLayoutCache,
                                        SalLayoutGlyphs const* const pSalLayoutCache) const
{
    return GetTextArray(rStr, nullptr, nIndex, nLen, false, pLayoutCache, pSalLayoutCache);
}

tools::Long OutputDevice::GetTextHeight() const
{
    if (!ImplNewFont())
        return 0;

    if (mpFontRealization && mpFontRealization->mxFont)
    {
        const double nPixelHeight
            = vcl::text::TextLayoutEngine::GetTextHeightPixel(*mpFontRealization);

        return DevicePixelToLogicHeight(static_cast<tools::Long>(nPixelHeight));
    }
    return 0;
}

double OutputDevice::GetTextHeightDouble() const
{
    if (!InitFont())
        return 0;

    const double nHeight = vcl::text::TextLayoutEngine::GetTextHeightPixel(*mpFontRealization);

    return mpMapper->DevicePixelToLogicHeightDouble(nHeight);
}

float OutputDevice::approximate_char_width() const
{
    //note pango uses "The quick brown fox jumps over the lazy dog." for english
    //and has a bunch of per-language strings which corresponds somewhat with
    //makeRepresentativeText in include/svtools/sampletext.hxx
    return GetTextWidth(u"aemnnxEM"_ustr) / 8.0;
}

float OutputDevice::approximate_digit_width() const
{
    return GetTextWidth(u"0123456789"_ustr) / 10.0;
}

void OutputDevice::DrawPartialTextArray(const Point& rStartPt, const OUString& rStr,
                                        KernArraySpan pDXArray,
                                        std::span<const sal_Bool> pKashidaArray, sal_Int32 nIndex,
                                        sal_Int32 nLen, sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                        SalLayoutFlags flags, const SalLayoutGlyphs* pLayoutCache)
{
    assert(!is_double_buffered_window());

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
    {
        nLen = rStr.getLength() - nIndex;
    }

    if (nPartLen < 0 || nPartIndex + nPartLen >= rStr.getLength())
    {
        nPartLen = rStr.getLength() - nPartIndex;
    }

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaTextArrayAction(rStartPt, rStr, pDXArray, pKashidaArray,
                                                      nPartIndex, nPartLen, nIndex, nLen));
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);
    if (mpClippingController->IsDirty())
        InitClipRegion();

    if (IsOutputCulled())
        return;

    // Adding the UnclusteredGlyphs flag during layout enables per-glyph styling.
    std::unique_ptr<SalLayout> pSalLayout
        = LayoutText(vcl::text::TextSpan{ rStr, nIndex, nLen },
                     vcl::text::LayoutConstraints{ rStartPt, 0, pDXArray, pKashidaArray,
                                                   flags | SalLayoutFlags::UnclusteredGlyphs },
                     vcl::text::LayoutCacheData{ nullptr, pLayoutCache },
                     vcl::text::RenderSelection{ nPartIndex, nPartIndex, nPartIndex + nPartLen });

    if (pSalLayout)
    {
        ImplDrawText(*pSalLayout);
    }
}

void OutputDevice::DrawTextArray(const Point& rStartPt, const OUString& rStr,
                                 KernArraySpan pDXArray, std::span<const sal_Bool> pKashidaArray,
                                 sal_Int32 nIndex, sal_Int32 nLen, SalLayoutFlags flags,
                                 const SalLayoutGlyphs* pSalLayoutCache)
{
    assert(!is_double_buffered_window());

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
    {
        nLen = rStr.getLength() - nIndex;
    }
    if (mpMetaFile)
        mpMetaFile->AddAction(
            new MetaTextArrayAction(rStartPt, rStr, pDXArray, pKashidaArray, nIndex, nLen));

    if (!IsDeviceOutputNecessary())
        return;
    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);
    if (mpClippingController->IsDirty())
        InitClipRegion();
    if (IsOutputCulled())
        return;

    std::unique_ptr<SalLayout> pSalLayout = LayoutText(
        vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ rStartPt, 0, pDXArray, pKashidaArray, flags },
        vcl::text::LayoutCacheData{ nullptr, pSalLayoutCache }, vcl::text::RenderSelection{});
    if (pSalLayout)
    {
        ImplDrawText(*pSalLayout);
    }
}

double OutputDevice::GetTextArray(const OUString& rStr, KernArray* pKernArray, sal_Int32 nIndex,
                                  sal_Int32 nLen, bool bCaret,
                                  vcl::text::TextLayoutCache const* const pLayoutCache,
                                  SalLayoutGlyphs const* const pSalLayoutCache,
                                  std::optional<tools::Rectangle>* pBounds) const
{
    return GetPartialTextArray(rStr, pKernArray, nIndex, nLen, nIndex, nLen, bCaret, pLayoutCache,
                               pSalLayoutCache, pBounds);
}

double OutputDevice::GetPartialTextArray(const OUString& rStr, KernArray* pKernArray,
                                         sal_Int32 nIndex, sal_Int32 nLen, sal_Int32 nPartIndex,
                                         sal_Int32 nPartLen, bool bCaret,
                                         const vcl::text::TextLayoutCache* pLayoutCache,
                                         const SalLayoutGlyphs* pSalLayoutCache,
                                         std::optional<tools::Rectangle>* pBounds) const
{
    if (!InitFont())
    {
        vcl::text::TextLayoutEngine::ZeroFillKernArray(pKernArray,
                                                       nPartLen); // Use the part length requested
        return 0.0;
    }

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::GetPartialTextArray(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, pKernArray, nPartIndex, nPartLen,
        bCaret, vcl::text::LayoutCacheData{ pLayoutCache, pSalLayoutCache }, pBounds);
}

void OutputDevice::GetCaretPositions(const OUString& rStr, KernArray& rCaretPos, sal_Int32 nIndex,
                                     sal_Int32 nLen, const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont())
        return;

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    vcl::text::TextLayoutEngine::GetCaretPositions(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, rCaretPos,
        vcl::text::LayoutCacheData{ nullptr, pGlyphs });
}

void OutputDevice::DrawStretchText(const Point& rStartPt, sal_Int32 nWidth, const OUString& rStr,
                                   sal_Int32 nIndex, sal_Int32 nLen)
{
    assert(!is_double_buffered_window());

    if ((nLen < 0) || (nIndex + nLen >= rStr.getLength()))
    {
        nLen = rStr.getLength() - nIndex;
    }

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaStretchTextAction(rStartPt, nWidth, rStr, nIndex, nLen));

    if (!IsDeviceOutputNecessary())
        return;

    std::unique_ptr<SalLayout> pSalLayout
        = LayoutText(vcl::text::TextSpan{ rStr, nIndex, nLen },
                     vcl::text::LayoutConstraints{ rStartPt, static_cast<tools::Long>(nWidth) },
                     vcl::text::LayoutCacheData{}, vcl::text::RenderSelection{});

    if (pSalLayout)
        ImplDrawText(*pSalLayout);
}

SalLayoutFlags OutputDevice::GetBiDiLayoutFlags(std::u16string_view rStr, const sal_Int32 nMinIndex,
                                                const sal_Int32 nEndIndex) const
{
    return vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(mpFontRealization->eLayoutMode, rStr,
                                                           nMinIndex, nEndIndex);
}

void OutputDevice::StartTrackingFontMappingUse() { vcl::text::TextLayoutEngine::StartTracking(); }

OutputDevice::FontMappingUseData OutputDevice::FinishTrackingFontMappingUse()
{
    return vcl::text::TextLayoutEngine::FinishTracking();
}

std::unique_ptr<SalLayout> OutputDevice::LayoutText(
    const vcl::text::TextSpan& rSpan, const vcl::text::LayoutConstraints& rConstraints,
    const vcl::text::LayoutCacheData& rCache, const vcl::text::RenderSelection& rSelection) const
{
    if (!InitFont())
        return nullptr;

    bool bUseSubpixel
        = IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning();

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            bUseSubpixel,
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::Layout(aResources, rSpan, rConstraints, rCache, rSelection);
}

std::shared_ptr<const vcl::text::TextLayoutCache>
OutputDevice::CreateTextLayoutCache(OUString const& rString)
{
    return vcl::text::TextLayoutCache::Create(rString);
}

bool OutputDevice::GetTextIsRTL(const OUString& rString, sal_Int32 nIndex, sal_Int32 nLen) const
{
    if (!InitFont())
        return false;

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::GetTextIsRTL(aResources, rString, nIndex, nLen);
}

sal_Int32 OutputDevice::GetTextBreak(const OUString& rStr, tools::Long nTextWidth, sal_Int32 nIndex,
                                     sal_Int32 nLen, tools::Long nCharExtra,
                                     const vcl::text::TextLayoutCache* pLayoutCache,
                                     const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont())
        return -1;

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::GetTextBreak(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, nTextWidth, nCharExtra,
        vcl::text::LayoutCacheData{ pLayoutCache, pGlyphs });
}

sal_Int32 OutputDevice::GetTextBreakArray(const OUString& rStr, tools::Long nTextWidth,
                                          std::optional<sal_Unicode> nHyphenChar,
                                          std::optional<sal_Int32*> pHyphenPos, sal_Int32 nIndex,
                                          sal_Int32 nLen, tools::Long nCharExtra,
                                          KernArraySpan aKernArray,
                                          vcl::text::TextLayoutCache const* const pLayoutCache,
                                          const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont())
        return -1;

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::GetTextBreakArray(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, nTextWidth, nHyphenChar, pHyphenPos,
        nCharExtra, aKernArray, vcl::text::LayoutCacheData{ pLayoutCache, pGlyphs });
}

void OutputDevice::ImplDrawText(OutputDevice& rTargetDevice, const tools::Rectangle& rRect,
                                const OUString& rOrigStr, DrawTextFlags nStyle,
                                std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                                vcl::TextLayoutCommon& _rLayout)
{
    Color aOldTextColor;
    Color aOldTextFillColor;
    bool bRestoreFillColor = false;
    if ((nStyle & DrawTextFlags::Disable) && !pVector)
    {
        bool bHighContrastBlack = false;
        bool bHighContrastWhite = false;
        const StyleSettings& rStyleSettings(rTargetDevice.GetSettings().GetStyleSettings());
        if (rStyleSettings.GetHighContrastMode())
        {
            Color aCol;
            if (rTargetDevice.IsBackground())
                aCol = rTargetDevice.GetBackground().GetColor();
            else
                // best guess is the face color here
                // but it may be totally wrong. the background color
                // was typically already reset
                aCol = rStyleSettings.GetFaceColor();

            bHighContrastBlack = aCol.IsDark();
            bHighContrastWhite = aCol.IsBright();
        }

        aOldTextColor = rTargetDevice.GetTextColor();
        if (rTargetDevice.IsTextFillColor())
        {
            bRestoreFillColor = true;
            aOldTextFillColor = rTargetDevice.GetTextFillColor();
        }
        if (bHighContrastBlack)
            rTargetDevice.SetTextColor(COL_GREEN);
        else if (bHighContrastWhite)
            rTargetDevice.SetTextColor(COL_LIGHTGREEN);
        else
        {
            // draw disabled text always without shadow
            // as it fits better with native look
            rTargetDevice.SetTextColor(
                rTargetDevice.GetSettings().GetStyleSettings().GetDisableColor());
        }
    }

    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();

    if (nWidth <= 0 || nHeight <= 0)
    {
        if (nStyle & DrawTextFlags::Clip)
            return;
        static bool bFuzzing = comphelper::IsFuzzing();
        SAL_WARN_IF(bFuzzing, "vcl",
                    "skipping negative rectangle of: " << nWidth << " x " << nHeight);
        if (bFuzzing)
            return;
    }

    Point aPos = rRect.TopLeft();

    tools::Long nTextHeight = rTargetDevice.GetTextHeight();
    TextAlign eAlign = rTargetDevice.GetTextAlign();
    sal_Int32 nMnemonicPos = -1;

    OUString aStr = rOrigStr;
    if (nStyle & DrawTextFlags::Mnemonic)
        aStr = removeMnemonicFromString(aStr, nMnemonicPos);

    const bool bDrawMnemonics = !(rTargetDevice.GetSettings().GetStyleSettings().GetOptions()
                                  & StyleSettingsOptions::NoMnemonics)
                                && !pVector;

    // We treat multiline text differently
    if (nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        sal_Int32 i;
        sal_Int32 nFormatLines;

        if (nTextHeight)
        {
            tools::Long nMaxTextWidth
                = _rLayout.GetTextLines(rRect, nTextHeight, aMultiLineInfo, nWidth, aStr, nStyle);
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

                    ImplTextLineInfo& rLineInfo = aMultiLineInfo.GetLine(nFormatLines);
                    aLastLine = convertLineEnd(aStr.copy(rLineInfo.GetIndex()), LINEEND_LF);
                    // Replace all LineFeeds with Spaces
                    OUStringBuffer aLastLineBuffer(aLastLine);
                    sal_Int32 nLastLineLen = aLastLineBuffer.getLength();
                    for (i = 0; i < nLastLineLen; i++)
                    {
                        if (aLastLineBuffer[i] == '\n')
                            aLastLineBuffer[i] = ' ';
                    }
                    aLastLine = aLastLineBuffer.makeStringAndClear();
                    aLastLine = _rLayout.GetEllipsisString(aLastLine, nWidth, nStyle);
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
                rTargetDevice.Push(vcl::PushFlags::CLIPREGION);
                rTargetDevice.IntersectClipRegion(rRect);
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
                aPos.AdjustY(rTargetDevice.GetFontMetric().GetAscent());

            // Output all lines except for the last one
            for (i = 0; i < nFormatLines; i++)
            {
                ImplTextLineInfo& rLineInfo = aMultiLineInfo.GetLine(i);
                if (nStyle & DrawTextFlags::Right)
                    aPos.AdjustX(nWidth - rLineInfo.GetWidth());
                else if (nStyle & DrawTextFlags::Center)
                    aPos.AdjustX((nWidth - rLineInfo.GetWidth()) / 2);
                sal_Int32 nIndex = rLineInfo.GetIndex();
                sal_Int32 nLineLen = rLineInfo.GetLen();
                _rLayout.DrawText(aPos, aStr, nIndex, nLineLen, pVector, pDisplayText);
                if (bDrawMnemonics)
                {
                    if ((nMnemonicPos >= nIndex) && (nMnemonicPos < nIndex + nLineLen))
                    {
                        tools::Long nMnemonicX;
                        tools::Long nMnemonicY;

                        KernArray aDXArray;
                        _rLayout.GetTextArray(aStr, &aDXArray, nIndex, nLineLen, true);
                        sal_Int32 nPos = nMnemonicPos - nIndex;
                        sal_Int32 lc_x1 = nPos ? aDXArray[nPos - 1] : 0;
                        sal_Int32 lc_x2 = aDXArray[nPos];
                        double nMnemonicWidth
                            = rTargetDevice.LogicWidthToDeviceSubPixel(std::abs(lc_x1 - lc_x2));

                        Point aTempPos = rTargetDevice.LogicToPixel(aPos);
                        nMnemonicX
                            = rTargetDevice.GetOutOffXPixel() + aTempPos.X()
                              + rTargetDevice.LogicWidthToDevicePixel(std::min(lc_x1, lc_x2));
                        nMnemonicY = rTargetDevice.GetOutOffYPixel() + aTempPos.Y()
                                     + rTargetDevice.LogicWidthToDevicePixel(
                                           rTargetDevice.GetFontMetric().GetAscent());
                        rTargetDevice.ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
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
                rTargetDevice.Pop();
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
                aStr = _rLayout.GetEllipsisString(aStr, nWidth, nStyle);
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
            aPos.AdjustY(rTargetDevice.GetFontMetric().GetAscent());

        if (nStyle & DrawTextFlags::Bottom)
            aPos.AdjustY(nHeight - nTextHeight);
        else if (nStyle & DrawTextFlags::VCenter)
            aPos.AdjustY((nHeight - nTextHeight) / 2);

        tools::Long nMnemonicX = 0;
        tools::Long nMnemonicY = 0;
        double nMnemonicWidth = 0;
        if (nMnemonicPos != -1 && nMnemonicPos < aStr.getLength())
        {
            KernArray aDXArray;
            _rLayout.GetTextArray(aStr, &aDXArray, 0, aStr.getLength(), true);
            tools::Long lc_x1 = nMnemonicPos ? aDXArray[nMnemonicPos - 1] : 0;
            tools::Long lc_x2 = aDXArray[nMnemonicPos];
            nMnemonicWidth = rTargetDevice.LogicWidthToDeviceSubPixel(std::abs(lc_x1 - lc_x2));

            Point aTempPos = rTargetDevice.LogicToPixel(aPos);
            nMnemonicX = rTargetDevice.GetOutOffXPixel() + aTempPos.X()
                         + rTargetDevice.LogicWidthToDevicePixel(std::min(lc_x1, lc_x2));
            nMnemonicY = rTargetDevice.GetOutOffYPixel() + aTempPos.Y()
                         + rTargetDevice.LogicWidthToDevicePixel(
                               rTargetDevice.GetFontMetric().GetAscent());
        }

        if (nStyle & DrawTextFlags::Clip)
        {
            auto popIt = rTargetDevice.ScopedPush(vcl::PushFlags::CLIPREGION);
            rTargetDevice.IntersectClipRegion(rRect);
            _rLayout.DrawText(aPos, aStr, 0, aStr.getLength(), pVector, pDisplayText);
            if (bDrawMnemonics && nMnemonicPos != -1)
                rTargetDevice.ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
        }
        else
        {
            _rLayout.DrawText(aPos, aStr, 0, aStr.getLength(), pVector, pDisplayText);
            if (bDrawMnemonics && nMnemonicPos != -1)
                rTargetDevice.ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);
        }
    }

    if (nStyle & DrawTextFlags::Disable && !pVector)
    {
        rTargetDevice.SetTextColor(aOldTextColor);
        if (bRestoreFillColor)
            rTargetDevice.SetTextFillColor(aOldTextFillColor);
    }
}

void OutputDevice::AddTextRectActions(const tools::Rectangle& rRect, const OUString& rOrigStr,
                                      DrawTextFlags nStyle, GDIMetaFile& rMtf)
{
    if (rOrigStr.isEmpty() || rRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);
    if (mpClippingController->IsDirty())
        InitClipRegion();

    // temporarily swap in passed mtf for action generation, and
    // disable output generation.
    const bool bOutputEnabled(IsOutputEnabled());
    GDIMetaFile* pMtf = mpMetaFile;

    mpMetaFile = &rMtf;
    EnableOutput(false);

    // #i47157# Factored out to ImplDrawTextRect(), to be shared
    // between us and DrawText()
    vcl::DefaultTextLayout aLayout(*this);
    ImplDrawText(*this, rRect, rOrigStr, nStyle, nullptr, nullptr, aLayout);

    // and restore again
    EnableOutput(bOutputEnabled);
    mpMetaFile = pMtf;
}

void OutputDevice::DrawText(const tools::Rectangle& rRect, const OUString& rOrigStr,
                            DrawTextFlags nStyle, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, vcl::TextLayoutCommon* _pTextLayout)
{
    assert(!is_double_buffered_window());

    if (mpOutDevData->mpRecordLayout)
    {
        pVector = &mpOutDevData->mpRecordLayout->m_aUnicodeBoundRects;
        pDisplayText = &mpOutDevData->mpRecordLayout->m_aDisplayText;
    }

    bool bDecomposeTextRectAction
        = (_pTextLayout != nullptr) && _pTextLayout->DecomposeTextRectAction();
    if (mpMetaFile && !bDecomposeTextRectAction)
        mpMetaFile->AddAction(new MetaTextRectAction(rRect, rOrigStr, nStyle));

    if ((!IsDeviceOutputNecessary() && !pVector && !bDecomposeTextRectAction) || rOrigStr.isEmpty()
        || rRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);
    if (mpClippingController->IsDirty())
        InitClipRegion();
    if (IsOutputCulled() && !bDecomposeTextRectAction && !pDisplayText)
        return;

    // temporarily disable mtf action generation (ImplDrawText _does_
    // create MetaActionType::TEXTs otherwise)
    GDIMetaFile* pMtf = mpMetaFile;
    if (!bDecomposeTextRectAction)
        mpMetaFile = nullptr;

    // #i47157# Factored out to ImplDrawText(), to be used also
    // from AddTextRectActions()
    vcl::DefaultTextLayout aDefaultLayout(*this);
    ImplDrawText(*this, rRect, rOrigStr, nStyle, pVector, pDisplayText,
                 _pTextLayout ? *_pTextLayout : aDefaultLayout);

    // and enable again
    mpMetaFile = pMtf;
}

tools::Rectangle OutputDevice::GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                           DrawTextFlags nStyle, TextRectInfo* pInfo,
                                           const vcl::TextLayoutCommon* _pTextLayout) const
{
    tools::Rectangle aRect = rRect;
    sal_Int32 nLines;
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nMaxWidth;
    tools::Long nTextHeight = GetTextHeight();

    OUString aStr = rStr;
    if (nStyle & DrawTextFlags::Mnemonic)
        aStr = removeMnemonicFromString(aStr);

    if (nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        sal_Int32 nFormatLines;
        sal_Int32 i;

        nMaxWidth = 0;
        vcl::DefaultTextLayout aDefaultLayout(*const_cast<OutputDevice*>(this));

        if (_pTextLayout)
            const_cast<vcl::TextLayoutCommon*>(_pTextLayout)
                ->GetTextLines(rRect, nTextHeight, aMultiLineInfo, nWidth, aStr, nStyle);
        else
            aDefaultLayout.GetTextLines(rRect, nTextHeight, aMultiLineInfo, nWidth, aStr, nStyle);

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
                ImplTextLineInfo& rLineInfo = aMultiLineInfo.GetLine(i);
                if (bMaxWidth && (rLineInfo.GetWidth() > nMaxWidth))
                    nMaxWidth = rLineInfo.GetWidth();
                if (rLineInfo.GetWidth() > pInfo->mnMaxWidth)
                    pInfo->mnMaxWidth = rLineInfo.GetWidth();
            }
        }
        else if (!nMaxWidth)
        {
            for (i = 0; i < nLines; i++)
            {
                ImplTextLineInfo& rLineInfo = aMultiLineInfo.GetLine(i);
                if (rLineInfo.GetWidth() > nMaxWidth)
                    nMaxWidth = rLineInfo.GetWidth();
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

    if (mpGraphicsState->maFont.GetOrientation() != 0_deg10)
    {
        tools::Polygon aRotatedPolygon(aRect);
        aRotatedPolygon.Rotate(Point(aRect.GetWidth() / 2, aRect.GetHeight() / 2),
                               mpGraphicsState->maFont.GetOrientation());
        return aRotatedPolygon.GetBoundRect();
    }

    return aRect;
}

void OutputDevice::DrawCtrlText(const Point& rPos, const OUString& rStr, const sal_Int32 nIndex,
                                const sal_Int32 nLen, DrawTextFlags nStyle,
                                std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                                const SalLayoutGlyphs* pGlyphs)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary() || (nIndex >= rStr.getLength()))
        return;

    // better get graphics here because ImplDrawMnemonicLine() will not
    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);
    if (mpClippingController->IsDirty())
        InitClipRegion();
    if (IsOutputCulled())
        return;

    // nIndex and nLen must go to mpAlphaVDev->DrawCtrlText unchanged
    sal_Int32 nCorrectedIndex = nIndex;
    sal_Int32 nCorrectedLen = nLen;
    if ((nCorrectedLen < 0) || (nCorrectedIndex + nCorrectedLen >= rStr.getLength()))
    {
        nCorrectedLen = rStr.getLength() - nCorrectedIndex;
    }
    sal_Int32 nMnemonicPos = -1;

    tools::Long nMnemonicX = 0;
    tools::Long nMnemonicY = 0;
    tools::Long nMnemonicWidth = 0;
    const OUString aStr = removeMnemonicFromString(rStr, nMnemonicPos); // Strip mnemonics always
    if (nMnemonicPos != -1)
    {
        if (nMnemonicPos < nCorrectedIndex)
        {
            --nCorrectedIndex;
        }
        else
        {
            if (nMnemonicPos < (nCorrectedIndex + nCorrectedLen))
                --nCorrectedLen;
        }
        if (nStyle & DrawTextFlags::Mnemonic && !pVector
            && !(GetSettings().GetStyleSettings().GetOptions() & StyleSettingsOptions::NoMnemonics))
        {
            SAL_WARN_IF(nMnemonicPos >= (nCorrectedIndex + nCorrectedLen), "vcl",
                        "Mnemonic underline marker after last character");
            bool bInvalidPos = false;

            if (nMnemonicPos >= nCorrectedLen)
            {
                // may occur in BiDi-Strings: the '~' is sometimes found behind the last char
                // due to some strange BiDi text editors
                // -> place the underline behind the string to indicate a failure
                bInvalidPos = true;
                nMnemonicPos = nCorrectedLen - 1;
            }

            KernArray aDXArray;
            GetTextArray(aStr, &aDXArray, nCorrectedIndex, nCorrectedLen, true, nullptr, pGlyphs);
            sal_Int32 nPos = nMnemonicPos - nCorrectedIndex;
            sal_Int32 lc_x1 = nPos ? aDXArray[nPos - 1] : 0;
            sal_Int32 lc_x2 = aDXArray[nPos];
            nMnemonicWidth = std::abs(lc_x1 - lc_x2);

            Point aTempPos(std::min(lc_x1, lc_x2), GetFontMetric().GetAscent());
            if (bInvalidPos) // #106952#, place behind the (last) character
                aTempPos = Point(std::max(lc_x1, lc_x2), GetFontMetric().GetAscent());

            aTempPos += rPos;
            aTempPos = LogicToPixel(aTempPos);
            nMnemonicX = GetOutOffXPixel() + aTempPos.X();
            nMnemonicY = GetOutOffYPixel() + aTempPos.Y();
        }
        else
            nMnemonicPos = -1; // Reset - we don't show the mnemonic
    }

    std::optional<Color> oOldTextColor;
    std::optional<Color> oOldTextFillColor;
    if (nStyle & DrawTextFlags::Disable && !pVector)
    {
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

        oOldTextColor = GetTextColor();
        if (IsTextFillColor())
            oOldTextFillColor = GetTextFillColor();

        if (bHighContrastBlack)
            SetTextColor(COL_GREEN);
        else if (bHighContrastWhite)
            SetTextColor(COL_LIGHTGREEN);
        else
            SetTextColor(GetSettings().GetStyleSettings().GetDisableColor());
    }

    DrawText(rPos, aStr, nCorrectedIndex, nCorrectedLen, pVector, pDisplayText, pGlyphs);
    if (nMnemonicPos != -1)
        ImplDrawMnemonicLine(nMnemonicX, nMnemonicY, nMnemonicWidth);

    if (oOldTextColor)
        SetTextColor(*oOldTextColor);
    if (oOldTextFillColor)
        SetTextFillColor(*oOldTextFillColor);
}

tools::Long OutputDevice::GetCtrlTextWidth(const OUString& rStr,
                                           const SalLayoutGlyphs* pGlyphs) const
{
    sal_Int32 nLen = rStr.getLength();
    sal_Int32 nIndex = 0;

    sal_Int32 nMnemonicPos;
    OUString aStr = removeMnemonicFromString(rStr, nMnemonicPos);
    if (nMnemonicPos != -1)
    {
        if (nMnemonicPos < nIndex)
            nIndex--;
        else if (static_cast<sal_uLong>(nMnemonicPos) < static_cast<sal_uLong>(nIndex + nLen))
            nLen--;
    }
    return GetTextWidth(aStr, nIndex, nLen, nullptr, pGlyphs);
}

bool OutputDevice::GetLogicalTextBoundRect(tools::Rectangle& rRect, const OUString& rStr,
                                           sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                           sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                           std::span<const sal_Bool> pKashidaArray,
                                           const SalLayoutGlyphs* pGlyphs) const
{
    basegfx::B2DRectangle aRect;
    bool bRet = GetLogicalTextBoundRect(aRect, rStr, nBase, nIndex, nLen,
                                        static_cast<tools::Long>(nLayoutWidth), pDXArray,
                                        pKashidaArray, pGlyphs);
    rRect = SalLayout::BoundRect2Rectangle(aRect);
    return bRet;
}

bool OutputDevice::GetLogicalTextBoundRect(basegfx::B2DRectangle& rRect, const OUString& rStr,
                                           sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                           sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                           std::span<const sal_Bool> pKashidaArray,
                                           const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont())
        return false;

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(),
            *mpMapper,
            &GetFontCache(),
            GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState,
            *mpFontRealization };

    return vcl::text::TextLayoutEngine::GetLogicalTextBoundRect(aResources, rRect, rStr, nBase,
                                                                nIndex, nLen, nLayoutWidth,
                                                                pDXArray, pKashidaArray, pGlyphs);
}

bool OutputDevice::GetTextOutlines(basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                                   sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                   sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                   std::span<const sal_Bool> pKashidaArray) const
{
    if (!InitFont())
        return false;

    bool bRet = false;
    rVector.clear();
    if (nLen < 0)
    {
        nLen = rStr.getLength() - nIndex;
    }
    rVector.reserve(nLen);

    // we want to get the Rectangle in logical units, so to
    // avoid rounding errors we just size the font in logical units
    bool bOldMap = mpMapper->IsMapModeEnabled();
    if (bOldMap)
    {
        mpMapper->EnableMapMode(false);
        const_cast<OutputDevice&>(*this).mbNewFont = true;
    }

    std::unique_ptr<SalLayout> pSalLayout;

    // calculate offset when nBase!=nIndex
    double nXOffset = 0;
    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;

        pSalLayout = LayoutText(
            vcl::text::TextSpan{ rStr, nStart, nOfsLen },
            vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                          pDXArray, pKashidaArray, eDefaultLayout },
            vcl::text::LayoutCacheData{ nullptr, nullptr }, vcl::text::RenderSelection{});

        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            pSalLayout.reset();
            // TODO: fix offset calculation for Bidi case
            if (nBase > nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = LayoutText(
        vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth), pDXArray,
                                      pKashidaArray, eDefaultLayout },
        vcl::text::LayoutCacheData{ nullptr, nullptr }, // No cache, no glyphs available here
        vcl::text::RenderSelection{});

    if (pSalLayout)
    {
        bRet = pSalLayout->GetOutline(rVector);
        if (bRet)
        {
            basegfx::B2DHomMatrix aMatrix = vcl::text::TextLayoutEngine::CalculateOutlineTransform(
                *pSalLayout, *mpFontRealization, nXOffset);

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
        mpMapper->EnableMapMode(bOldMap);
        const_cast<OutputDevice&>(*this).mbNewFont = true;
    }

    return bRet;
}

bool OutputDevice::GetTextOutlines(PolyPolyVector& rResultVector, const OUString& rStr,
                                   sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                   sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                   std::span<const sal_Bool> pKashidaArray) const
{
    rResultVector.clear();

    // get the basegfx polypolygon vector
    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;
    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, nBase, nIndex, nLen, nLayoutWidth, pDXArray,
                         pKashidaArray))
    {
        return false;
    }

    // convert to a tool polypolygon vector
    rResultVector.reserve(aB2DPolyPolyVector.size());
    for (auto const& elem : aB2DPolyPolyVector)
    {
        rResultVector.emplace_back(elem); // #i76339#
    }

    return true;
}

bool OutputDevice::GetTextOutline(tools::PolyPolygon& rPolyPoly, const OUString& rStr) const
{
    rPolyPoly.Clear();

    // get the basegfx polypolygon vector
    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;
    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, 0 /*nBase*/, 0 /*nIndex*/, /*nLen*/ -1,
                         /*nLayoutWidth*/ 0, /*pDXArray*/ {}))
    {
        return false;
    }

    // convert and merge into a tool polypolygon
    for (auto const& elem : aB2DPolyPolyVector)
    {
        for (auto const& rB2DPolygon : elem)
        {
            rPolyPoly.Insert(tools::Polygon(rB2DPolygon)); // #i76339#
        }
    }

    return true;
}

void OutputDevice::SetSystemTextColor(SystemTextColorFlags nFlags, bool bEnabled)
{
    if (nFlags & SystemTextColorFlags::Mono)
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
    }
}

std::unique_ptr<SalLayout>
OutputDevice::getFallbackLayout(LogicalFontInstance* pLogicalFont, int nFallbackLevel,
                                vcl::text::TextLayoutRequest& rLayoutArgs,
                                const SalLayoutGlyphs* pGlyphs) const
{
    if (!mpGraphics && !AcquireGraphics())
        return nullptr;

    assert(mpGraphics != nullptr);
    mpGraphics->SetFont(pLogicalFont, nFallbackLevel);

    rLayoutArgs.ResetPos();
    std::unique_ptr<GenericSalLayout> pFallback = mpGraphics->GetTextLayout(nFallbackLevel);

    if (!pFallback)
        return nullptr;

    if (!pFallback->LayoutText(rLayoutArgs, pGlyphs ? pGlyphs->Impl(nFallbackLevel) : nullptr))
        return nullptr;

    return pFallback;
}

tools::Rectangle OutputDevice::GetTextInkBounds(const SalLayout& rLayout) const
{
    return vcl::text::TextLayoutEngine::GetTextInkBounds(rLayout, *mpFontRealization);
}

void OutputDevice::GetWordKashidaPositions(const OUString& rText, std::vector<bool>* pOutMap) const
{
    if (!pOutMap)
        return;

    auto nEnd = rText.getLength();
    std::unique_ptr<SalLayout> pSalLayout
        = LayoutText(vcl::text::TextSpan{ rText, 0, nEnd },
                     vcl::text::LayoutConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE },
                     vcl::text::LayoutCacheData{}, vcl::text::RenderSelection{});

    if (!pSalLayout)
    {
        pOutMap->clear();
        return;
    }

    vcl::text::TextLayoutEngine::GetWordKashidaPositions(*pSalLayout, rText, *pOutMap);
}

bool OutputDevice::GetGlyphBoundRects(const Point& rOrigin, const OUString& rStr, int nIndex,
                                      int nLen, std::vector<tools::Rectangle>& rVector) const
{
    rVector.clear();
    if (nIndex >= rStr.getLength())
        return false;
    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
        nLen = rStr.getLength() - nIndex;

    tools::Rectangle aRect;
    for (int i = 0; i < nLen; i++)
    {
        if (!GetLogicalTextBoundRect(aRect, rStr, nIndex, nIndex + i, 1))
            break;
        aRect.Move(rOrigin.X(), rOrigin.Y());
        rVector.push_back(aRect);
    }
    return (nLen == static_cast<int>(rVector.size()));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
