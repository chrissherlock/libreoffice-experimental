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

#include <sal/log.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>

#include <outdev.h>
#include <ImplOutDevData.hxx>
#include <salgdi.hxx>

RenderContext2::RenderContext2()
    : mpGraphics(nullptr)
    , mpAlphaVDev(nullptr)
    , mnDrawMode(DrawModeFlags::Default)
    , mnAntialiasing(AntialiasingFlags::NONE)
    , meRasterOp(RasterOp::OverPaint)
    , mxSettings(new AllSettings(Application::GetSettings()))
    , meTextLanguage(LANGUAGE_SYSTEM) // TODO: get default from configuration?
    , maFillColor(COL_WHITE)
    , maTextColor(COL_BLACK)
    , maTextLineColor(COL_TRANSPARENT)
    , maOverlineColor(COL_TRANSPARENT)
    , maRegion(true)
    , mpDeviceFontList(nullptr)
    , mpDeviceFontSizeList(nullptr)
    , mpFontInstance(nullptr)
    , mnTextOffX(0)
    , mnTextOffY(0)
    , mnEmphasisAscent(0)
    , mnEmphasisDescent(0)
    , mbInitFont(true)
    , mbInitFillColor(true)
    , mbInitTextColor(true)
    , mbLineColor(true)
    , mbInitLineColor(true)
    , mbFillColor(true)
    , mbNewFont(true)
    , mbRefPoint(false)
    , mbBackground(false)
    , mbInitClipRegion(true)
    , mbClipRegion(false)
    , mbClipRegionSet(false)
    , mbOutputClipped(false)
    , mbEnableRTL(false) // mirroring must be explicitly allowed (typically for windows only)
    , mbDevOutput(false)
    , mbTextLines(false)
    , mbTextSpecial(false)
    , mpExtOutDevData(nullptr)
    , mbOutput(true)
{
    // #i84553 toop BiDi preference to RTL
    if (AllSettings::GetLayoutRTL())
        mnTextLayoutMode = ComplexTextLayoutFlags::BiDiRtl | ComplexTextLayoutFlags::TextOriginLeft;
    else
        mnTextLayoutMode = ComplexTextLayoutFlags::Default;

    // struct ImplOutDevData- see #i82615#
    mpOutDevData.reset(new ImplOutDevData);
    mpOutDevData->mpRotateDev = nullptr;
    mpOutDevData->mpRecordLayout = nullptr;

    // #i75163#
    mpOutDevData->mpViewTransform = nullptr;
    mpOutDevData->mpInverseViewTransform = nullptr;
}

RenderContext2::~RenderContext2() { disposeOnce(); }

void RenderContext2::dispose()
{
    mpOutDevData->mpRotateDev.disposeAndClear();
    ImplInvalidateViewTransform(); // #i75163#
    mpOutDevData.reset();

    // for some reason, we haven't removed state from the stack properly
    SAL_WARN_IF(!maOutDevStateStack.empty(), "vcl.gdi",
                "RenderContext2::~RenderContext2(): RenderContext2::Push() calls != "
                "RenderContext2::Pop() calls");
    maOutDevStateStack.clear();

    mpFontInstance.clear(); // release the active font instance
    mpDeviceFontList.reset();
    mpDeviceFontSizeList.reset();
    mxFontCache.reset(); // release ImplFontCache specific to this RenderContext2
    mxFontCollection.reset(); // release ImplFontList specific to this RenderContext2

    mpAlphaVDev.disposeAndClear();
    VclReferenceBase::dispose();
}

bool RenderContext2::IsScreenComp() const { return true; }

bool RenderContext2::IsVirtual() const { return false; }

bool RenderContext2::UsesAlphaVDev() const
{
    if (mpAlphaVDev)
        return true;

    return false;
}

bool RenderContext2::ImplIsRecordLayout() const
{
    if (!mpOutDevData)
        return false;

    return mpOutDevData->mpRecordLayout;
}

bool RenderContext2::is_double_buffered_window() const
{
    const vcl::Window* pWindow = dynamic_cast<const vcl::Window*>(this);
    return pWindow && pWindow->SupportsDoubleBuffering();
}

void RenderContext2::DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                                Size const& rSrcSize)
{
    if (ImplIsRecordLayout())
        return;

    if (meRasterOp == RasterOp::Invert)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    tools::Long nSrcWidth = ImplLogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight = ImplLogicHeightToDevicePixel(rSrcSize.Height());
    tools::Long nDestWidth = ImplLogicWidthToDevicePixel(rDestSize.Width());
    tools::Long nDestHeight = ImplLogicHeightToDevicePixel(rDestSize.Height());

    if (nSrcWidth && nSrcHeight && nDestWidth && nDestHeight)
    {
        SalTwoRect aPosAry(ImplLogicXToDevicePixel(rSrcPt.X()), ImplLogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight, ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()), nDestWidth, nDestHeight);

        AdjustTwoRect(aPosAry, GetOutputRectPixel());

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
            mpGraphics->CopyBits(aPosAry, *this);
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawOutDev(rDestPt, rDestSize, rSrcPt, rSrcSize);
}

void RenderContext2::DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                                Size const& rSrcSize, RenderContext2 const& rOutDev)
{
    if (ImplIsRecordLayout())
        return;

    if (RasterOp::Invert == meRasterOp)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (rOutDev.mpAlphaVDev)
    {
        // alpha-blend source over destination
        DrawBitmapEx(rDestPt, rDestSize, rOutDev.GetBitmapEx(rSrcPt, rSrcSize));
    }
    else
    {
        SalGraphics* pSrcGraphics;

        if (RenderContext2 const* pCheckedSrc = DrawOutDevDirectCheck(rOutDev))
        {
            if (!pCheckedSrc->mpGraphics && !pCheckedSrc->AcquireGraphics())
                return;

            pSrcGraphics = pCheckedSrc->mpGraphics;
        }
        else
        {
            pSrcGraphics = nullptr;
        }

        if (!mpGraphics && !AcquireGraphics())
            return;

        assert(mpGraphics);

        // #102532# Offset only has to be pseudo window offset

        SalTwoRect aPosAry(rOutDev.ImplLogicXToDevicePixel(rSrcPt.X()),
                           rOutDev.ImplLogicYToDevicePixel(rSrcPt.Y()),
                           rOutDev.ImplLogicWidthToDevicePixel(rSrcSize.Width()),
                           rOutDev.ImplLogicHeightToDevicePixel(rSrcSize.Height()),
                           ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        AdjustTwoRect(aPosAry, rOutDev.GetOutputRectPixel());

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            // if this is no window, but rSrcDev is a window
            // mirroring may be required
            // because only windows have a SalGraphicsLayout
            // mirroring is performed here
            DrawOutDevDirect(rOutDev, aPosAry, pSrcGraphics);
        }

        // #i32109#: make destination rectangle opaque - source has no alpha
        if (mpAlphaVDev)
            mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
    }
}

void RenderContext2::DrawOutDevDirect(RenderContext2 const& rSrcDev, SalTwoRect& rPosAry,
                                      SalGraphics* pSrcGraphics)
{
    if (pSrcGraphics && (pSrcGraphics->GetLayout() & SalLayoutFlags::BiDiRtl))
    {
        SalTwoRect aPosAry2 = rPosAry;
        pSrcGraphics->mirror(aPosAry2.mnSrcX, aPosAry2.mnSrcWidth, rSrcDev);
        mpGraphics->CopyBits(aPosAry2, *pSrcGraphics, *this, rSrcDev);
        return;
    }

    if (pSrcGraphics)
        mpGraphics->CopyBits(rPosAry, *pSrcGraphics, *this, rSrcDev);
    else
        mpGraphics->CopyBits(rPosAry, *this);
}

RenderContext2 const* RenderContext2::DrawOutDevDirectCheck(RenderContext2 const& rSrcDev) const
{
    return this == &rSrcDev ? nullptr : &rSrcDev;
}

void RenderContext2::CopyArea(Point const& rDestPt, Point const& rSrcPt, Size const& rSrcSize,
                              bool bWindowInvalidate)
{
    if (ImplIsRecordLayout())
        return;

    RasterOp eOldRop = GetRasterOp();
    SetRasterOp(RasterOp::OverPaint);

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    tools::Long nSrcWidth = ImplLogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight = ImplLogicHeightToDevicePixel(rSrcSize.Height());
    if (nSrcWidth && nSrcHeight)
    {
        SalTwoRect aPosAry(ImplLogicXToDevicePixel(rSrcPt.X()), ImplLogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight, ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()), nSrcWidth, nSrcHeight);

        AdjustTwoRect(aPosAry, GetOutputRectPixel());

        CopyDeviceArea(aPosAry, bWindowInvalidate);
    }

    SetRasterOp(eOldRop);

    if (mpAlphaVDev)
        mpAlphaVDev->CopyArea(rDestPt, rSrcPt, rSrcSize, bWindowInvalidate);
}

// Direct OutputDevice drawing protected function

void RenderContext2::CopyDeviceArea(SalTwoRect& aPosAry, bool /*bWindowInvalidate*/)
{
    if (aPosAry.mnSrcWidth == 0 || aPosAry.mnSrcHeight == 0 || aPosAry.mnDestWidth == 0
        || aPosAry.mnDestHeight == 0)
        return;

    aPosAry.mnDestWidth = aPosAry.mnSrcWidth;
    aPosAry.mnDestHeight = aPosAry.mnSrcHeight;
    mpGraphics->CopyBits(aPosAry, *this);
}

Point RenderContext2::GetBandedPageOffset() const { return Point(); }

Size RenderContext2::GetBandedPageSize() const { return GetOutputSizePixel(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
