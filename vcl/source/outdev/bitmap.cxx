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

#include <config_features.h>

#include <osl/diagnose.h>
#include <tools/debug.hxx>
#include <tools/helpers.hxx>

#include <vcl/deviceconcepts.hxx>
#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/image.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/virdev.hxx>
#include <vcl/BitmapWriteAccess.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <bitmap/bmpfast.hxx>
#include <devicedispatcher.hxx>
#include <drawmode.hxx>
#include <salbmp.hxx>
#include <salgdi.hxx>

void OutputDevice::DrawBitmap(const Point& rDestPt, const Bitmap& rBitmap)
{
    assert(!is_double_buffered_window());

    const Size aSizePix(rBitmap.GetSizePixel());
    MetaActionType nAction = rBitmap.HasAlpha() ? MetaActionType::BMPEX : MetaActionType::BMP;

    DrawBitmap(rDestPt, PixelToLogic(aSizePix), Point(), aSizePix, rBitmap, nAction);
}

void OutputDevice::DrawBitmap(const Point& rDestPt, const Size& rDestSize, const Bitmap& rBitmap)
{
    assert(!is_double_buffered_window());

    MetaActionType nAction = rBitmap.HasAlpha() ? MetaActionType::BMPEXSCALE : MetaActionType::BMPSCALE;

    DrawBitmap(rDestPt, rDestSize, Point(), rBitmap.GetSizePixel(), rBitmap, nAction);
}

void OutputDevice::DrawBitmap( const Point& rDestPt, const Size& rDestSize,
                               const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                               const Bitmap& rBitmap)
{
    assert(!is_double_buffered_window());

    // Preserve the legacy Metafile action type difference, but route
    // everything into the unified master dispatcher.
    MetaActionType nAction = rBitmap.HasAlpha() ? MetaActionType::BMPEXSCALEPART
                                                : MetaActionType::BMPSCALEPART;

    DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, nAction);
}

void OutputDevice::DrawBitmap( const Point& rDestPt, const Size& rDestSize,
                               const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                               const Bitmap& rBitmap, const MetaActionType nAction )
{
    assert(!is_double_buffered_window());

    if( IsLayoutCalculationNecessary() )
        return;

    if ( RasterOp::Invert == mpGraphicsState->meRasterOp )
    {
        DrawRect( tools::Rectangle( rDestPt, rDestSize ) );
        return;
    }

    if (mpGraphicsState->mnDrawMode & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap))
    {
        sal_uInt8 cCmpVal;
        if (mpGraphicsState->mnDrawMode & DrawModeFlags::BlackBitmap)
            cCmpVal = 0;
        else
            cCmpVal = 255;

        Color aCol(cCmpVal, cCmpVal, cCmpVal);
        auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR);
        SetLineColor(aCol);
        SetFillColor(aCol);
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    Bitmap aBmp(rBitmap);

    if (mpGraphicsState->mnDrawMode & DrawModeFlags::GrayBitmap && !aBmp.IsEmpty())
        aBmp.Convert(BmpConversion::N8BitGreys);

    maRecorder.RecordBitmapAction(nAction, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmp);

    if ( !IsDeviceOutputNecessary() )
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    if (aBmp.IsEmpty())
      return;

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                       mpMapper->LogicXToDevicePixel(rDestPt.X()), mpMapper->LogicYToDevicePixel(rDestPt.Y()),
                       mpMapper->LogicWidthToDevicePixel(rDestSize.Width()),
                       mpMapper->LogicHeightToDevicePixel(rDestSize.Height()));

    if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
        return;

    // Normalize coordinates and flip bitmap payload if logical size was negative
    const BmpMirrorFlags nMirrFlags = AdjustTwoRect( aPosAry, aBmp.GetSizePixel() );

    if ( nMirrFlags != BmpMirrorFlags::NONE )
        aBmp.Mirror( nMirrFlags );

    if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
        return;

    // Subsampling (High-quality downscale)
    if (nAction == MetaActionType::BMPSCALE && CanSubsampleBitmap())
    {
        double nScaleX = aPosAry.mnDestWidth  / static_cast<double>(aPosAry.mnSrcWidth);
        double nScaleY = aPosAry.mnDestHeight / static_cast<double>(aPosAry.mnSrcHeight);

        // hidpi surfaces like cairo have their own scale, so don't downscale
        // past the surface scaling which can retain the extra detail
        double fScale(1.0);
        if (mpGraphics->ShouldDownscaleIconsAtSurface(fScale))
        {
            nScaleX *= fScale;
            nScaleY *= fScale;
        }

        if ( nScaleX < 1.0 || nScaleY < 1.0 )
        {
            aBmp.Scale(nScaleX, nScaleY);
            aPosAry.mnSrcWidth = aPosAry.mnDestWidth * fScale;
            aPosAry.mnSrcHeight = aPosAry.mnDestHeight * fScale;
        }
    }

    const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    if (bRTL)
    {
        tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
        tools::Rectangle aDestRect(Point(aPosAry.mnDestX, aPosAry.mnDestY), Size(aPosAry.mnDestWidth, aPosAry.mnDestHeight));
        mpMapper->MirrorDevicePixelRect(aDestRect, nFrameWidth, bRTL, ImplIsAntiparallel());
        aPosAry.mnDestX = aDestRect.Left();
        aPosAry.mnDestY = aDestRect.Top();
    }

    vcl::rendercontext::BitmapRenderer::DrawBitmap(*mpGraphics, aPosAry, aBmp);
}

void OutputDevice::DrawDeviceBitmap( const Point& rDestPt, const Size& rDestSize,
                                     const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                     Bitmap& rBitmap )
{
    assert(!is_double_buffered_window());

    if (rBitmap.IsEmpty())
        return;

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                       mpMapper->LogicXToDevicePixel(rDestPt.X()), mpMapper->LogicYToDevicePixel(rDestPt.Y()),
                       mpMapper->LogicWidthToDevicePixel(rDestSize.Width()),
                       mpMapper->LogicHeightToDevicePixel(rDestSize.Height()));

    if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
        return;

    // Mutates the referenced bitmap (which is safely a local copy from the orchestrator)
    const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, rBitmap.GetSizePixel());
    if (nMirrFlags != BmpMirrorFlags::NONE)
        rBitmap.Mirror(nMirrFlags);

    if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
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

        vcl::rendercontext::BitmapRenderer::DrawBitmap(*mpGraphics, aPosAry, rBitmap);
    }
}

Bitmap OutputDevice::GetBitmap( const Point& rSrcPt, const Size& rSize ) const
{
    if ( !mpGraphics && !AcquireGraphics() )
        return Bitmap();

    assert(mpGraphics);

    tools::Long    nX = LogicXToDevicePixel( rSrcPt.X() );
    tools::Long    nY = LogicYToDevicePixel( rSrcPt.Y() );
    tools::Long nWidth = LogicWidthToDevicePixel(rSize.Width());
    tools::Long nHeight = LogicHeightToDevicePixel(rSize.Height());
    if ( nWidth <= 0 || nHeight <= 0 || nX > (GetOutputWidthPixel() + GetOutOffXPixel()) || nY > (GetOutputHeightPixel() + GetOutOffYPixel()))
        return Bitmap();

    tools::Rectangle   aRect( Point( nX, nY ), Size( nWidth, nHeight ) );
    bool bClipped = false;

    // X-Coordinate outside of draw area?
    if ( nX < GetOutOffXPixel() )
    {
        nWidth -= ( GetOutOffXPixel() - nX );
        nX = GetOutOffXPixel();
        bClipped = true;
    }

    // Y-Coordinate outside of draw area?
    if ( nY < GetOutOffYPixel() )
    {
        nHeight -= ( GetOutOffYPixel() - nY );
        nY = GetOutOffYPixel();
        bClipped = true;
    }

    // Width outside of draw area?
    if ( (nWidth + nX) > (GetOutputWidthPixel() + GetOutOffXPixel()) )
    {
        nWidth  = GetOutOffXPixel() + GetOutputWidthPixel() - nX;
        bClipped = true;
    }

    // Height outside of draw area?
    if ( (nHeight + nY) > (GetOutputHeightPixel() + GetOutOffYPixel()) )
    {
        nHeight = GetOutOffYPixel() + GetOutputHeightPixel() - nY;
        bClipped = true;
    }

    if (bClipped)
    {
        // If the visible part has been clipped, we have to create a
        // Bitmap with the correct size in which we copy the clipped
        // Bitmap to the correct position.
        ScopedVclPtrInstance< VirtualDevice > aVDev(  *this  );

        if ( aVDev->SetOutputSizePixel( aRect.GetSize() ) )
        {
            if ( aVDev->mpGraphics || aVDev->AcquireGraphics() )
            {
                if ( (nWidth > 0) && (nHeight > 0) )
                {
                    SalTwoRect aPosAry(nX, nY, nWidth, nHeight,
                                      (aRect.Left() < GetOutOffXPixel()) ? (GetOutOffXPixel() - aRect.Left()) : 0L,
                                      (aRect.Top() < GetOutOffYPixel()) ? (GetOutOffYPixel() - aRect.Top()) : 0L,
                                      nWidth, nHeight);
                    aVDev->mpGraphics->CopyBits(aPosAry, *mpGraphics, *this, *this);
                }
                else
                {
                    OSL_ENSURE(false, "CopyBits with zero or negative width or height");
                }

                return aVDev->GetBitmap( Point(), aVDev->GetOutputSizePixel() );
            }
        }
    }

    std::shared_ptr<SalBitmap> pSalBmp;
    // if we are a virtual device, we might need to remove the unused alpha channel
    bool bWithoutAlpha = false;
    if (OUTDEV_VIRDEV == GetOutDevType())
        bWithoutAlpha = static_cast<const VirtualDevice*>(this)->IsWithoutAlpha();

    pSalBmp = mpGraphics->GetBitmap( nX, nY, nWidth, nHeight, *this, bWithoutAlpha );

    Bitmap aBmp;

    if( pSalBmp )
        aBmp.ImplSetSalBitmap(pSalBmp);

    return aBmp;
}

bool OutputDevice::HasFastDrawTransformedBitmap() const
{
    if( IsLayoutCalculationNecessary() )
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;
    assert(mpGraphics);

    return mpGraphics->HasFastDrawTransformedBitmap();
}

bool OutputDevice::DrawDeviceTransformedBitmap(
    const basegfx::B2DHomMatrix& aFullTransform,
    const Bitmap& rBitmap,
    double fAlpha)
{
    assert(!is_double_buffered_window());

    if (rBitmap.IsEmpty())
        return true;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    return vcl::DispatchDevice(*this, [&](auto& rConcreteDevice) -> bool {

        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        // Logical records don't have "Fast" hardware paths; they record the matrix directly.
        if constexpr (vcl::LogicalRecorder<DeviceType>)
        {
            // PDFWriterImpl overrides the drawing methods to record the
            // transformation matrix as a vector object.
            return false; // Fall back to standard recording machinery
        }

        if constexpr (vcl::HWAccelerated<DeviceType>)
        {
            // Calculate the three points defining the transformed parallelogram
            basegfx::B2DPoint aNull(aFullTransform * basegfx::B2DPoint(0.0, 0.0));
            basegfx::B2DPoint aTopX(aFullTransform * basegfx::B2DPoint(1.0, 0.0));
            basegfx::B2DPoint aTopY(aFullTransform * basegfx::B2DPoint(0.0, 1.0));

            const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
            if (bRTL)
            {
                tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
                double fMirrorOrigin = static_cast<double>(nFrameWidth - 1);

                aNull.setX(fMirrorOrigin - aNull.getX());
                aTopX.setX(fMirrorOrigin - aTopX.getX());
                aTopY.setX(fMirrorOrigin - aTopY.getX());
            }

            return vcl::rendercontext::BitmapRenderer::DrawTransformedBitmap(
                *mpGraphics, aNull, aTopX, aTopY, rBitmap, fAlpha);
        }

        // fallback (Printers, etc.)
        return false;
    });
}

void OutputDevice::DrawImage( const Point& rPos, const Image& rImage, DrawImageFlags nStyle )
{
    assert(!is_double_buffered_window());

    DrawImage( rPos, Size(), rImage, nStyle );
}

void OutputDevice::DrawImage( const Point& rPos, const Size& rSize,
                              const Image& rImage, DrawImageFlags nStyle )
{
    assert(!is_double_buffered_window());

    if (!IsLayoutCalculationNecessary())
    {
        if (!rSize.IsEmpty())
            rImage.Draw(this, rPos, nStyle, &rSize);
        else
            rImage.Draw(this, rPos, nStyle);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
