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
#include <tools/mapunit.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <unotools/fontdefs.hxx>

#include <vcl/fontcapabilities.hxx>
#include <vcl/metaact.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/settings.hxx>

#include <window.h>
#include <CoordinateMapper.hxx>
#include <FontController.hxx>
#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <font/EmphasisMark.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/PhysicalFontFaceCollection.hxx>
#include <sallayout.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

#include <unicode/uchar.h>

const vcl::Font& OutputDevice::GetFont() const { return mpGraphicsState->maFont; }

void OutputDevice::SetFont(const vcl::Font& rNewFont)
{
    vcl::Font aFont
        = vcl::drawmode::GetFont(rNewFont, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaFontAction(aFont));
        // the color and alignment actions don't belong here
        // TODO: get rid of them without breaking anything...
        mpMetaFile->AddAction(new MetaTextAlignAction(aFont.GetAlignment()));
        mpMetaFile->AddAction(
            new MetaTextFillColorAction(aFont.GetFillColor(), !aFont.IsTransparent()));
    }

    if (mpGraphicsState->maFont.IsSameInstance(aFont))
        return;

    // Optimization MT/HDU: COL_TRANSPARENT means SetFont should ignore the font color,
    // because SetTextColor() is used for this.
    // #i28759# mpGraphicsState->maTextColor might have been changed behind our back, commit then, too.
    if (aFont.GetColor() != COL_TRANSPARENT
        && (aFont.GetColor() != mpGraphicsState->maFont.GetColor()
            || aFont.GetColor() != mpGraphicsState->maTextColor))
    {
        mpGraphicsState->maTextColor = aFont.GetColor();
        mbInitTextColor = true;
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaTextColorAction(aFont.GetColor()));
    }
    mpGraphicsState->maFont = aFont;
    mbNewFont = true;
}

void OutputDevice::SetFontCollection(const std::shared_ptr<vcl::font::PhysicalFontCollection>& pPFC)
{
    if (!mpFontController)
        mpFontController = std::make_unique<vcl::font::FontController>();
    mpFontController->SetFontCollection(pPFC);
}

vcl::font::PhysicalFontCollection* OutputDevice::GetFontCollection() const
{
    return mpFontController ? mpFontController->GetFontCollection() : nullptr;
}

const std::shared_ptr<vcl::font::PhysicalFontCollection>&
OutputDevice::GetSharedFontCollection() const
{
    static const std::shared_ptr<vcl::font::PhysicalFontCollection> pEmpty;
    if (ImplGetSVData()->mbDeInit)
        return pEmpty;

    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    return mpFontController->GetSharedFontCollection();
}

FontMetric OutputDevice::GetFontMetricFromCollection(sal_uInt32 nDevFontIndex) const
{
    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    if (!mpGraphics)
        AcquireGraphics();

    return mpFontController->GetFontMetricFromCollection(mpGraphics, nDevFontIndex);
}

sal_uInt32 OutputDevice::GetFontFaceCollectionCount() const
{
    if (!mpFontController)
        return 0;

    return mpFontController->GetFontFaceCollectionCount();
}

bool OutputDevice::IsFontAvailable(std::u16string_view rFontName) const
{
    ImplInitFontList();

    return mpFontController && mpFontController->IsFontAvailable(rFontName);
}

bool OutputDevice::AddTempDevFont(const OUString& rFileURL, const OUString& rFontName) const
{
    if (!mpGraphics && !const_cast<OutputDevice*>(this)->AcquireGraphics())
        return false;
    return mpFontController && mpFontController->AddTempDevFont(mpGraphics, rFileURL, rFontName);
}

bool OutputDevice::RemoveTempDevFont(const OUString& rFileURL, const OUString& rFontName)
{
    if (!mpGraphics && !AcquireGraphics())
        return false;
    return mpFontController && mpFontController->RemoveTempDevFont(mpGraphics, rFileURL, rFontName);
}

bool OutputDevice::GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const
{
    if (!ImplNewFont())
        return false;

    if (mpFontRealization && mpFontRealization->mxFont)
    {
        mpFontController->GetFontFeatures(mpFontRealization->mxFont.get(), rFontFeatures);
        return true;
    }

    return false;
}


void OutputDevice::ImplScaleFontMetric(FontMetric& rMetric) const
{
    rMetric.SetFontSize(PixelToLogic(rMetric.GetFontSize()));
    rMetric.SetAscent(DevicePixelToLogicHeight(rMetric.GetAscent()));
    rMetric.SetDescent(DevicePixelToLogicHeight(rMetric.GetDescent()));
    rMetric.SetInternalLeading(DevicePixelToLogicHeight(rMetric.GetInternalLeading()));
    rMetric.SetLineHeight(DevicePixelToLogicHeight(rMetric.GetLineHeight()));
    rMetric.SetSlant(DevicePixelToLogicHeight(rMetric.GetSlant()));
    rMetric.SetHangingBaseline(DevicePixelToLogicHeight(rMetric.GetHangingBaseline()));

    rMetric.SetUnitEm(DevicePixelToLogicWidth(rMetric.GetUnitEm()));
    rMetric.SetHorCJKAdvance(DevicePixelToLogicWidth(rMetric.GetHorCJKAdvance()));
    rMetric.SetVertCJKAdvance(DevicePixelToLogicHeight(rMetric.GetVertCJKAdvance()));

    // OutputDevice manages external leading separately due to legacy #i60945#
    rMetric.SetExternalLeading(DevicePixelToLogicHeight(GetFontExtLeading()));
}

FontMetric OutputDevice::GetFontMetric() const
{
    FontMetric aMetric;

    if (!ImplNewFont())
        return aMetric;

    aMetric = mpGraphicsState->maFont;
    aMetric.SetAlignment(TextAlign::ALIGN_TOP);

    mpFontController->PopulateFontMetric(
        aMetric, mpGraphicsState->maFont, mpFontRealization->mxFont.get(),
        mpFontRealization->nEmphasisAscent, mpFontRealization->nEmphasisDescent);

    ImplScaleFontMetric(aMetric);

    SAL_INFO("vcl.gdi.fontmetric", "OutputDevice::GetFontMetric:" << aMetric);
    return aMetric;
}

FontMetric OutputDevice::GetFontMetric(const vcl::Font& rFont) const
{
    // select font, query metrics, select original font again
    vcl::Font aOldFont = GetFont();
    const_cast<OutputDevice*>(this)->SetFont(rFont);
    FontMetric aMetric(GetFontMetric());
    const_cast<OutputDevice*>(this)->SetFont(aOldFont);
    return aMetric;
}

bool OutputDevice::GetFontCharMap(FontCharMapRef& rxFontCharMap) const
{
    if (!InitFont())
        return false;

    rxFontCharMap = mpFontController->GetFontCharMap(mpGraphics);

    return !rxFontCharMap->IsDefaultMap();
}

bool OutputDevice::GetFontCapabilities(vcl::FontCapabilities& rFontCapabilities) const
{
    if (!mpGraphics && !const_cast<OutputDevice*>(this)->AcquireGraphics())
        return false;
    return mpFontController && mpFontController->GetFontCapabilities(mpGraphics, rFontCapabilities);
}

tools::Long OutputDevice::GetFontExtLeading() const
{
    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont->mxFontMetric->GetExternalLeading();

    return 0;
}

void OutputDevice::ImplClearFontData(const bool bNewFontLists)
{
    // the currently selected logical font is no longer needed
    mpFontInstance.clear();

    mbFontDirty = true;
    mbNewFont = true;

    if (bNewFontLists)
        mpFontFaceCollection.reset();

    SalGraphics* pGraphics = nullptr;
    if (AcquireGraphics())
        pGraphics = mpGraphics;

    mpFontController->ClearFontResources(pGraphics, bNewFontLists);
}

void OutputDevice::RefreshFontData(const bool bNewFontLists) { ImplRefreshFontData(bNewFontLists); }

void OutputDevice::ImplRefreshFontData(const bool bNewFontLists)
{
    if (bNewFontLists && AcquireGraphics())
        mpFontController->RefreshFromGraphics(mpGraphics);
}

void OutputDevice::ImplUpdateFontData()
{
    SalGraphics* pGraphics = nullptr;
    if (AcquireGraphics())
        pGraphics = mpGraphics;

    mpFontController->UpdateFontData(pGraphics, true);
}

void OutputDevice::ImplClearFontsOnAllFrames(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplClearFontData, bNewFontLists);
}

void OutputDevice::ImplClearAllFontData(bool bNewFontLists)
{
    // Forwarding for legacy calls
    vcl::font::FontController::ClearAllFontData(bNewFontLists);
}


void OutputDevice::ImplRefreshFontsOnAllFrames(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplRefreshFontData, bNewFontLists);
}

void OutputDevice::ImplRefreshAllFontData(bool bNewFontLists)
{
     // Forwarding for legacy calls
     vcl::font::FontController::RefreshAllFontData(bNewFontLists);
}


void OutputDevice::ImplUpdateAllFontData(bool bNewFontLists) { vcl::font::FontController::UpdateAllFontData(bNewFontLists); }


void OutputDevice::ImplUpdateFirstFrameGraphics()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    if (!pFrame)
        return;

    if (pFrame->GetOutDev()->AcquireGraphics())
    {
        OutputDevice* pDevice = pFrame->GetOutDev();

        pDevice->mpFontController->UpdateSystemFontList(pDevice->mpGraphics);}
}

void OutputDevice::ImplUpdateFontDataForAllFrames(const FontUpdateHandler_t pHdl,
                                                  const bool bNewFontLists)
{
    ImplSVData* const pSVData = ImplGetSVData();

    // update all windows
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    while (pFrame)
    {
        (pFrame->GetOutDev()->*pHdl)(bNewFontLists);

        vcl::Window* pSysWin = pFrame->mpWindowImpl->mpFrameData->mpFirstOverlap;
        while (pSysWin)
        {
            (pSysWin->GetOutDev()->*pHdl)(bNewFontLists);
            pSysWin = pSysWin->mpWindowImpl->mpNextOverlap;
        }

        pFrame = pFrame->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // update all virtual devices
    VirtualDevice* pVirDev = pSVData->maGDIData.mpFirstVirDev;
    while (pVirDev)
    {
        (pVirDev->*pHdl)(bNewFontLists);
        pVirDev = pVirDev->mpNext;
    }

    // update all printers
    Printer* pPrinter = pSVData->maGDIData.mpFirstPrinter;
    while (pPrinter)
    {
        (pPrinter->*pHdl)(bNewFontLists);
        pPrinter = pPrinter->mpNext;
    }
}

void OutputDevice::BeginFontSubstitution() { vcl::font::FontController::BeginFontSubstitution(); }

void OutputDevice::EndFontSubstitution() { vcl::font::FontController::EndFontSubstitution(); }

void OutputDevice::AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName, AddFontSubstituteFlags nFlags)
{
    vcl::font::FontController::AddFontSubstitute(rFontName, rReplaceFontName, nFlags);
}

void OutputDevice::RemoveFontsSubstitute() { vcl::font::FontController::RemoveFontsSubstitute(); }

//hidpi TODO: This routine has hard-coded font-sizes that break places such as DialControl
vcl::Font OutputDevice::GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                       GetDefaultFontFlags nFlags, const OutputDevice* pOutDev)
{
    return vcl::font::FontController::GetDefaultFont(nType, eLang, nFlags, pOutDev);
}

void OutputDevice::ImplInitFontList() const
{
    if (mpFontController)
        mpFontController->InitializeFonts(mpGraphics);
}

bool OutputDevice::InitFont() const
{
    DBG_TESTSOLARMUTEX();

    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    if (mpFontController->NeedsUpdate(mpGraphicsState->maFont, mbNewFont))
    {
        if (!const_cast<OutputDevice*>(this)->ImplNewFont())
            return false;
    }

    LogicalFontInstance* pFontToUse = nullptr;

    if (mpFontRealization && mpFontRealization->mxFont)
        pFontToUse = mpFontRealization->mxFont.get();
    else if (mpFontInstance)
        pFontToUse = mpFontInstance.get();

    if (!pFontToUse)
        return false;

    if (!mpGraphics)
    {
        if (!const_cast<OutputDevice*>(this)->AcquireGraphics())
            return false;
    }
    else if (!mbFontDirty)
    {
        return true;
    }

    if (mpGraphics && mpFontController->ActivateFontOnDevice(mpGraphics, pFontToUse))
    {
        mbFontDirty = false;
        return true;
    }

    return false;
}

const LogicalFontInstance* OutputDevice::GetFontInstance() const
{
    if (!InitFont())
        return nullptr;

    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont.get();

    return mpFontInstance.get();
}

bool OutputDevice::ImplNewFont() const
{
    DBG_TESTSOLARMUTEX();

    SAL_INFO("vcl.gdi", "ImplNewFont: Start. Dirty=" << mbFontDirty << " New=" << mbNewFont);

    if (!mbNewFont)
        return true;

    if (!mpGraphics && !AcquireGraphics())
    {
        SAL_WARN("vcl.gdi", "OutputDevice::ImplNewFont(): no Graphics, no Font");
        return false;
    }
    assert(mpGraphics);

    ImplInitFontList();

    auto[fExactHeight, aSize]
        = mpFontController->CalculateDeviceSize(mpGraphicsState->maFont, *mpMapper, GetDPIY());

    if (mpFontController->NeedsOLEFontScaleFix(*mpMapper, aSize))
        aSize = mpFontController->GetOLECorrectedSize(*mpMapper, aSize, aSize.Height());

    const bool bNonAntialiased = mpFontController->ShouldDisableAntialiasing(
        GetAntialiasing(), GetSettings().GetStyleSettings(),
        mpGraphicsState->maFont.GetFontSize().Height());

    rtl::Reference<LogicalFontInstance> pOldFontInstance = mpFontInstance;
    mpFontInstance = mpFontController->RealizeFont(GetFontCollection(), mpGraphicsState->maFont, mpGraphics,
                                                   aSize, fExactHeight, bNonAntialiased);

    if (!mpFontInstance)
    {
        SAL_WARN("vcl.gdi", "ImplNewFont: !!! NO FONT INSTANCE FOUND for request !!!");
    }

    // We must update the struct *before* calling InitFont
    if (mpFontRealization)
        mpFontRealization->mxFont = mpFontInstance;

    const bool bNewFontInstance = pOldFontInstance.get() != mpFontInstance.get();
    pOldFontInstance.clear();

    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    if (!pFontInstance)
        return false;

    // Compute font size in points for optical sizing.
    if (!pFontInstance->GetPointSize())
    {
        auto nHeight = GetFont().GetFontHeight();
        auto eFrom = MapToO3tlLength(GetMapMode().GetMapUnit());
        float fPointSize = o3tl::convert(float(nHeight), eFrom, o3tl::Length::pt);
        pFontInstance->SetPointSize(fPointSize);
    }

    // mark when lower layers need to get involved
    mbNewFont = false;
    if (bNewFontInstance)
        mbFontDirty = true;

    ImplInitializeFontInstance(pFontInstance);

    std::tie(mpFontRealization->nXOffset, mpFontRealization->nYOffset,
             mpFontRealization->nEmphasisAscent, mpFontRealization->nEmphasisDescent)
        = mpFontController->CalculateTextOffsets(mpGraphicsState->maFont, mpFontInstance.get());

    // Use local temporary variables to bypass the bit-field reference restriction
    bool bTextLines = false;
    bool bTextSpecial = false;

    std::tie(bTextLines, bTextSpecial)
        = mpFontController->GetTextLayoutFlags(mpGraphicsState->maFont);

    if (mpFontRealization)
    {
        mpFontRealization->mxFont = mpFontInstance;
        mpFontRealization->bHasLineDecorations = bTextLines;
        mpFontRealization->bHasSpecialEffects = bTextSpecial;
        mpFontRealization->eLayoutMode = mpGraphicsState->mnTextLayoutMode;
    }

    return true;
}


void OutputDevice::ImplInitializeFontInstance(LogicalFontInstance* pFontInstance) const
{
    if (!pFontInstance->mbInit && InitFont())
    {
        mpFontController->InitializeInstance(pFontInstance, mpGraphics);
        ImplInitFontMetrics(pFontInstance);
        SetFontOrientation(pFontInstance);
    }
}

void OutputDevice::ImplInitFontMetrics(LogicalFontInstance* pFontInstance) const
{
    {
        const vcl::Font& rFont = GetFont();
        tools::Long nDPIY = GetDPIY();

        tools::Long nBulletOffset = (GetTextWidth(OUString(u' ')) - GetTextWidth(u"\x00b7"_ustr)) >> 1;
        pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);

        tools::Long nPixelWidth = LogicToPixel(Size(1, 0)).Width();
        pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nPixelWidth);

        bool bCentered = true;
        if (MsLangId::isCJK(rFont.GetLanguage()))
        {
            tools::Rectangle aRect;
            GetTextBoundRect( aRect, u"\x3001"_ustr ); // Fullwidth fullstop
            const auto nH = rFont.GetFontSize().Height();
            const auto nB = aRect.Left();
            // Use 18.75% as a threshold to define a centered fullwidth fullstop.
            // In general, nB/nH < 5% for most Japanese fonts.
            bCentered = nB > (((nH >> 1)+nH)>>3);
        }
        pFontInstance->mxFontMetric->SetFullstopCenteredFlag(bCentered);
    }

    pFontInstance->mnLineHeight
        = pFontInstance->mxFontMetric->GetAscent() + pFontInstance->mxFontMetric->GetDescent();
}

void OutputDevice::SetFontOrientation(LogicalFontInstance* const pFontInstance) const
{
    if (pFontInstance->GetFontSelectPattern().mnOrientation
        && !pFontInstance->mxFontMetric->GetOrientation())
    {
        pFontInstance->mnOwnOrientation = pFontInstance->GetFontSelectPattern().mnOrientation;
        pFontInstance->mnOrientation = pFontInstance->mnOwnOrientation;
    }
    else
    {
        pFontInstance->mnOrientation = pFontInstance->mxFontMetric->GetOrientation();
    }
}

void OutputDevice::ImplDrawEmphasisMark(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                        const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                        const tools::Rectangle& rRect1,
                                        const tools::Rectangle& rRect2)
{
    if (IsRTLEnabled())
        nX = nBaseX - (nX - nBaseX - 1);

    nX -= GetOutOffXPixel();
    nY -= GetOutOffYPixel();

    if (rPolyPoly.Count())
    {
        if (bPolyLine)
        {
            tools::Polygon aPoly = rPolyPoly.GetObject(0);
            aPoly.Move(nX, nY);
            DrawPolyLine(aPoly);
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move(nX, nY);
            DrawPolyPolygon(aPolyPoly);
        }
    }

    if (!rRect1.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect1.Left(), nY + rRect1.Top()), rRect1.GetSize());
        DrawRect(aRect);
    }

    if (!rRect2.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect2.Left(), nY + rRect2.Top()), rRect2.GetSize());
        DrawRect(aRect);
    }
}

void OutputDevice::ImplDrawEmphasisMarks(SalLayout& rSalLayout)
{
    vcl::font::FontRealization const* pRealization = mpFontRealization.get();
    if (!pRealization || !pRealization->mxFont)
        return;

    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR | vcl::PushFlags::MAPMODE);
    GDIMetaFile* pOldMetaFile = mpMetaFile;
    mpMetaFile = nullptr;
    mpMapper->EnableMapMode(false);

    FontEmphasisMark nEmphasisMark = mpGraphicsState->maFont.GetEmphasisMarkStyle();
    tools::Long nEmphasisHeight;

    if (nEmphasisMark & FontEmphasisMark::PosBelow)
        nEmphasisHeight = pRealization->nEmphasisDescent;
    else
        nEmphasisHeight = pRealization->nEmphasisAscent;

    vcl::font::EmphasisMark aEmphasisMark(nEmphasisMark, nEmphasisHeight, GetDPIY());

    if (aEmphasisMark.IsShapePolyLine())
    {
        SetLineColor(GetTextColor());
        SetFillColor();
    }
    else
    {
        SetLineColor();
        SetFillColor(GetTextColor());
    }

    Point aOffset(0, 0);
    Point aOffsetVert(0, 0);

    if (nEmphasisMark & FontEmphasisMark::PosBelow)
    {
        aOffset.AdjustY(pRealization->mxFont->mxFontMetric->GetDescent() + aEmphasisMark.GetYOffset());
        aOffsetVert = aOffset;
    }
    else
    {
        aOffset.AdjustY(-(pRealization->mxFont->mxFontMetric->GetAscent() + aEmphasisMark.GetYOffset()));
        aOffsetVert.AdjustY(-(pRealization->mxFont->mxFontMetric->GetAscent()
                              + pRealization->mxFont->mxFontMetric->GetDescent()
                              + aEmphasisMark.GetYOffset()));
    }

    tools::Long nEmphasisWidth2 = aEmphasisMark.GetWidth() / 2;
    tools::Long nEmphasisHeight2 = nEmphasisHeight / 2;
    aOffset += Point(nEmphasisWidth2, nEmphasisHeight2);

    basegfx::B2DPoint aOutPoint;
    basegfx::B2DRectangle aRectangle;
    const GlyphItem* pGlyph;
    const LogicalFontInstance* pGlyphFont;
    int nStart = 0;
    while (rSalLayout.GetNextGlyph(&pGlyph, aOutPoint, nStart, &pGlyphFont))
    {
        if (!pGlyph->GetGlyphBoundRect(pGlyphFont, aRectangle))
            continue;

        if (!pGlyph->IsSpacing())
        {
            Point aAdjPoint;
            if (pGlyph->IsVertical())
            {
                aAdjPoint = aOffsetVert;
                aAdjPoint.AdjustX((-pGlyph->origWidth() + aEmphasisMark.GetWidth()) / 2);
            }
            else
            {
                aAdjPoint = aOffset;
                aAdjPoint.AdjustX(aRectangle.getMinX() + (aRectangle.getWidth() - aEmphasisMark.GetWidth()) / 2);
            }

            if (pRealization->mxFont->mnOrientation)
            {
                Point aOriginPt(0, 0);
                aOriginPt.RotateAround(aAdjPoint, pRealization->mxFont->mnOrientation);
            }
            aOutPoint.adjustX(aAdjPoint.X() - nEmphasisWidth2);
            aOutPoint.adjustY(aAdjPoint.Y() - nEmphasisHeight2);
            ImplDrawEmphasisMark(rSalLayout.DrawBase().getX(), aOutPoint.getX(), aOutPoint.getY(),
                                 aEmphasisMark.GetShape(), aEmphasisMark.IsShapePolyLine(),
                                 aEmphasisMark.GetRect1(), aEmphasisMark.GetRect2());
        }
    }

    mpMetaFile = pOldMetaFile;
}

bool OutputDevice::ForceFallbackFont(vcl::Font const& rFallbackFont)
{
    vcl::Font aOldFont = GetFont();
    SetFont(rFallbackFont);
    if (!InitFont())
        return false;

    mpForcedFallbackInstance = mpFontInstance;
    SetFont(aOldFont);
    if (!InitFont())
        return false;

    if (mpForcedFallbackInstance)
        return true;

    return false;
}

tools::Long OutputDevice::GetMinKashida() const
{
    if (!ImplNewFont())
        return 0;

    double nKashidaWidth = mpFontController->GetMinKashidaWidth(mpFontRealization->mxFont.get());

    if (!mpMapper->IsMapModeEnabled())
        nKashidaWidth = std::ceil(nKashidaWidth);

    return mpMapper->DevicePixelToLogicWidth(nKashidaWidth);
}

// tdf#163105: Get map of valid kashida positions for a single word
void OutputDevice::GetWordKashidaPositions(const OUString& rText, std::vector<bool>* pOutMap) const
{
    pOutMap->clear();
    auto nEnd = rText.getLength();
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rText, 0, nEnd);
    if (!pSalLayout || !pSalLayout->HasFontKashidaPositions())
        return;

    pOutMap->resize(nEnd, false);
    for (sal_Int32 i = 0; i < nEnd; ++i)
    {
        auto nNextPos = i + 1;
        while (nNextPos < nEnd && u_getIntPropertyValue(rText[nNextPos], UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT)
        {
            ++nNextPos;
        }

        pOutMap->at(i) = pSalLayout->IsKashidaPosValid(i, nNextPos);
    }
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
        if (!GetTextBoundRect(aRect, rStr, nIndex, nIndex + i, 1))
            break;

        aRect.Move(rOrigin.X(), rOrigin.Y());
        rVector.push_back(aRect);
    }

    return (nLen == static_cast<int>(rVector.size()));
}

sal_Int32 OutputDevice::HasGlyphs(const vcl::Font& rTempFont, std::u16string_view rStr,
                                  sal_Int32 nIndex, sal_Int32 nLen) const
{
    if (nIndex >= static_cast<sal_Int32>(rStr.size())) return nIndex;
    sal_Int32 nEnd = (nLen == -1) ? rStr.size() : std::min<sal_Int32>(rStr.size(), nIndex + nLen);

    const vcl::Font aOrigFont = GetFont();
    const_cast<OutputDevice&>(*this).SetFont(rTempFont);
    FontCharMapRef xFontCharMap;
    bool bRet = GetFontCharMap(xFontCharMap);
    const_cast<OutputDevice&>(*this).SetFont(aOrigFont);

    if (!bRet) return nIndex;
    for (sal_Int32 i = nIndex; nIndex < nEnd; ++i, ++nIndex)
    {
        if (!xFontCharMap->HasChar(rStr[i]))
            return nIndex;
    }

    return -1;
}

void OutputDevice::ReleaseFontCache()
{
    if (mpFontController)
        mpFontController->mxFontCache.reset();
}

void OutputDevice::ReleaseFontCollection()
{
    SetFontCollection(nullptr);
}

void OutputDevice::SetFontCollectionFromSVData()
{
    SetFontCollection(ImplGetSVData()->maGDIData.mxScreenFontList->Clone());
}

void OutputDevice::ResetNewFontCache()
{
    if (mpFontController)
        mpFontController->mxFontCache = std::make_shared<ImplFontCache>();
}

void OutputDevice::ImplReleaseFonts()
{
    // Delegate resource cleanup to controller
    // Note: We pass nullptr for graphics because ReleaseGraphics calls this *after* // or *during* release, but mostly we want to clear the *cache* and *lists*.
    // However, original code called mpGraphics->ReleaseFonts().
    // If mpGraphics is valid here, we should pass it.
    // Usually ImplReleaseFonts is called inside ReleaseGraphics(bool bRelease).

    mpFontController->ClearFontResources(mpGraphics, true);

    // OutputDevice state cleanup
    mbNewFont = true;
    mbFontDirty = true;
    mpForcedFallbackInstance.clear();

    // Legacy member cleanup (if still used)
    mpFontFaceCollection.reset();
}
tools::Long OutputDevice::GetEmphasisAscent() const
{
    return mpFontRealization ? mpFontRealization->nEmphasisAscent : 0;
}

tools::Long OutputDevice::GetEmphasisDescent() const
{
    return mpFontRealization ? mpFontRealization->nEmphasisDescent : 0;
}

void OutputDevice::AdoptSharedFontCache(const std::shared_ptr<ImplFontCache>& pShared)
{
    if (mpFontController)
        mpFontController->AdoptCache(pShared);
}

void OutputDevice::InvalidateFontCache()
{
    if (mpFontController)
        mpFontController->InvalidateCache();
}

void OutputDevice::ClearFontCache()
{
    if (mpFontController)
        mpFontController->ClearCache();
}

void OutputDevice::ResetFontCache()
{
    if (!mpFontController)
        mpFontController = std::make_unique<vcl::font::FontController>();

    mpFontController->ResetCache();
}

ImplFontCache& OutputDevice::GetFontCache() const
{
    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    return mpFontController->GetCache();
}

rtl::Reference<LogicalFontInstance> OutputDevice::GetFontInstance(vcl::font::PhysicalFontCollection* pPFC, const vcl::Font& rFont, const Size& rSize, float fHeight) const
{
    return GetFontCache().GetFontInstance(pPFC, rFont, rSize, fHeight);
}

void OutputDevice::AcquireScreenFontCache()
{
    AdoptSharedFontCache(ImplGetSVData()->maGDIData.mxScreenFontCache);
}

bool OutputDevice::IsScreenFontCache() const
{
    return &GetFontCache() == ImplGetSVData()->maGDIData.mxScreenFontCache.get();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
