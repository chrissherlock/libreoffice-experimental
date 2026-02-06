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
#include <tools/debug.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/text/TextLayoutData.hxx>
#include <vcl/text/TextRecordingDispatcher.hxx>
#include <vcl/textrectinfo.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <text/AccessibilityRecorder.hxx>
#include <text/MeasurementRecorder.hxx>
#include <font/FontController.hxx>
#include <text/GraphicLayoutFactory.hxx>
#include <drawmode.hxx>
#include <textlayout.hxx>
#include <TextLayoutCache.hxx>

#include <memory>
#include <optional>

vcl::text::ComplexTextLayoutFlags OutputDevice::GetLayoutMode() const
{
    return mpFontRealization ? mpFontRealization->eLayoutMode : mpGraphicsState->mnTextLayoutMode;
}

void OutputDevice::SetLayoutMode(vcl::text::ComplexTextLayoutFlags nTextLayoutMode)
{
    maRecorder.RecordLayoutMode(nTextLayoutMode);

    mpGraphicsState->mnTextLayoutMode = nTextLayoutMode;
    if (mpFontRealization)
        mpFontRealization->eLayoutMode = nTextLayoutMode;
}

LanguageType OutputDevice::GetDigitLanguage() const { return mpGraphicsState->meTextLanguage; }

void OutputDevice::SetDigitLanguage(LanguageType eTextLanguage)
{
    maRecorder.RecordTextLanguage(eTextLanguage);

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
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();
    basegfx::B2DPoint aOrigOffset = rSalLayout.DrawOffset();
    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        rSalLayout.DrawOffset() = aOrigOffset;
    });

    tools::Long nX = aOrigBase.getX();
    tools::Long nY = aOrigBase.getY();

    tools::Rectangle aBoundRect
        = vcl::text::TextLayoutEngine::GetTextInkBounds(rSalLayout, *mpFontRealization, false);

    Bitmap aBmp = ImplCreateRotatedTextBitmap(rSalLayout, aBoundRect);

    if (aBmp.IsEmpty())
        return false;

    Point aPoint = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(
        Point(nX, nY), aBoundRect, mpFontRealization->mxFont->mnOwnOrientation);

    ImplDrawRotatedTextMask(aPoint, aBmp);

    return true;
}

Bitmap OutputDevice::ImplCreateRotatedTextBitmap(SalLayout& rSalLayout,
                                                 const tools::Rectangle& rBoundRect)
{
    VirtualDevice* pVDev = ImplPrepareRotateDevice(rBoundRect.GetSize());

    if (!pVDev)
        return Bitmap();

    // Adjust layout to draw into the buffer's upper-left corner
    rSalLayout.DrawBase() = basegfx::B2DPoint(-rBoundRect.Left(), -rBoundRect.Top());
    rSalLayout.DrawOffset() = basegfx::B2DPoint(0, 0);
    rSalLayout.DrawText(*pVDev->mpGraphics);

    // Extract the buffer and apply the rotation
    Bitmap aBmp = pVDev->GetBitmap(Point(), rBoundRect.GetSize());

    if (!aBmp.IsEmpty())
        aBmp.Rotate(mpFontRealization->mxFont->mnOwnOrientation, COL_WHITE);

    return aBmp;
}

VirtualDevice* OutputDevice::ImplPrepareRotateDevice(const Size& rSize)
{
    if (!mpRotateDev)
        mpRotateDev = VclPtr<VirtualDevice>::Create(*this);

    VirtualDevice* pVDev = mpRotateDev;
    if (!pVDev->SetOutputSizePixel(rSize))
        return nullptr;

    const vcl::font::FontSelectPattern& rPattern
        = mpFontRealization->mxFont->GetFontSelectPattern();
    vcl::Font aFont(GetFont());
    aFont.SetOrientation(0_deg10); // Draw horizontal first
    aFont.SetFontSize(Size(rPattern.mnWidth, rPattern.mnHeight));

    pVDev->SetFont(aFont);
    pVDev->SetTextColor(COL_BLACK);
    pVDev->SetTextFillColor();

    if (!pVDev->InitFont())
        return nullptr;

    pVDev->ImplInitTextColor();
    return pVDev;
}

void OutputDevice::ImplDrawRotatedTextMask(const Point& rPoint, const Bitmap& rBmp)
{
    tools::Long nOldOffX = GetOutOffXPixel();
    tools::Long nOldOffY = GetOutOffYPixel();
    bool bOldMap = mpMapper->IsMapModeEnabled();

    // Suspend recording for mask drawing
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    comphelper::ScopeGuard aRestoreGuard([&]() {
        mpMapper->EnableMapMode(bOldMap);
        SetDeviceOriginX(nOldOffX);
        SetDeviceOriginY(nOldOffY);
    });

    SetDeviceOriginX(0);
    SetDeviceOriginY(0);
    mpMapper->EnableMapMode(false);

    DrawMask(rPoint, rBmp, GetTextColor());
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

    comphelper::ScopeGuard aRestoreGuard([&]() {
        SetTextColor(aOldColor);
        SetTextLineColor(aOldTextLineColor);
        SetOverlineColor(aOldOverlineColor);
        rSalLayout.DrawOffset() = aOrigOffset;
        ImplInitTextColor();
    });

    Color aReliefColor(COL_LIGHTGRAY);
    Color aTextColor(aOldColor);

    // Black text is always drawn on white in VCL logic
    if (aTextColor == COL_BLACK)
        aTextColor = COL_WHITE;

    Color aEffectiveLineColor = (aOldTextLineColor == COL_BLACK) ? COL_WHITE : aOldTextLineColor;
    Color aEffectiveOverlineColor
        = (aOldOverlineColor == COL_BLACK) ? COL_WHITE : aOldOverlineColor;

    // Relief color is black for white text
    if (aTextColor == COL_WHITE)
        aReliefColor = COL_BLACK;

    // Draw Relief Shadow
    SetTextColor(aReliefColor);
    SetTextLineColor(aReliefColor);
    SetOverlineColor(aReliefColor);
    ImplInitTextColor();

    tools::Long nOff = vcl::text::TextLayoutEngine::GetReliefOffset(
        GetDPIX(), mpGraphicsState->maFont.GetRelief());
    rSalLayout.DrawOffset() += basegfx::B2DPoint(nOff, nOff);

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    // Draw Main Text
    rSalLayout.DrawOffset() = aOrigOffset; // Reset offset for main draw

    SetTextColor(aTextColor);
    SetTextLineColor(aEffectiveLineColor);
    SetOverlineColor(aEffectiveOverlineColor);
    ImplInitTextColor();

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);
}

void OutputDevice::ImplDrawShadowText(SalLayout& rSalLayout)
{
    Color aOldColor = GetTextColor();
    Color aOldTextLineColor = GetTextLineColor();
    Color aOldOverlineColor = GetOverlineColor();
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();

    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        SetTextColor(aOldColor);
        SetTextLineColor(aOldTextLineColor);
        SetOverlineColor(aOldOverlineColor);
        ImplInitTextColor();
    });

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
        mpFontRealization->mxFont->mnLineHeight, mpGraphicsState->maFont.IsOutline());

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

    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        SetTextColor(aOldColor);
        SetTextLineColor(aOldTextLineColor);
        SetOverlineColor(aOldOverlineColor);
        ImplInitTextColor();
    });

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

    maRecorder.RecordTextColor(aColor);

    if (mpGraphicsState->maTextColor != aColor)
    {
        mpGraphicsState->maTextColor = aColor;
        mbInitTextColor = true;
    }
}

bool OutputDevice::IsTextFillColor() const { return !mpGraphicsState->maFont.IsTransparent(); }

void OutputDevice::SetTextFillColor()
{
    maRecorder.RecordTextFillColor(Color(), false);

    if (mpGraphicsState->maFont.GetColor() != COL_TRANSPARENT)
        mpGraphicsState->maFont.SetFillColor(COL_TRANSPARENT);

    if (!mpGraphicsState->maFont.IsTransparent())
        mpGraphicsState->maFont.SetTransparent(true);
}

void OutputDevice::SetTextFillColor(const Color& rColor)
{
    Color aColor(
        vcl::drawmode::GetFillColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    maRecorder.RecordTextFillColor(aColor, true);

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
    maRecorder.RecordTextAlign(eAlign);

    if (mpGraphicsState->maFont.GetAlignment() != eAlign)
    {
        mpGraphicsState->maFont.SetAlignment(eAlign);
    }
}

vcl::Region OutputDevice::GetOutputBoundsClipRegion() const { return GetClipRegion(); }

const SalLayoutFlags eDefaultLayout = SalLayoutFlags::NONE;


bool OutputDevice::IsLayoutCalculationNecessary() const
{
    return moRecordingState && moRecordingState->IsActive();
}

void OutputDevice::DrawText(const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                            sal_Int32 nLen, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, const SalLayoutGlyphs* pLayoutCache)
{
    assert(!is_double_buffered_window());

    nLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nIndex, nLen);
    assert(nLen >= 0 && "DrawTextArray: Length must be non-negative after normalization");

    maRecorder.RecordDrawText(rStartPt, rStr, nIndex, nLen);

    vcl::text::TextRecordingState* pStateToUse = nullptr;
    std::optional<vcl::text::TextRecordingState> oTempState;

    if (moRecordingState)
        pStateToUse = &*moRecordingState;
    else if (pVector)
    {
        oTempState.emplace();
        pStateToUse = &*oTempState;
    }

    if (pStateToUse && pVector)
    {
        pStateToUse->mpMeasurementVector = pVector;
        pStateToUse->mpMeasurementString = pDisplayText;
        // Clip is transient, we calculate it here
        vcl::Region aClip(GetOutputBoundsClipRegion());
        pStateToUse->mpMeasurementClip = &aClip; // Note: risky if aClip dies, but Record() is immediate
    }

    // Note: For pVector (Measurement), we need a layout *now* to record it.
    // For normal drawing, we create layout later.
    // To solve this overlap, we create the layout once if possible, or create a temp one for pVector.
    if (pVector && pStateToUse)
    {
        vcl::Region aClip(GetOutputBoundsClipRegion());

        pStateToUse->mpMeasurementVector = pVector;
        pStateToUse->mpMeasurementString = pDisplayText;
        pStateToUse->mpMeasurementClip = &aClip;

        comphelper::ScopeGuard aCleanupGuard([&]() {
            pStateToUse->mpMeasurementVector = nullptr;
            pStateToUse->mpMeasurementString = nullptr;
            pStateToUse->mpMeasurementClip = nullptr;
        });

        std::unique_ptr<SalLayout> pSalLayout = LayoutText(
            vcl::text::TextSpan{rStr, nIndex, nLen},
            vcl::text::LayoutConstraints{rStartPt, 0, {}, {}, SalLayoutFlags::NONE},
            vcl::text::LayoutCacheData{nullptr, pLayoutCache},
            vcl::text::RenderSelection{}
        );

        if (pSalLayout)
            vcl::text::TextRecordingDispatcher::Dispatch(*pStateToUse, *this, rStr, nIndex, nLen, pSalLayout.get());
    }

    // If pVector is set, we handled it above (External Recording).
    // If mpRecordLayout is set, we proceed to LayoutText to get accurate bounds.
    if ((!IsDeviceOutputNecessary() && !IsLayoutCalculationNecessary()) || pVector)
        return;

    if (mpFontRealization->mxFont)
    {
        if (mpFontRealization->mxFont->mpConversion)
            pLayoutCache = nullptr;
    }

    std::unique_ptr<SalLayout> pSalLayout = LayoutText(
        vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ rStartPt, 0, {}, {}, eDefaultLayout },
        vcl::text::LayoutCacheData{ nullptr, pLayoutCache }, vcl::text::RenderSelection{});

    if (pSalLayout)
    {
        // Internal Recording: Use the actual layout to ensure accessibility bounds match visual bounds
        if (moRecordingState)
             vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, nIndex, nLen, pSalLayout.get());

        if ((!moRecordingState || !moRecordingState->IsActive()))
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
    if (!ImplUpdateFontInstance())
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

    nLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nIndex, nLen);

    nPartLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nPartIndex, nPartLen);

    maRecorder.RecordDrawPartialTextArray(rStartPt, rStr, pDXArray, pKashidaArray, nPartIndex, nPartLen, nIndex, nLen);

    if (!IsDeviceOutputNecessary() && !IsLayoutCalculationNecessary())
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
        if (moRecordingState)
            vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, nIndex, nLen, pSalLayout.get());

        if ((!moRecordingState || !moRecordingState->IsActive()))
            ImplDrawText(*pSalLayout);
    }
}

void OutputDevice::DrawTextArray(const Point& rStartPt, const OUString& rStr,
                                 KernArraySpan aKernArray, std::span<const sal_Bool> pKashidaAry,
                                 sal_Int32 nIndex, sal_Int32 nLen, SalLayoutFlags nFlags,
                                 const SalLayoutGlyphs* pLayoutCache)
{
    nLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nIndex, nLen);
    assert(!is_double_buffered_window());

    maRecorder.RecordDrawTextArray(rStartPt, rStr, aKernArray, pKashidaAry, nIndex, nLen);

    // Allow layout calculation to proceed if we are recording (to get accurate bounds)
    if (!IsDeviceOutputNecessary() && !IsLayoutCalculationNecessary())
        return;

    if (mpFontRealization->mxFont && mpFontRealization->mxFont->mpConversion)
        pLayoutCache = nullptr;

    auto pSalLayout = LayoutText(
        vcl::text::TextSpan{ rStr, nIndex, nLen },
        vcl::text::LayoutConstraints{ rStartPt, 0, aKernArray, pKashidaAry, nFlags },
        vcl::text::LayoutCacheData{ nullptr, pLayoutCache }, vcl::text::RenderSelection{});

    if (pSalLayout)
    {
        if (moRecordingState)
            vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, nIndex, nLen, pSalLayout.get(), true);

        ImplRenderTextLayout(*pSalLayout);
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

    nLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nIndex, nLen);

    maRecorder.RecordDrawStretchText(rStartPt, nWidth, rStr, nIndex, nLen);

    if (!IsDeviceOutputNecessary() && !IsLayoutCalculationNecessary())
        return;

    std::unique_ptr<SalLayout> pSalLayout
        = LayoutText(vcl::text::TextSpan{ rStr, nIndex, nLen },
                     vcl::text::LayoutConstraints{ rStartPt, static_cast<tools::Long>(nWidth) },
                     vcl::text::LayoutCacheData{}, vcl::text::RenderSelection{});

    if (pSalLayout)
    {
        if (moRecordingState)
            vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, nIndex, nLen, pSalLayout.get());

        if ((!moRecordingState || !moRecordingState->IsActive()))
            ImplDrawText(*pSalLayout);
    }
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

static Color lcl_getDisabledTextColor(const OutputDevice& rTargetDevice)
{
    const StyleSettings& rStyleSettings = rTargetDevice.GetSettings().GetStyleSettings();

    if (rStyleSettings.GetHighContrastMode())
    {
        Color aCol;

        if (rTargetDevice.IsBackground())
            aCol = rTargetDevice.GetBackground().GetColor();
        else
            aCol = rStyleSettings.GetFaceColor();

        if (aCol.IsDark())
            return COL_GREEN;

        if (aCol.IsBright())
            return COL_LIGHTGREEN;
    }

    return rStyleSettings.GetDisableColor();
}

void OutputDevice::ImplDrawMnemonic(vcl::TextLayoutCommon& rLayout, const OUString& rStr,
                                    sal_Int32 nIndex, sal_Int32 nLen, sal_Int32 nRelMnemonicPos,
                                    const Point& rPos)
{
    KernArray aDXArray;
    rLayout.GetTextArray(rStr, &aDXArray, nIndex, nLen, true);

    vcl::text::TextLayoutEngine::MnemonicDeviceParams aParams{ GetFontMetric().GetAscent(),
                                                               GetOutOffXPixel(),
                                                               GetOutOffYPixel() };

    auto aGeo = vcl::text::TextLayoutEngine::GetMnemonicGeometry(
        [&](tools::Long w) { return LogicWidthToDeviceSubPixel(w); },
        [&](tools::Long w) { return LogicWidthToDevicePixel(w); },
        [&](const Point& p) { return LogicToPixel(p); }, aParams, aDXArray, nRelMnemonicPos, rPos);

    ImplDrawMnemonicLine(aGeo.nX, aGeo.nY, static_cast<double>(aGeo.nWidth));
}

void OutputDevice::ImplDrawText(OutputDevice& rTargetDevice, const tools::Rectangle& rRect,
                                const OUString& rOrigStr, DrawTextFlags nStyle,
                                std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                                vcl::TextLayoutCommon& rLayout)
{
    bool bDisabled = (nStyle & DrawTextFlags::Disable) && !pVector;

    if (bDisabled)
        rTargetDevice.Push(vcl::PushFlags::TEXTCOLOR | vcl::PushFlags::TEXTFILLCOLOR);

    comphelper::ScopeGuard aStateGuard([&rTargetDevice, bDisabled]() {
        if (bDisabled)
            rTargetDevice.Pop();
    });

    if (bDisabled)
        rTargetDevice.SetTextColor(lcl_getDisabledTextColor(rTargetDevice));

    if (rRect.GetWidth() <= 0 || rRect.GetHeight() <= 0)
    {
        if (nStyle & DrawTextFlags::Clip)
            return;

        static bool bFuzzing = comphelper::IsFuzzing();

        SAL_WARN_IF(bFuzzing, "vcl", "skipping negative rectangle");

        if (bFuzzing)
            return;
    }

    tools::Long nTextHeight = rTargetDevice.GetTextHeight();
    TextAlign eAlign = rTargetDevice.GetTextAlign();
    sal_Int32 nMnemonicPos = -1;

    OUString aStr = rOrigStr;
    if (nStyle & DrawTextFlags::Mnemonic)
        aStr = removeMnemonicFromString(aStr, nMnemonicPos);

    const bool bDrawMnemonics = !(rTargetDevice.GetSettings().GetStyleSettings().GetOptions()
                                  & StyleSettingsOptions::NoMnemonics)
                                && !pVector;

    if (nStyle & DrawTextFlags::MultiLine)
    {
        rTargetDevice.ImplDrawTextMultiLine(rTargetDevice, rRect, aStr, nStyle, pVector,
                                            pDisplayText, rLayout, nTextHeight, eAlign,
                                            nMnemonicPos, bDrawMnemonics);
    }
    else
    {
        rTargetDevice.ImplDrawTextSingleLine(rTargetDevice, rRect, aStr, nStyle, pVector,
                                             pDisplayText, rLayout, nTextHeight, eAlign,
                                             nMnemonicPos, bDrawMnemonics);
    }
}

void OutputDevice::ImplDrawTextSingleLine(OutputDevice& rTargetDevice,
                                          const tools::Rectangle& rRect, const OUString& rStr,
                                          DrawTextFlags nStyle,
                                          std::vector<tools::Rectangle>* pVector,
                                          OUString* pDisplayText, vcl::TextLayoutCommon& rLayout,
                                          tools::Long nTextHeight, TextAlign eAlign,
                                          sal_Int32 nMnemonicPos, bool bDrawMnemonics)
{
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();
    OUString aDrawStr = rStr;

    tools::Long nTextWidth = rLayout.GetTextWidth(aDrawStr, 0, -1);

    if (nTextWidth > nWidth)
    {
        if (nStyle & TEXT_DRAW_ELLIPSIS)
        {
            aDrawStr = rLayout.GetEllipsisString(aDrawStr, nWidth, nStyle);
            nStyle &= ~DrawTextFlags(DrawTextFlags::Center | DrawTextFlags::Right);
            nStyle |= DrawTextFlags::Left;
            nTextWidth = rLayout.GetTextWidth(aDrawStr, 0, aDrawStr.getLength());
        }
    }
    else
    {
        if (nTextHeight <= nHeight)
            nStyle &= ~DrawTextFlags::Clip;
    }

    Point aPos = vcl::text::TextLayoutEngine::CalculateLayoutOrigin(
        rTargetDevice, rRect, nTextWidth, nTextHeight, nStyle, eAlign);

    auto fnDrawMnemonic = [&]() {
        if (bDrawMnemonics && nMnemonicPos != -1 && nMnemonicPos < aDrawStr.getLength())
        {
            rTargetDevice.ImplDrawMnemonic(rLayout, aDrawStr, 0, aDrawStr.getLength(), nMnemonicPos,
                                           aPos);
        }
    };

    if (nStyle & DrawTextFlags::Clip)
    {
        auto popIt = rTargetDevice.ScopedPush(vcl::PushFlags::CLIPREGION);
        rTargetDevice.IntersectClipRegion(rRect);
        rLayout.DrawText(aPos, aDrawStr, 0, aDrawStr.getLength(), pVector, pDisplayText);
        fnDrawMnemonic();
    }
    else
    {
        rLayout.DrawText(aPos, aDrawStr, 0, aDrawStr.getLength(), pVector, pDisplayText);
        fnDrawMnemonic();
    }
}

void OutputDevice::ImplDrawTextMultiLine(OutputDevice& rTargetDevice, const tools::Rectangle& rRect,
                                         const OUString& rStr, DrawTextFlags nStyle,
                                         std::vector<tools::Rectangle>* pVector,
                                         OUString* pDisplayText, vcl::TextLayoutCommon& rLayout,
                                         tools::Long nTextHeight, TextAlign eAlign,
                                         sal_Int32 nMnemonicPos, bool bDrawMnemonics)
{
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();
    Point aPos = rRect.TopLeft();

    if (!nTextHeight)
        return;

    vcl::text::MultiLineLayout aLayout;
    vcl::text::TextLayoutEngine::CalculateMultiLineLayout(rLayout, aLayout, rRect, nTextHeight,
                                                          nWidth, nHeight, rStr, nStyle);
    nStyle = aLayout.nResultStyle;

    if (nStyle & DrawTextFlags::Clip)
    {
        rTargetDevice.Push(vcl::PushFlags::CLIPREGION);
        rTargetDevice.IntersectClipRegion(rRect);
    }

    comphelper::ScopeGuard aClipGuard([&]() {
        if (nStyle & DrawTextFlags::Clip)
            rTargetDevice.Pop();
    });

    tools::Long nTotalTextHeight = aLayout.nFormatLines * nTextHeight;
    if (nStyle & DrawTextFlags::Bottom)
        aPos.AdjustY(nHeight - nTotalTextHeight);
    else if (nStyle & DrawTextFlags::VCenter)
        aPos.AdjustY((nHeight - nTotalTextHeight) / 2);

    if (eAlign == ALIGN_BOTTOM)
        aPos.AdjustY(nTextHeight);
    else if (eAlign == ALIGN_BASELINE)
        aPos.AdjustY(rTargetDevice.GetFontMetric().GetAscent());

    for (sal_Int32 i = 0; i < aLayout.nFormatLines; i++)
    {
        if (moRecordingState)
        {
            vcl::text::AccessibilityRecorder aRecorder(*moRecordingState);

            if (aRecorder.IsActive())
                vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, aLayout.aLineInfo.GetLine(i).GetIndex(), 0, nullptr, true);
        }

        if (moRecordingState)
        {
            vcl::text::AccessibilityRecorder aRecorder(*moRecordingState);

            if (aRecorder.IsActive())
                vcl::text::TextRecordingDispatcher::Dispatch(*moRecordingState, *this, rStr, aLayout.aLineInfo.GetLine(i).GetIndex(), 0, nullptr, true);
        }

        ImplTextLineInfo& rLineInfo = aLayout.aLineInfo.GetLine(i);

        if (nStyle & DrawTextFlags::Right)
            aPos.AdjustX(nWidth - rLineInfo.GetWidth());
        else if (nStyle & DrawTextFlags::Center)
            aPos.AdjustX((nWidth - rLineInfo.GetWidth()) / 2);

        sal_Int32 nIndex = rLineInfo.GetIndex();
        sal_Int32 nLineLen = rLineInfo.GetLen();

        rLayout.DrawText(aPos, rStr, nIndex, nLineLen, pVector, pDisplayText);

        if (bDrawMnemonics
            && vcl::text::TextLayoutEngine::IsMnemonicInRange(nMnemonicPos, nIndex, nLineLen))
        {
            rTargetDevice.ImplDrawMnemonic(rLayout, rStr, nIndex, nLineLen, nMnemonicPos - nIndex,
                                           aPos);
        }

        aPos.AdjustY(nTextHeight);
        aPos.setX(rRect.Left());
    }

    if (!aLayout.aLastLine.isEmpty())
    {
        rLayout.DrawText(aPos, aLayout.aLastLine, 0, aLayout.aLastLine.getLength(), pVector,
                         pDisplayText);
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

    // Redirect recording to rMtf
    vcl::MetafileRecorder::ScopedSwitch aMetaFileSwitch(maRecorder, &rMtf);

    EnableOutput(false);

    // #i47157# Factored out to ImplDrawTextRect(), to be shared
    // between us and DrawText()
    vcl::DefaultTextLayout aLayout(*this);
    ImplDrawText(*this, rRect, rOrigStr, nStyle, nullptr, nullptr, aLayout);

    // and restore again
    EnableOutput(bOutputEnabled);
}

void OutputDevice::DrawText(const tools::Rectangle& rRect, const OUString& rOrigStr,
                            DrawTextFlags nStyle, std::vector<tools::Rectangle>* pVector,
                            OUString* pDisplayText, vcl::TextLayoutCommon* _pTextLayout)
{
    assert(!is_double_buffered_window());

    if ((moRecordingState && moRecordingState->IsActive()))
    {
        pVector = &moRecordingState->mpLayoutData->m_aUnicodeBoundRects;
        pDisplayText = &moRecordingState->mpLayoutData->m_aDisplayText;
    }

    bool bDecomposeTextRectAction
        = (_pTextLayout != nullptr) && _pTextLayout->DecomposeTextRectAction();

    // Semantic Tagging: Group decomposed actions or record atomic action
    std::unique_ptr<vcl::ScopedMetaGroup> oMetaGroup;
    if (bDecomposeTextRectAction)
        oMetaGroup = maRecorder.CreateScopedGroup("DrawTextRect Decomposed");
    if (!bDecomposeTextRectAction)
        maRecorder.RecordDrawTextRect(rRect, rOrigStr, nStyle);

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
    std::optional<vcl::MetafileRecorder::ScopedSuspend> xMetaFileSuspend;
    if (!bDecomposeTextRectAction)
        xMetaFileSuspend.emplace(maRecorder);

    // #i47157# Factored out to ImplDrawText(), to be used also
    // from AddTextRectActions()
    vcl::DefaultTextLayout aDefaultLayout(*this);
    ImplDrawText(*this, rRect, rOrigStr, nStyle, pVector, pDisplayText,
                 _pTextLayout ? *_pTextLayout : aDefaultLayout);

    // and enable again

}

tools::Rectangle OutputDevice::GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                           DrawTextFlags nStyle, TextRectInfo* pInfo,
                                           const vcl::TextLayoutCommon* pTextLayout) const
{
    vcl::text::TextLayoutEngine::LayoutRequest aReq;
    aReq.aText = rStr;
    aReq.aTargetRect = rRect;
    aReq.nStyle = nStyle;
    aReq.nMnemonicPos = -1;
    aReq.nFontOrientation = mpGraphicsState->maFont.GetOrientation();
    aReq.nFontAscent = GetFontMetric().GetAscent();
    aReq.nFontHeight = GetTextHeight();

    const vcl::TextLayoutCommon* pLayout = pTextLayout;

    if (!pLayout)
    {
        vcl::DefaultTextLayout aDefault(const_cast<OutputDevice&>(*this));
        auto aResult = vcl::text::TextLayoutEngine::CalculateLayout(*mpMapper, aReq, aDefault);

        if (pInfo)
        {
            pInfo->mnLineCount = aResult.nLineCount;
            pInfo->mnMaxWidth = aResult.nMaxWidth;
            pInfo->mbEllipsis = aResult.bEllipsisGenerated;
        }
        return aResult.aTextRect;
    }

    auto aResult = vcl::text::TextLayoutEngine::CalculateLayout(*mpMapper, aReq, *pLayout);

    if (pInfo)
    {
        pInfo->mnLineCount = aResult.nLineCount;
        pInfo->mnMaxWidth = aResult.nMaxWidth;
        pInfo->mbEllipsis = aResult.bEllipsisGenerated;
    }

    return aResult.aTextRect;
}

void OutputDevice::DrawCtrlText(const Point& rPos, const OUString& rStr, const sal_Int32 nIndex,
                                const sal_Int32 nLen, DrawTextFlags nStyle,
                                std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                                const SalLayoutGlyphs* pGlyphs)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary() || (nIndex >= rStr.getLength()))
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mpClippingController->IsDirty())
        InitClipRegion();

    if (IsOutputCulled())
        return;

    sal_Int32 nCorrectedIndex = nIndex;
    sal_Int32 nCorrectedLen = nLen;

    nCorrectedLen
        = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nCorrectedIndex, nCorrectedLen);

    auto aMnemonicText
        = vcl::text::TextLayoutEngine::PrepareMnemonicText(rStr, nCorrectedIndex, nCorrectedLen);
    const OUString& aStr = aMnemonicText.aText;
    nCorrectedIndex = aMnemonicText.nIndex;
    nCorrectedLen = aMnemonicText.nLen;
    sal_Int32 nMnemonicPos = aMnemonicText.nMnemonicPos;

    tools::Long nMnemonicX = 0;
    tools::Long nMnemonicY = 0;
    tools::Long nMnemonicWidth = 0;

    if (nMnemonicPos != -1)
    {
        if (nStyle & DrawTextFlags::Mnemonic && !pVector
            && !(GetSettings().GetStyleSettings().GetOptions() & StyleSettingsOptions::NoMnemonics))
        {
            SAL_WARN_IF(nMnemonicPos >= (nCorrectedIndex + nCorrectedLen), "vcl",
                        "Mnemonic underline marker after last character");

            const bool bInvalidPos = (nMnemonicPos >= nCorrectedLen);

            if (bInvalidPos)
                nMnemonicPos = nCorrectedLen - 1;

            KernArray aDXArray;
            GetTextArray(aStr, &aDXArray, nCorrectedIndex, nCorrectedLen, true, nullptr, pGlyphs);

            vcl::text::TextLayoutEngine::MnemonicDeviceParams aParams{ GetFontMetric().GetAscent(),
                                                                       GetOutOffXPixel(),
                                                                       GetOutOffYPixel() };

            // Pass bInvalidPos to bTrailing to handle BiDi edge cases
            const auto aGeo = vcl::text::TextLayoutEngine::GetMnemonicGeometry(
                [&](tools::Long w) { return LogicWidthToDeviceSubPixel(w); },
                [&](tools::Long w) { return LogicWidthToDevicePixel(w); },
                [&](const Point& p) { return LogicToPixel(p); }, aParams, aDXArray,
                nMnemonicPos - nCorrectedIndex, rPos, bInvalidPos);

            nMnemonicX = aGeo.nX;
            nMnemonicY = aGeo.nY;
            nMnemonicWidth = aGeo.nWidth;
        }
        else
        {
            nMnemonicPos = -1; // Reset - we don't show the mnemonic
        }
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
    auto aMnemonicText
        = vcl::text::TextLayoutEngine::PrepareMnemonicText(rStr, 0, rStr.getLength());
    const OUString& aStr = aMnemonicText.aText;
    sal_Int32 nIndex = aMnemonicText.nIndex;
    sal_Int32 nLen = aMnemonicText.nLen;

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

    bool bOldMap = mpMapper->IsMapModeEnabled();

    if (bOldMap)
    {
        mpMapper->EnableMapMode(false);
    }

    vcl::text::LayoutResources aResources
        = { mpFontRealization->mxFont.get(), *mpMapper, &GetFontCache(), GetFontCollection(),
            mpForcedFallbackInstance.get(),
            [this]() {
                const_cast<OutputDevice*>(this)->AcquireGraphics();
                return mpGraphics;
            },
            IsRTLEnabled(),
            // MapMode is explicitly disabled above, so Subpixel might be false depending on config
            IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
            *mpGraphicsState, *mpFontRealization };

    bool bRet = vcl::text::TextLayoutEngine::GetTextOutlines(
        aResources, rVector, rStr, nBase, nIndex, nLen, nLayoutWidth, pDXArray, pKashidaArray);

    if (bOldMap)
    {
        mpMapper->EnableMapMode(bOldMap);
    }

    return bRet;
}

bool OutputDevice::GetTextOutlines(PolyPolyVector& rResultVector, const OUString& rStr,
                                   sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                   sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                   std::span<const sal_Bool> pKashidaArray) const
{
    rResultVector.clear();

    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;

    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, nBase, nIndex, nLen, nLayoutWidth, pDXArray,
                         pKashidaArray))
    {
        return false;
    }

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

    basegfx::B2DPolyPolygonVector aB2DPolyPolyVector;

    if (!GetTextOutlines(aB2DPolyPolyVector, rStr, 0 /*nBase*/, 0 /*nIndex*/, /*nLen*/ -1,
                         /*nLayoutWidth*/ 0, /*pDXArray*/ {}))
    {
        return false;
    }

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

    nLen = vcl::text::TextLayoutEngine::GetNormalizedLength(rStr, nIndex, nLen);

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

void OutputDevice::ImplRenderTextLayout(SalLayout& rSalLayout)
{
    if (!IsDeviceOutputNecessary())
        return;

    ImplDrawText(rSalLayout);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
