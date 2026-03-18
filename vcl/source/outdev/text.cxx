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

#include <vcl/deviceconcepts.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/rendercontext/SystemTextColorFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/rendercontext/WaveLineGeometry.hxx>
#include <vcl/text/TextEffects.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/TextLayoutData.hxx>
#include <vcl/text/TextRecordingDispatcher.hxx>
#include <vcl/text/TextRenderContext.hxx>
#include <vcl/text/TextRenderer.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/text/MultiLineEngine.hxx>
#include <vcl/text/MnemonicGeometry.hxx>
#include <vcl/text/MultiLineEngine.hxx>
#include <vcl/textrectinfo.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <TextLayoutCache.hxx>
#include <devicedispatcher.hxx>
#include <drawmode.hxx>
#include <font/FontController.hxx>
#include <text/FontMappingTracker.hxx>
#include <text/TextLayoutEngine.hxx>
#include <text/TextJustifier.hxx>
#include <text/TextAnalyzer.hxx>
#include <text/AccessibilityRecorder.hxx>
#include <text/MeasurementRecorder.hxx>
#include <text/SalLayoutFactory.hxx>
#include <textlayout.hxx>

#include <memory>
#include <optional>

std::optional<vcl::text::TextRenderContext> OutputDevice::CreateTextRenderContext()
{
    if (!mpGraphics && !AcquireGraphics())
        return std::nullopt; // Safely bail out!

    tools::Long nFrameWidth = vcl::DispatchDevice(*this, [](const auto& rDev) {
        return vcl::get_reference_width_v(rDev);
    });

    const LogicalFontInstance& rFontInst = *GetFontInstance();

    return vcl::text::TextRenderContext{
        *mpGraphics,
        *mpMapper,
        rFontInst,
        rFontInst.mnOrientation,
        IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl),
        nFrameWidth,
        ImplIsAntiparallel(),
        GetTextColor(),
        GetTextLineColor(),
        GetTextFillColor()
    };
}

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
    return vcl::text::MultiLineEngine::GetEllipsisString(
        rStr, nMaxWidth, nStyle, [this](const OUString& s) { return GetTextWidth(s); });
}

void OutputDevice::ImplDrawTextDecoration(const SalLayout& rSalLayout)
{
    tools::Rectangle InkRect = vcl::text::TextGeometry::GetTextInkBounds(rSalLayout, *mpFontRealization);
    Point aBasePt(InkRect.Left(), InkRect.Top());
    tools::Rectangle aLogicalRect(Point(0, 0), InkRect.GetSize());

    if (mpGraphicsState->mbLineColor || mbLineColorDirty)
    {
        mpGraphics->SetLineColor();
        mbLineColorDirty = true;
    }

    mpGraphics->SetFillColor(GetTextFillColor());
    mbFillColorDirty = true;

    Degree10 nOrientation = mpFontRealization->mxFont->mnOrientation;
    auto aGeo = vcl::text::TextGeometry::GetRotatedGeometry(aBasePt, aLogicalRect, nOrientation);

    const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    if (bRTL)
    {
        tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
        bool bAntiparallel = ImplIsAntiparallel();

        if (aGeo.mbIsPolygon)
            mpMapper->MirrorDevicePixelPolygon(aGeo.maPoly, nFrameWidth, bRTL, bAntiparallel);
        else
            mpMapper->MirrorDevicePixelRect(aGeo.maRect, nFrameWidth, bRTL, bAntiparallel);
    }

    if (auto aCtx = CreateTextRenderContext())
        vcl::text::TextRenderer::DrawTextDecoration(*aCtx, aGeo);
}

bool OutputDevice::ImplDrawRotateText(SalLayout& rSalLayout)
{
    basegfx::B2DPoint aOrigBase = rSalLayout.DrawBase();
    basegfx::B2DPoint aOrigOffset = rSalLayout.DrawOffset();
    comphelper::ScopeGuard aRestoreGuard([&]() {
        rSalLayout.DrawBase() = aOrigBase;
        rSalLayout.DrawOffset() = aOrigOffset;
    });

    tools::Rectangle aBoundRect
        = vcl::text::TextGeometry::GetTextInkBounds(rSalLayout, *mpFontRealization, false);

    if (aBoundRect.IsEmpty())
        return false;

    // 1. Prepare the Off-Screen Canvas (VirtualDevice)
    ScopedVclPtrInstance<VirtualDevice> pVDev(*this);
    if (!pVDev->SetOutputSizePixel(aBoundRect.GetSize()))
        return false;

    const vcl::font::FontSelectPattern& rPattern = mpFontRealization->mxFont->GetFontSelectPattern();
    vcl::Font aFont(GetFont());
    aFont.SetOrientation(0_deg10); // Draw horizontal first
    aFont.SetFontSize(Size(rPattern.mnWidth, rPattern.mnHeight));

    pVDev->SetFont(aFont);
    pVDev->SetTextColor(COL_BLACK);
    pVDev->SetTextFillColor();

    if (!pVDev->InitFont())
        return false;

    pVDev->ImplInitTextColor();

    // 2. Draw text onto the canvas
    rSalLayout.DrawBase() = basegfx::B2DPoint(-aBoundRect.Left(), -aBoundRect.Top());
    rSalLayout.DrawOffset() = basegfx::B2DPoint(0, 0);
    rSalLayout.DrawText(*pVDev->mpGraphics);

    // 3. Extract and rotate the bitmap
    Bitmap aBmp = pVDev->GetBitmap(Point(), aBoundRect.GetSize());
    if (aBmp.IsEmpty())
        return false;

    aBmp.Rotate(mpFontRealization->mxFont->mnOwnOrientation, COL_WHITE);

    // 4. Stamp the bitmap back to the screen
    Point aPoint = vcl::text::TextGeometry::GetRotatedImageOrigin(
        Point(aOrigBase.getX(), aOrigBase.getY()), aBoundRect, mpFontRealization->mxFont->mnOwnOrientation);

    tools::Long nOldOffX = GetOutOffXPixel();
    tools::Long nOldOffY = GetOutOffYPixel();
    bool bOldMap = mpMapper->IsMapModeEnabled();

    // Suspend recording for mask drawing
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    comphelper::ScopeGuard aStateRestoreGuard([&]() {
        mpMapper->EnableMapMode(bOldMap);
        SetDeviceOriginX(nOldOffX);
        SetDeviceOriginY(nOldOffY);
    });

    SetDeviceOriginX(0);
    SetDeviceOriginY(0);
    mpMapper->EnableMapMode(false);

    DrawMask(aPoint, aBmp, GetTextColor());

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
    {
        tools::Long nOldX = rSalLayout.DrawBase().getX();

        // Ensure X is restored for decoration calculations after glyph drawing
        comphelper::ScopeGuard aRestoreGuard([&]() { rSalLayout.DrawBase().setX(nOldX); });

        if (HasMirroredGraphics() || IsRTLEnabled())
        {
            vcl::text::MirroringContext aCtx{
                nOldX,
                IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth(),
                GetOutputWidthPixel(),
                GetOutOffXPixel(),
                HasMirroredGraphics(),
                IsRTLEnabled()
            };

            rSalLayout.DrawBase().setX(vcl::text::TextGeometry::GetMirroredX(aCtx));
        }

        rSalLayout.DrawText(*mpGraphics);
    }

    // Render Decorations (Underlines, Strikeouts, Overlines)
    if (bTextLines)
    {
        const vcl::Font& rFont = mpGraphicsState->maFont;

        // Call the new internal orchestrator to handle rotation and mirroring
        DrawTextLines(rSalLayout,
                      rFont.GetStrikeout(),
                      rFont.GetUnderline(),
                      rFont.GetOverline(),
                      rFont.IsWordLineMode(),
                      rFont.IsUnderlineAbove());
    }

    if (mpGraphicsState->maFont.GetEmphasisMark() & FontEmphasisMark::Style)
    {
        // TODO This should eventually be refactored to a similar local DrawEmphasisMarks
        // to fully remove the 'OutputDevice&' dependency from PrimitiveRenderer.
        vcl::rendercontext::PrimitiveRenderer::DrawEmphasisMarks(*this, rSalLayout);
    }
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

    vcl::text::ReliefColors aColors = vcl::text::TextEffects::GetReliefColors(aOldColor, aOldTextLineColor, aOldOverlineColor);

    // Draw Relief Shadow
    SetTextColor(aColors.maReliefColor);
    SetTextLineColor(aColors.maReliefColor);
    SetOverlineColor(aColors.maReliefColor);
    ImplInitTextColor();

    tools::Long nOff = vcl::text::TextGeometry::GetReliefOffset(
        GetDPIX(), mpGraphicsState->maFont.GetRelief());
    rSalLayout.DrawOffset() += basegfx::B2DPoint(nOff, nOff);

    ImplRenderLayout(rSalLayout, mpFontRealization->bHasLineDecorations);

    // Draw Main Text
    rSalLayout.DrawOffset() = aOrigOffset; // Reset offset for main draw

    SetTextColor(aColors.maTextColor);
    SetTextLineColor(aColors.maLineColor);
    SetOverlineColor(aColors.maOverlineColor);
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

    SetTextColor(vcl::text::TextEffects::GetShadowColor(aOldColor));
    ImplInitTextColor();

    // Draw Shadow
    tools::Long nOff = vcl::text::TextGeometry::GetShadowOffset(
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

    for (const auto& rOffset : vcl::text::TextGeometry::GetOutlineOffsets())
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
        ImplDrawTextDecoration(rSalLayout);

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

    nLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nIndex, nLen);
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

void OutputDevice::DrawTextLines(SalLayout& rSalLayout, FontStrikeout eStrikeout,
                                 FontLineStyle eUnderline, FontLineStyle eOverline,
                                 bool bWordLine, bool bUnderlineAbove)
{
    if (!mpGraphics && !AcquireGraphics())
        return;

    const vcl::font::FontRealization& rFontRealization = *mpFontRealization;
    const Degree10 nOrientation = rFontRealization.mxFont->mnOrientation;
    const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    const tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
    const bool bAntiparallel = ImplIsAntiparallel();

    // Identify logical segments (WordLine vs Full Line)
    std::vector<std::pair<double, double>> aLogicalSegments;
    if (bWordLine)
        vcl::text::TextGeometry::GetWordLineSegments(rSalLayout, rFontRealization, aLogicalSegments);
    else
        aLogicalSegments.push_back({ 0.0, rSalLayout.GetTextWidth() });

    basegfx::B2DPoint aDrawBase = rSalLayout.DrawBase();
    Point aOrigin(aDrawBase.getX(), aDrawBase.getY());

    // Transforms logical offsets into rotated/mirrored device geometry
    auto fnCollectDeviceGeometry = [&](std::vector<vcl::text::RotatedGeometry>& rVector,
                            tools::Long nX, tools::Long nWidth, tools::Long nY, tools::Long nHeight) {
        auto aGeo = vcl::text::TextGeometry::GetRotatedGeometry(
            aOrigin, tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight)), nOrientation);

        if (bRTL)
        {
            if (aGeo.mbIsPolygon)
                mpMapper->MirrorDevicePixelPolygon(aGeo.maPoly, nFrameWidth, bRTL, bAntiparallel);
            else
                mpMapper->MirrorDevicePixelRect(aGeo.maRect, nFrameWidth, bRTL, bAntiparallel);
        }
        rVector.push_back(aGeo);
    };

    if (eUnderline != LINESTYLE_NONE || eOverline != LINESTYLE_NONE)
    {
        std::vector<vcl::text::RotatedGeometry> aLineGeos;

        // Resolve metrics for both decorations
        vcl::text::StraightLineMetrics aUnderMetrics(*mpFontInstance->mxFontMetric, eUnderline, 0, bUnderlineAbove);
        vcl::text::StraightLineMetrics aOverMetrics(*mpFontInstance->mxFontMetric, eOverline, 0, false);

        for (const auto& rSeg : aLogicalSegments)
        {
            tools::Long nX = static_cast<tools::Long>(rSeg.first);
            tools::Long nWidth = static_cast<tools::Long>(rSeg.second);

            if (eUnderline != LINESTYLE_NONE)
            {
                fnCollectDeviceGeometry(aLineGeos, nX, nWidth, aUnderMetrics.nLinePos, aUnderMetrics.nLineHeight);
                if (eUnderline == LINESTYLE_DOUBLE)
                    fnCollectDeviceGeometry(aLineGeos, nX, nWidth, aUnderMetrics.nLinePos2, aUnderMetrics.nLineHeight);
            }

            if (eOverline != LINESTYLE_NONE)
            {
                fnCollectDeviceGeometry(aLineGeos, nX, nWidth, aOverMetrics.nLinePos, aOverMetrics.nLineHeight);
                if (eOverline == LINESTYLE_DOUBLE)
                    fnCollectDeviceGeometry(aLineGeos, nX, nWidth, aOverMetrics.nLinePos2, aOverMetrics.nLineHeight);
            }
        }

        if (!aLineGeos.empty())
        {
            Color aColor = IsTextLineColor() ? GetTextLineColor() : GetTextColor();
            vcl::rendercontext::PrimitiveRenderer::DrawTextLines(*mpGraphics, aLineGeos, aColor);
        }
    }

    if (eStrikeout != STRIKEOUT_NONE)
    {
        // Character-based strikeouts (Slash/X) still need to go through the legacy path
        // until DrawStrikeoutChar is fully stateless.
        if (eStrikeout == STRIKEOUT_SLASH || eStrikeout == STRIKEOUT_X)
        {
             for (const auto& rSeg : aLogicalSegments)
             {
                 vcl::rendercontext::TextLineGeometry aCharGeo(
                     aOrigin, static_cast<tools::Long>(rSeg.first), rSeg.second,
                     eStrikeout, LINESTYLE_NONE, LINESTYLE_NONE, false);

                 ImplDrawStrikeoutChar(aCharGeo, 0, GetTextColor());
             }
        }
        else // Line-based strikeouts (Single, Bold, Double)
        {
            std::vector<vcl::text::RotatedGeometry> aStrikeGeos;
            vcl::text::StrikeoutGeometry aStrikeMetrics = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
                *mpFontInstance->mxFontMetric, eStrikeout, 0);

            for (const auto& rSeg : aLogicalSegments)
            {
                tools::Long nX = static_cast<tools::Long>(rSeg.first);
                tools::Long nWidth = static_cast<tools::Long>(rSeg.second);

                for (const auto& rLine : aStrikeMetrics.aSegments)
                {
                    fnCollectDeviceGeometry(aStrikeGeos, nX, nWidth, rLine.nYOffset, rLine.nHeight);
                }
            }

            if (!aStrikeGeos.empty())
            {
                Color aColor = IsTextLineColor() ? GetTextLineColor() : GetTextColor();
                vcl::rendercontext::PrimitiveRenderer::DrawTextLines(*mpGraphics, aStrikeGeos, aColor);
            }
        }
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
            = vcl::text::TextGeometry::GetTextHeightPixel(*mpFontRealization);

        return DevicePixelToLogicHeight(static_cast<tools::Long>(nPixelHeight));
    }
    return 0;
}

double OutputDevice::GetTextHeightDouble() const
{
    if (!InitFont())
        return 0;

    const double nHeight = vcl::text::TextGeometry::GetTextHeightPixel(*mpFontRealization);

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

    nLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nIndex, nLen);

    nPartLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nPartIndex, nPartLen);

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
    nLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nIndex, nLen);
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
        vcl::text::TextJustifier::ZeroFillKernArray(pKernArray,
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

    return vcl::text::TextGeometry::GetPartialTextArray(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, pKernArray, nPartIndex, nPartLen,
        bCaret, vcl::text::LayoutCacheData{ pLayoutCache, pSalLayoutCache }, pBounds);
}

void OutputDevice::GetCaretPositions(const OUString& rStr, KernArray& rCaretPos, sal_Int32 nIndex,
                                     sal_Int32 nLen, const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont()) return;

    vcl::text::LayoutResources aResources = {
        mpFontRealization->mxFont.get(), *mpMapper, &GetFontCache(), GetFontCollection(),
        mpForcedFallbackInstance.get(),
        [this]() { const_cast<OutputDevice*>(this)->AcquireGraphics(); return mpGraphics; },
        IsRTLEnabled(),
        IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
        *mpGraphicsState, *mpFontRealization
    };

    vcl::text::TextGeometry::GetCaretPositions(
        aResources, vcl::text::TextSpan{ rStr, nIndex, nLen }, rCaretPos,
        vcl::text::LayoutCacheData{ nullptr, pGlyphs });
}

void OutputDevice::DrawStretchText(const Point& rStartPt, sal_Int32 nWidth, const OUString& rStr,
                                   sal_Int32 nIndex, sal_Int32 nLen)
{
    assert(!is_double_buffered_window());

    nLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nIndex, nLen);

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
    return vcl::text::TextAnalyzer::GetBiDiLayoutFlags(mpFontRealization->eLayoutMode, rStr,
                                                           nMinIndex, nEndIndex);
}

void OutputDevice::StartTrackingFontMappingUse() { vcl::text::FontMappingTracker::StartTracking(); }

OutputDevice::FontMappingUseData OutputDevice::FinishTrackingFontMappingUse()
{
    return vcl::text::FontMappingTracker::FinishTracking();
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

    return vcl::text::TextAnalyzer::GetTextIsRTL(aResources, rString, nIndex, nLen);
}

sal_Int32 OutputDevice::GetTextBreak(const OUString& rStr, tools::Long nTextWidth, sal_Int32 nIndex,
                                     sal_Int32 nLen, tools::Long nCharExtra,
                                     const vcl::text::TextLayoutCache* pLayoutCache,
                                     const SalLayoutGlyphs* pGlyphs) const
{
    if (!InitFont()) return -1;

    vcl::text::LayoutResources aResources = {
        mpFontRealization->mxFont.get(), *mpMapper, &GetFontCache(), GetFontCollection(),
        mpForcedFallbackInstance.get(),
        [this]() { const_cast<OutputDevice*>(this)->AcquireGraphics(); return mpGraphics; },
        IsRTLEnabled(),
        IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
        *mpGraphicsState, *mpFontRealization
    };

    return vcl::text::MultiLineEngine::GetTextBreak(
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

    return vcl::text::MultiLineEngine::GetTextBreakArray(
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

    vcl::text::MnemonicDeviceParams aParams{ GetFontMetric().GetAscent(),
                                                               GetOutOffXPixel(),
                                                               GetOutOffYPixel() };

    auto aGeo = vcl::text::TextGeometry::GetMnemonicGeometry(
        [&](tools::Long w) { return LogicWidthToDeviceSubPixel(w); },
        [&](tools::Long w) { return LogicWidthToDevicePixel(w); },
        [&](const Point& p) { return LogicToPixel(p); }, aParams, aDXArray, nRelMnemonicPos, rPos);

    vcl::rendercontext::PrimitiveRenderer::DrawMnemonicLine(*this, aGeo.nX, aGeo.nY, static_cast<double>(aGeo.nWidth));
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

    Point aPos = vcl::text::TextGeometry::CalculateLayoutOrigin(
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
    vcl::text::MultiLineEngine::CalculateMultiLineLayout(rLayout, aLayout, rRect, nTextHeight,
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
            && vcl::text::TextAnalyzer::IsMnemonicInRange(nMnemonicPos, nIndex, nLineLen))
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

    // #i47157# Factored out to ImplDrawTextDecoration(), to be shared
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
        oMetaGroup = maRecorder.CreateScopedGroup("DrawTextDecoration Decomposed");
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
    vcl::text::TextGeometry::LayoutRequest aReq;
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
        auto aResult = vcl::text::TextGeometry::CalculateLayout(*mpMapper, aReq, aDefault);

        if (pInfo)
        {
            pInfo->mnLineCount = aResult.nLineCount;
            pInfo->mnMaxWidth = aResult.nMaxWidth;
            pInfo->mbEllipsis = aResult.bEllipsisGenerated;
        }
        return aResult.aTextRect;
    }

    auto aResult = vcl::text::TextGeometry::CalculateLayout(*mpMapper, aReq, *pLayout);

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
        = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nCorrectedIndex, nCorrectedLen);

    auto aMnemonicText
        = vcl::text::TextAnalyzer::PrepareMnemonicText(rStr, nCorrectedIndex, nCorrectedLen);
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

            vcl::text::MnemonicDeviceParams aParams{ GetFontMetric().GetAscent(),
                                                     GetOutOffXPixel(),
                                                     GetOutOffYPixel() };

            // Pass bInvalidPos to bTrailing to handle BiDi edge cases
            const auto aGeo = vcl::text::TextGeometry::GetMnemonicGeometry(
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
        vcl::rendercontext::PrimitiveRenderer::DrawMnemonicLine(*this, nMnemonicX, nMnemonicY, nMnemonicWidth);

    if (oOldTextColor)
        SetTextColor(*oOldTextColor);

    if (oOldTextFillColor)
        SetTextFillColor(*oOldTextFillColor);
}

tools::Long OutputDevice::GetCtrlTextWidth(const OUString& rStr,
                                           const SalLayoutGlyphs* pGlyphs) const
{
    auto aMnemonicText
        = vcl::text::TextAnalyzer::PrepareMnemonicText(rStr, 0, rStr.getLength());
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

    return vcl::text::TextGeometry::GetLogicalTextBoundRect(aResources, rRect, rStr, nBase,
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

    // Temporarily disable MapMode to get raw pixel outlines (legacy behavior)
    bool bOldMap = mpMapper->IsMapModeEnabled();

    if (bOldMap)
    {
        mpMapper->EnableMapMode(false);
        InitFont();
    }

    vcl::text::LayoutResources aResources = {
        mpFontRealization->mxFont.get(), *mpMapper, &GetFontCache(), GetFontCollection(),
        mpForcedFallbackInstance.get(),
        [this]() { const_cast<OutputDevice*>(this)->AcquireGraphics(); return mpGraphics; },
        IsRTLEnabled(),
        IsMapModeEnabled() || isSubpixelPositioning() || SupportsSubpixelPositioning(),
        *mpGraphicsState, *mpFontRealization
    };

    bool bRet = vcl::text::TextGeometry::GetTextOutlines(
        aResources, rVector, rStr, nBase, nIndex, nLen, nLayoutWidth, pDXArray, pKashidaArray);

    if (bOldMap)
        mpMapper->EnableMapMode(bOldMap);

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
    return vcl::text::TextGeometry::GetTextInkBounds(rLayout, *mpFontRealization);
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

    vcl::text::TextJustifier::GetWordKashidaPositions(*pSalLayout, rText, *pOutMap);
}

bool OutputDevice::GetGlyphBoundRects(const Point& rOrigin, const OUString& rStr, int nIndex,
                                      int nLen, std::vector<tools::Rectangle>& rVector) const
{
    rVector.clear();

    if (nIndex >= rStr.getLength())
        return false;

    nLen = vcl::text::TextAnalyzer::GetNormalizedLength(rStr, nIndex, nLen);

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

/**
 * Pushes a clipping region to the OutputDevice for a text decoration.
 * Uses RAII to ensure the clip is popped.
 */
[[nodiscard]] static auto lcl_BeginDecorationClipping(OutputDevice& rOutDev, const Point& rOrigin,
                                                      double fWidth, tools::Long nAscent,
                                                      tools::Long nDescent)
{
    tools::Rectangle aPixelRect;
    aPixelRect.SetLeft(rOrigin.X());
    aPixelRect.SetRight(aPixelRect.Left() + static_cast<tools::Long>(fWidth));
    aPixelRect.SetBottom(rOrigin.Y() + nDescent);
    aPixelRect.SetTop(rOrigin.Y() - nAscent);

    const LogicalFontInstance& rFontInst = *rOutDev.GetFontInstance();
    const Degree10 nOrientation = rFontInst.mnOrientation;

    if (nOrientation)
    {
        tools::Polygon aPoly(aPixelRect);
        aPoly.Rotate(rOrigin, nOrientation);
        aPixelRect = aPoly.GetBoundRect();
    }

    // Crucial for overlines and rotated text to prevent "inside-out" empty rects
    aPixelRect.Normalize();

    rOutDev.Push(vcl::PushFlags::CLIPREGION);
    rOutDev.IntersectClipRegion(aPixelRect);

    return comphelper::ScopeGuard([&rOutDev]() { rOutDev.Pop(); });
}

/**
 * Calculates the rotated and offset origin point for text decorations.
 *
 * @param rOrigin      The base origin of the text (the baseline start).
 * @param nOrientation The font rotation in Degree10 (0.1 degree units).
 * @param nDistX       The horizontal offset along the baseline.
 * @param nY           The vertical offset relative to the baseline.
 * @return             The final Point in logical coordinates.
 */
static Point lcl_GetDecorationOrigin(const Point& rOrigin, Degree10 nOrientation,
                                     tools::Long nDistX, tools::Long nY)
{
    Point aOriginPt = rOrigin;

    if (nDistX || nY)
    {
        tools::Long nTmpX = nDistX;
        tools::Long nTmpY = nY;

        if (nOrientation)
        {
            // Rotate the relative offsets around the (0,0) pivot
            // before applying them to the absolute origin.
            Point aOffset(0, 0);
            aOffset.RotateAround(nTmpX, nTmpY, nOrientation);
        }

        aOriginPt.AdjustX(nTmpX);
        aOriginPt.AdjustY(nTmpY);
    }

    return aOriginPt;
}

void OutputDevice::ImplDrawTextLine(const vcl::rendercontext::TextLineGeometry& rGeo)
{
    vcl::text::TextLineOffsetInfo aInfo(*mpFontInstance->mxFontMetric, rGeo.meUnderline,
                                        rGeo.meOverline, rGeo.mbUnderlineAbove);

    Color aStrikeoutColor = GetTextColor();
    Color aUnderlineColor = GetTextLineColor();
    Color aOverlineColor = GetOverlineColor();

    if (!IsTextLineColor())
        aUnderlineColor = GetTextColor();

    if (!IsOverlineColor())
        aOverlineColor = GetTextColor();

    vcl::rendercontext::TextLineGeometry aDrawGeo = rGeo;
    if (IsRTLEnabled())
    {
        tools::Long nXAdd = aDrawGeo.mfWidth - aDrawGeo.mnDistX;
        if (mpFontInstance->mnOrientation)
            nXAdd = basegfx::fround<tools::Long>(
                nXAdd * cos(toRadians(mpFontInstance->mnOrientation)));
        aDrawGeo.maOrigin.AdjustX(nXAdd - 1);
    }

    const LogicalFontInstance& rFontInst = *GetFontInstance();
    const tools::Long nAscent = rFontInst.mxFontMetric->GetAscent();
    const tools::Long nDescent = rFontInst.mxFontMetric->GetDescent();

    auto fnDrawLine = [&](FontLineStyle eLineStyle, bool bIsWave, tools::Long nOffset, Color aColor,
                          bool bIsAbove = true) {
        if (eLineStyle == LINESTYLE_NONE)
            return;

        vcl::rendercontext::TextLineGeometry aLineGeo = aDrawGeo;
        aLineGeo.meUnderline = eLineStyle;

        if (bIsWave)
        {
            ImplDrawWaveTextLine(aLineGeo, nOffset, aColor, bIsAbove);
        }
        else
        {
            Point aOrigin = lcl_GetDecorationOrigin(aLineGeo.maOrigin, rFontInst.mnOrientation,
                                                    aLineGeo.mnDistX, nOffset);

            auto aClipGuard = lcl_BeginDecorationClipping(*this, aOrigin, aLineGeo.mfWidth,
                                                          nAscent, nDescent);

            ImplDrawStraightTextLine(aLineGeo, 0, aColor, bIsAbove);
        }
    };

    // Execute for Underline
    fnDrawLine(aDrawGeo.meUnderline, aInfo.bUnderlineIsWave, aInfo.nUnderlineOffset,
               aUnderlineColor, aDrawGeo.mbUnderlineAbove);

    // Execute for Overline (Overlines are always "above" the baseline)
    fnDrawLine(aDrawGeo.meOverline, aInfo.bOverlineIsWave, aInfo.nOverlineOffset, aOverlineColor);

    if (aDrawGeo.meStrikeout != STRIKEOUT_NONE)
    {
        if (aDrawGeo.meStrikeout == STRIKEOUT_SLASH || aDrawGeo.meStrikeout == STRIKEOUT_X)
        {
            ImplDrawStrikeoutChar(aDrawGeo, 0, aStrikeoutColor);
        }
        else
        {
            Point aStrikeoutOrigin
                = lcl_GetDecorationOrigin(aDrawGeo.maOrigin, rFontInst.mnOrientation,
                                          aDrawGeo.mnDistX, aInfo.nStrikeoutOffset);

            auto aClipGuard = lcl_BeginDecorationClipping(*this, aStrikeoutOrigin,
                                                          aDrawGeo.mfWidth, nAscent, nDescent);

            ImplDrawStrikeoutLine(aDrawGeo, aInfo.nStrikeoutOffset, aStrikeoutColor);
        }
    }
}

void OutputDevice::ImplDrawStrikeoutChar(const vcl::rendercontext::TextLineGeometry& rGeo,
                                         tools::Long nY, Color aColor)
{
    if (rGeo.mfWidth <= 0)
        return;

    vcl::text::LayoutResources aRes{
        mpFontInstance.get(), *mpMapper, &GetFontCache(), GetFontCollection(),
        nullptr, [&]() { return mpGraphics; }, IsRTLEnabled(), false,
        *mpGraphicsState, *mpFontRealization
    };

    std::unique_ptr<SalLayout> pLayout =
        vcl::text::TextGeometry::GetStrikeoutCharLayout(aRes, rGeo.mfWidth, rGeo.meStrikeout);

    if (!pLayout)
        return;

    Point aOriginPt = lcl_GetDecorationOrigin(rGeo.maOrigin, mpFontInstance->mnOrientation,
                                              rGeo.mnDistX, nY);

    auto aClipGuard = lcl_BeginDecorationClipping(*this, aOriginPt, rGeo.mfWidth,
                                                  mpFontInstance->mxFontMetric->GetAscent(),
                                                  mpFontInstance->mxFontMetric->GetDescent());

    if (auto aCtx = CreateTextRenderContext())
        vcl::text::TextRenderer::DrawStrikeoutCharLayout(*aCtx, *pLayout, aOriginPt, aColor);
}

void OutputDevice::ImplDrawStrikeoutLine(const vcl::rendercontext::TextLineGeometry& rGeo,
                                         tools::Long nY, Color aColor)
{
    if (rGeo.mfWidth <= 0)
        return;

    vcl::text::StrikeoutGeometry aStrikeoutGeo
        = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
            *mpFontInstance->mxFontMetric, rGeo.meStrikeout, nY);

    if (aStrikeoutGeo.aSegments.empty())
        return;

    if (mpGraphicsState->mbLineColor || mbLineColorDirty)
    {
        mpGraphics->SetLineColor();
        mbLineColorDirty = true;
    }

    mpGraphics->SetFillColor(aColor);
    mbFillColorDirty = true;

    if (auto aCtx = CreateTextRenderContext())
        vcl::text::TextRenderer::DrawStrikeoutLine(*aCtx, rGeo, aStrikeoutGeo);
}

void OutputDevice::ImplDrawWaveTextLine(const vcl::rendercontext::TextLineGeometry& rGeo,
                                        tools::Long nY, Color aColor, bool bIsAbove)
{
    vcl::text::WaveLineGeometry aWaveStyle = vcl::text::TextDecorator::CalculateWaveLineGeometry(
        *mpFontInstance->mxFontMetric, rGeo.meUnderline, bIsAbove, nY, GetDPIX(), GetDPIY());

    const Size aWavePixelSize = GetWaveLineSize(aWaveStyle.nLineWidth);
    const bool bDrawAsRect = shouldDrawWavePixelAsRect(aWaveStyle.nLineWidth);

    Degree10 nOrientation = mpFontInstance->mnOrientation;

    for (const auto& rSeg : aWaveStyle.aSegments)
    {
        vcl::rendercontext::WaveLineGeometry aWaveGeo(
            rGeo.maOrigin.X(), rGeo.maOrigin.Y(), rGeo.mnDistX, rSeg.nYOffset,
            rGeo.mfWidth, rSeg.nHeight, nOrientation, aWavePixelSize, bDrawAsRect);

        if (aWaveGeo.maWavePixelSize.Height() == 1 && aWaveGeo.maSize.Height() == 1)
        {
            // OutputDevice strictly manages the dirty flags
            mpGraphics->SetLineColor(aColor);
            mbLineColorDirty = true;

            if (auto aCtx = CreateTextRenderContext())
                vcl::text::TextRenderer::DrawWaveHairline(*aCtx, aWaveGeo, aColor);
        }
        else
        {
            vcl::rendercontext::PrimitiveRenderer::DrawWaveLine(*this, aWaveGeo, aColor);
        }
    }
}

void OutputDevice::ImplDrawStraightTextLine(const vcl::rendercontext::TextLineGeometry& rGeo,
                                            tools::Long nY, Color aColor, bool bIsAbove)
{
    static bool bFuzzing = comphelper::IsFuzzing();
    if (bFuzzing && rGeo.mfWidth > 25000)
        return;

    vcl::text::StraightLineMetrics aMetrics(*mpFontInstance->mxFontMetric, rGeo.meUnderline,
                                            nY, bIsAbove);

    if (!aMetrics.nLineHeight)
        return;

    if (mpGraphicsState->mbLineColor || mbLineColorDirty)
    {
        mpGraphics->SetLineColor();
        mbLineColorDirty = true;
    }

    mpGraphics->SetFillColor(aColor);
    mbFillColorDirty = true;

    // Calculate dashed/dotted segments if necessary
    std::vector<vcl::text::TextDashSegment> aDashSegments;
    if (aMetrics.eUnderline != LINESTYLE_SINGLE && aMetrics.eUnderline != LINESTYLE_BOLD && aMetrics.eUnderline != LINESTYLE_DOUBLE)
    {
        aDashSegments = vcl::text::TextDecorator::CalculateTextLineSegments(
            rGeo.mfWidth, aMetrics.eUnderline, aMetrics.nLineHeight, GetDPIX(), GetDPIY());
    }

    if (auto aCtx = CreateTextRenderContext())
        vcl::text::TextRenderer::DrawStraightTextLine(*aCtx, rGeo, aMetrics, aDashSegments);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
