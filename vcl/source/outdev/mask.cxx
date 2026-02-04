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
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
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
                             const MetaActionType nAction )
{
    assert(!is_double_buffered_window());

    if( IsLayoutCalculationNecessary() )
        return;

    if( RasterOp::Invert == mpGraphicsState->meRasterOp )
    {
        DrawRect( tools::Rectangle( rDestPt, rDestSize ) );
        return;
    }

    switch( nAction )
    {
        case MetaActionType::MASK:
            maRecorder.RecordMask(rDestPt, rBitmap, rMaskColor);
        break;

        case MetaActionType::MASKSCALE:
            maRecorder.RecordMaskScale(rDestPt, rDestSize, rBitmap, rMaskColor);
        break;

        case MetaActionType::MASKSCALEPART:
            maRecorder.RecordMaskScalePart(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, rMaskColor);
        break;

        default: break;
    }

    if ( !IsDeviceOutputNecessary() )
        return;

    if ( !mpGraphics && !AcquireGraphics() )
        return;

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    DrawDeviceMask( rBitmap, rMaskColor, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel );

}

void OutputDevice::DrawDeviceMask( const Bitmap& rMask, const Color& rMaskColor,
                              const Point& rDestPt, const Size& rDestSize,
                              const Point& rSrcPtPixel, const Size& rSrcSizePixel )
{
    assert(!is_double_buffered_window());

    const std::shared_ptr<SalBitmap>& xImpBmp = rMask.ImplGetSalBitmap();
    if (xImpBmp)
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                           LogicXToDevicePixel(rDestPt.X()), LogicYToDevicePixel(rDestPt.Y()),
                           LogicWidthToDevicePixel(rDestSize.Width()),
                           LogicHeightToDevicePixel(rDestSize.Height()));

        // we don't want to mirror via coordinates
        const BmpMirrorFlags nMirrFlags = AdjustTwoRect( aPosAry, xImpBmp->GetSize() );

        // check if output is necessary
        if( aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth && aPosAry.mnDestHeight )
        {

            if( nMirrFlags != BmpMirrorFlags::NONE )
            {
                Bitmap aTmp( rMask );
                aTmp.Mirror( nMirrFlags );
                mpGraphics->DrawMask( aPosAry, *aTmp.ImplGetSalBitmap(),
                                      rMaskColor, *this);
            }
            else
                mpGraphics->DrawMask( aPosAry, *xImpBmp, rMaskColor, *this );

        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
