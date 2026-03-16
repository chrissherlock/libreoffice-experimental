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

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
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

void OutputDevice::DrawMask( const Point& rDestPt, const Size& rDestSize,
                             const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                             const Bitmap& rBitmap, const Color& rMaskColor,
                             MetaActionType nAction )
{
    assert(!is_double_buffered_window());

    if (rBitmap.IsEmpty() || IsLayoutCalculationNecessary())
        return;

    if( RasterOp::Invert == mpGraphicsState->meRasterOp )
    {
        DrawRect( tools::Rectangle( rDestPt, rDestSize ) );
        return;
    }

    if (maRecorder.IsRecording())
        maRecorder.RecordMaskAction(nAction, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, rMaskColor);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip))
        return;

    DrawDeviceMask(rBitmap, rMaskColor, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel);
}

void OutputDevice::DrawDeviceMask( const Bitmap& rMask, const Color& rMaskColor,
                                   const Point& rDestPt, const Size& rDestSize,
                                   const Point& rSrcPtPixel, const Size& rSrcSizePixel )
{
    if (rMask.IsEmpty())
        return;

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                       mpMapper->LogicXToDevicePixel(rDestPt.X()), mpMapper->LogicYToDevicePixel(rDestPt.Y()),
                       mpMapper->LogicWidthToDevicePixel(rDestSize.Width()),
                       mpMapper->LogicHeightToDevicePixel(rDestSize.Height()));

    if (!aPosAry.HasArea())
        return;

    // Normalize Coordinates and Handle Flipped Payloads
    Bitmap aBmp(rMask);
    const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aBmp.GetSizePixel());

    if (nMirrFlags != BmpMirrorFlags::NONE)
        aBmp.Mirror(nMirrFlags);

    if (!aPosAry.HasArea())
        return;

    if (mpGraphics)
    {
        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
            tools::Rectangle aDestRect(Point(aPosAry.mnDestX, aPosAry.mnDestY),
                                       Size(aPosAry.mnDestWidth, aPosAry.mnDestHeight));

            mpMapper->MirrorDevicePixelRect(aDestRect, nFrameWidth, bRTL, ImplIsAntiparallel());

            aPosAry.mnDestX = aDestRect.Left();
            aPosAry.mnDestY = aDestRect.Top();
        }

        vcl::rendercontext::BitmapRenderer::DrawMask(*mpGraphics, aPosAry, aBmp, rMaskColor);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
