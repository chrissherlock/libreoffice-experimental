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

#include <vcl/deviceconcepts.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <devicedispatcher.hxx>
#include <salgdi.hxx>
#include <salbmp.hxx>

#include <cassert>

void OutputDevice::DrawMask( const Point& rDestPt,
                             const Bitmap& rBitmap, const Color& rMaskColor )
{
    assert(!is_double_buffered_window());

    const Size aSizePix( rBitmap.GetSizePixel() );
    DrawMask( rDestPt, PixelToLogic( aSizePix ), Point(), aSizePix, rBitmap, rMaskColor, MetaActionType::MASK );
}

void OutputDevice::DrawMask( const Point& rDestPt, const Size& rDestSize,
                             const Bitmap& rBitmap, const Color& rMaskColor )
{
    assert(!is_double_buffered_window());

    DrawMask( rDestPt, rDestSize, Point(), rBitmap.GetSizePixel(), rBitmap, rMaskColor, MetaActionType::MASKSCALE );
}

void OutputDevice::DrawMask( const Point& rDestPt, const Size& rDestSize,
                             const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                             const Bitmap& rBitmap, const Color& rMaskColor)
{

    assert(!is_double_buffered_window());

    DrawMask( rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, rMaskColor, MetaActionType::MASKSCALEPART );
}

void OutputDevice::DrawMask(const Point& rDestPt, const Size& rDestSize,
                            const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                            const Bitmap& rMask, const Color& rMaskColor,
                            MetaActionType nAction)
{
    if (rMask.IsEmpty())
        return;

    if (maRecorder.IsRecording())
        maRecorder.RecordMaskAction(nAction, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rMask, rMaskColor);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip))
        return;

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(),
                       rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                       mpMapper->LogicXToDevicePixel(rDestPt.X()),
                       mpMapper->LogicYToDevicePixel(rDestPt.Y()),
                       mpMapper->LogicWidthToDevicePixel(rDestSize.Width()),
                       mpMapper->LogicHeightToDevicePixel(rDestSize.Height()));

    if (!aPosAry.HasArea())
        return;

    Bitmap aMask(rMask);
    const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aMask.GetSizePixel());
    if (nMirrFlags != BmpMirrorFlags::NONE)
        aMask.Mirror(nMirrFlags);

    if (!aPosAry.HasArea())
        return;

    if (mpGraphics)
    {
        vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
            using DeviceType = std::decay_t<decltype(rConcrete)>;

            const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
            if (bRTL)
            {
                tools::Long nFrameWidth = vcl::get_reference_width_v(rConcrete);
                tools::Rectangle aDestRect(Point(aPosAry.mnDestX, aPosAry.mnDestY),
                                           Size(aPosAry.mnDestWidth, aPosAry.mnDestHeight));

                mpMapper->MirrorDevicePixelRect(aDestRect, nFrameWidth, bRTL, ImplIsAntiparallel());

                aPosAry.mnDestX = aDestRect.Left();
                aPosAry.mnDestY = aDestRect.Top();
            }

            if constexpr (vcl::BandedPrinting<DeviceType>)
                DrawMaskEmulation(aMask, rMaskColor, aPosAry);
            else
                vcl::rendercontext::BitmapRenderer::DrawMask(*mpGraphics, aPosAry, aMask, rMaskColor);
        });
    }
}

void OutputDevice::DrawMaskEmulation(const Bitmap& rMask, const Color& rMaskColor, const SalTwoRect& rPosAry)
{
    // High-level devices might record this differently, but for
    // BandedPrinting, we suspend the metafile to avoid recording the individual rects
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
    bool bOldMap = mpMapper->IsMapModeEnabled();
    mpMapper->EnableMapMode(false);

    Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    SetLineColor(rMaskColor);
    SetFillColor(rMaskColor);

    // Instead of raw arrays, we use the scaling ratios provided in rPosAry
    const vcl::Region aWorkRgn(rMask.CreateRegion(COL_BLACK, tools::Rectangle(Point(), rMask.GetSizePixel())));
    RectangleVector aRectangles;
    aWorkRgn.GetRegionRectangles(aRectangles);

    for (auto const& rect : aRectangles)
    {
        // Calculate the destination coordinates based on the SalTwoRect scaling
        tools::Long nX = rPosAry.mnDestX + (rect.Left() * rPosAry.mnDestWidth / rPosAry.mnSrcWidth);
        tools::Long nY = rPosAry.mnDestY + (rect.Top() * rPosAry.mnDestHeight / rPosAry.mnSrcHeight);
        tools::Long nW = (rect.GetWidth() * rPosAry.mnDestWidth / rPosAry.mnSrcWidth);
        tools::Long nH = (rect.GetHeight() * rPosAry.mnDestHeight / rPosAry.mnSrcHeight);

        DrawRect(tools::Rectangle(Point(nX, nY), Size(nW, nH)));
    }

    Pop();
    mpMapper->EnableMapMode(bOldMap);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
