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
#include <tools/mapunit.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <comphelper/lok.hxx>

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

#include <cstdlib>

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

    if (aBmp.IsEmpty())
        return;

    maRecorder.RecordBitmapAction(nAction, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmp);

    if (!IsDeviceOutputNecessary())
        return;

    // Route directly into the hardware dispatcher!
    DrawDeviceBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmp);
}

void OutputDevice::DrawDeviceBitmap(const Point& rDestPt, const Size& rDestSize,
                                    const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                    const Bitmap& rBitmap)
{
    if (!FlushGraphicsState() || rBitmap.IsEmpty())
        return;

    vcl::DispatchDevice(*this, [&](auto& rConcreteDevice) {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        SalTwoRect aPosAry = mpMapper->ToDeviceRect(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel);
        if (!aPosAry.HasArea())
            return;

        if constexpr (vcl::BandedPrinting<DeviceType>)
        {
             rConcreteDevice.DrawScaledDeviceBitmap(rBitmap, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel);
             return;
        }

        assert(mpGraphics && "Hardware device dispatched without valid SalGraphics!");

        Bitmap aLocalBmp(rBitmap);

        const BmpMirrorFlags nMirr = AdjustTwoRect(aPosAry, aLocalBmp.GetSizePixel());
        if (nMirr != BmpMirrorFlags::NONE)
            aLocalBmp.Mirror(nMirr);

        if constexpr (vcl::SubsamplingCapable<DeviceType>)
            vcl::rendercontext::BitmapRenderer::ApplySubsampling(*mpGraphics, aPosAry, aLocalBmp);

        vcl::rendercontext::BitmapRenderer::DrawBitmap(
            *mpGraphics,
            aPosAry,
            aLocalBmp,
            GetRTLFrameWidth(),
            IsRTLEnabled(),
            vcl::AlphaCapable<DeviceType>
        );
    });
}

static Bitmap lcl_PadClippedBitmap(const Bitmap& rClippedBmp,
                                   const tools::Rectangle& rRequestedRect,
                                   const tools::Rectangle& rClippedRect)
{
    // Create the full-sized canvas the caller originally expected
    Bitmap aFullBmp(rRequestedRect.GetSize(), rClippedBmp.getPixelFormat());

    // Standard VCL behavior: fill background with white
    aFullBmp.Erase(COL_WHITE);

    // Calculate the destination position within the full canvas
    const Point aDestPos(rClippedRect.Left() - rRequestedRect.Left(),
                         rClippedRect.Top() - rRequestedRect.Top());

    const tools::Rectangle aSrcRect(Point(0,0), rClippedBmp.GetSizePixel());
    const tools::Rectangle aDestRect(aDestPos, rClippedBmp.GetSizePixel());

    // Restore the clipped pixels into the padded canvas
    aFullBmp.CopyPixel(aDestRect, aSrcRect, rClippedBmp);

    return aFullBmp;
}

Bitmap OutputDevice::GetBitmap(const Point& rSrcPt, const Size& rSize) const
{
    bool bCanReadPixels = false;
    vcl::DispatchDevice(*this, [&bCanReadPixels](auto& rDev) {
        using DevType = std::decay_t<decltype(rDev)>;

        if constexpr (vcl::ReadableRasterDevice<DevType>)
            bCanReadPixels = true;
    });

    if (!bCanReadPixels)
    {
        SAL_WARN("vcl.gdi", "FATAL LOGIC ERROR: Attempted to read a Bitmap from a device without a readable raster buffer. Halting execution.");
        std::abort();
    }

    if (IsLayoutCalculationNecessary())
        return Bitmap();

    if (!mpGraphics && !AcquireGraphics())
        return Bitmap();

    assert(mpGraphics);

    tools::Long nX = LogicXToDevicePixel(rSrcPt.X());
    tools::Long nY = LogicYToDevicePixel(rSrcPt.Y());
    tools::Long nWidth = LogicWidthToDevicePixel(rSize.Width());
    tools::Long nHeight = LogicHeightToDevicePixel(rSize.Height());

    if (IsPixelAreaOutOfBounds(nX, nY, nWidth, nHeight))
        return Bitmap();

    tools::Rectangle aRequestedRect(Point(nX, nY), Size(nWidth, nHeight));
    tools::Rectangle aDeviceBounds(Point(GetOutOffXPixel(), GetOutOffYPixel()),
                                   Size(GetOutputWidthPixel(), GetOutputHeightPixel()));

    tools::Rectangle aClippedRect = aRequestedRect.Intersection(aDeviceBounds);

    if (aClippedRect.IsEmpty())
        return Bitmap();

    const bool bClipped = (aRequestedRect != aClippedRect);

    tools::Long nFinalX = aClippedRect.Left();
    if (IsRTLEnabled())
        nFinalX = MirrorX(nFinalX, aClippedRect.GetWidth());

    Bitmap aBmp = vcl::rendercontext::BitmapRenderer::CaptureBitmap(
        *mpGraphics, nFinalX, aClippedRect.Top(),
        aClippedRect.GetWidth(), aClippedRect.GetHeight());

    if (aBmp.IsEmpty())
        return aBmp;

    if (bClipped)
        aBmp = lcl_PadClippedBitmap(aBmp, aRequestedRect, aClippedRect);

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
                double fMirrorOrigin = static_cast<double>(GetRTLFrameWidth() - 1);

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

void OutputDevice::DrawScaledAndTranslatedBitmap(
        const basegfx::B2DVector& rScale, const basegfx::B2DVector& rTranslate,
        const Bitmap& rBitmap)
{
    const basegfx::B2DRange aRange(
        rTranslate.getX(),
        rTranslate.getY(),
        rScale.getX() + rTranslate.getX(),
        rScale.getY() + rTranslate.getY());

    const tools::Rectangle aDestRect = mpMapper->RoundDeviceRect(aRange);

    const Point aOrigin = GetMapMode().GetOrigin();
    const bool bIsLOK = !GetConnectMetaFile() && comphelper::LibreOfficeKit::isActive()
                        && GetMapMode().GetMapUnit() != MapUnit::MapPixel;

    Point aFinalDestPt = aDestRect.TopLeft();

    if (bIsLOK)
    {
        aFinalDestPt.Move(aOrigin.getX(), aOrigin.getY());
        EnableMapMode(false);
    }

    DrawBitmap(aFinalDestPt, aDestRect.GetSize(), rBitmap);

    if (bIsLOK)
        EnableMapMode();
}

void OutputDevice::DrawMirroredBitmap(
        const basegfx::B2DVector& rScale, const basegfx::B2DVector& rTranslate,
        const Bitmap& rBitmap)
{
    // Mirrored bitmaps often result from negative scales.
    // We treat them as a transformed range and let RoundDeviceRect
    // and DrawBitmap (via AdjustTwoRect) handle the coordinate flipping.

    const basegfx::B2DRange aRange(
        rTranslate.getX(),
        rTranslate.getY(),
        rScale.getX() + rTranslate.getX(),
        rScale.getY() + rTranslate.getY());

    const tools::Rectangle aDestRect = mpMapper->RoundDeviceRect(aRange);

    DrawBitmap(aDestRect.TopLeft(), aDestRect.GetSize(), rBitmap);
}

/** * Calculates the maximum pixel area allowed for a transformed bitmap
 * to balance memory usage and visual quality.
 */
static double lcl_CalculateMaximumArea(const Size& rOriginalSizePixel)
{
    // The heuristic: Start with 50% of the original area
    const double fOrigArea = static_cast<double>(rOriginalSizePixel.Width()) * rOriginalSizePixel.Height() * 0.5;

    // Scale by 1.44 (roughly 1.2x increase in each dimension) to allow
    // for extra "gutter" space often needed during rotation/shearing.
    const double fOrigAreaScaled = fOrigArea * 1.44;

    // Clamp the result:
    // Min: 1,000,000 pixels (approx 1000x1000)
    // Max: 4,500,000 pixels (approx 2100x2100)
    return std::clamp(fOrigAreaScaled, 1000000.0, 4500000.0);
}

/**
 * Applies a uniform alpha (opacity) value to a bitmap.
 * If the bitmap already has an alpha channel, the new alpha is blended
 * with the existing mask.
 */
static void lcl_ApplyAlpha(Bitmap& rBitmap, double fAlpha)
{
    if (rtl::math::approxEqual(fAlpha, 1.0))
        return;

    // Convert opacity (0.0 to 1.0) to VCL transparency (255 to 0)
    sal_uInt8 nTransparency(static_cast<sal_uInt8>(basegfx::fround(255.0 * (1.0 - fAlpha) + 0.5)));
    AlphaMask aAlpha(rBitmap.GetSizePixel(), &nTransparency);

    if (rBitmap.HasAlpha())
        aAlpha.BlendWith(rBitmap.CreateAlphaMask());

    rBitmap = Bitmap(rBitmap.CreateColorBitmap(), aAlpha);
}

void OutputDevice::DrawTransformedBitmap(
    const basegfx::B2DHomMatrix& rTransformation,
    const Bitmap& rBitmap,
    double fAlpha)
{
    assert(!is_double_buffered_window());

    if (rBitmap.IsEmpty())
        return;

    if( fAlpha == 0.0 )
        return;

    if( IsLayoutCalculationNecessary() )
        return;

    // MM02 compared to other public methods of OutputDevice
    // this test was missing and led to zero-ptr-accesses
    if ( !mpGraphics && !AcquireGraphics() )
        return;

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    /*
       tdf#135325 typically in these OutputDevice methods, for the in
       record-to-metafile case the  MetaFile is already written to before the
       test against mbOutputClipped to determine that output to the current
       device would result in no visual output. In this case the metafile is
       written after the test, so we must continue past mbOutputClipped if
       recording to a metafile. It's typical to record with a device of nominal
       size and play back later against something of a totally different size.
     */
    if (IsOutputCulled() && !GetConnectMetaFile())
        return;

    Bitmap bitmap = rBitmap;

    const bool bInvert(RasterOp::Invert == mpGraphicsState->meRasterOp);
    const bool bBitmapChangedColor(mpGraphicsState->mnDrawMode & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap | DrawModeFlags::GrayBitmap ));
    const bool bTryDirectPaint(!bInvert && !bBitmapChangedColor && !GetConnectMetaFile());

    // tdf#130768 CAUTION(!) using GetViewTransformation() is *not* enough here, it may
    // be that mnOutOffX/mnOutOffY is used - see AOO bug 75163, mentioned at
    // GetDeviceTransformation declaration
    basegfx::B2DHomMatrix aFullTransform(mpMapper->GetDeviceTransformation() * rTransformation);

    // First try to handle additional alpha blending, either directly, or modify the bitmap.
    if (!rtl::math::approxEqual(fAlpha, 1.0) && bTryDirectPaint)
    {
        if (DrawDeviceTransformedBitmap(aFullTransform, bitmap, fAlpha))
            return;
    }

    lcl_ApplyAlpha(bitmap, fAlpha);

    if (bTryDirectPaint && mpGraphics->HasFastDrawTransformedBitmap() && DrawDeviceTransformedBitmap(aFullTransform, bitmap))
        return;

    // decompose matrix to check rotation and shear
    basegfx::B2DVector aScale, aTranslate;
    double fRotate, fShearX;
    rTransformation.decompose(aScale, aTranslate, fRotate, fShearX);

    const bool bRotated(!basegfx::fTools::equalZero(fRotate));
    const bool bSheared(!basegfx::fTools::equalZero(fShearX));
    const bool bMirroredX(aScale.getX() < 0.0);
    const bool bMirroredY(aScale.getY() < 0.0);

    if (!bRotated && !bSheared && !bMirroredX && !bMirroredY)
    {
        DrawScaledAndTranslatedBitmap(aScale, aTranslate, bitmap);
        return;
    }

    if (bTryDirectPaint && DrawDeviceTransformedBitmap(aFullTransform, bitmap))
        return;

    if(!bRotated && !bSheared)
    {
        DrawMirroredBitmap(aScale, aTranslate, bitmap);
        return;
    }

    DrawTransformedBitmapSoftwareFallback(rTransformation, aFullTransform, bitmap, bSheared, bRotated);
}

/**
 * Maps the visible sub-section of a transformed bitmap back to the logical
 * destination, scales/translates it, and returns the final rounded device rectangle.
 */
static tools::Rectangle lcl_CalculateDestRect(
    const basegfx::B2DRange& rVisibleRange,
    const basegfx::B2DHomMatrix& rLogicalTransform,
    const CoordinateMapper& rMapper)
{
    // Calculate the target range by applying the logical transform to a unit square
    basegfx::B2DRange aTargetRange(0.0, 0.0, 1.0, 1.0);
    aTargetRange.transform(rLogicalTransform);

    // Scale/translate the visible sub-section into that target
    basegfx::B2DRange aFinalVisibleRange(rVisibleRange);
    aFinalVisibleRange.transform(
        basegfx::utils::createScaleTranslateB2DHomMatrix(
            aTargetRange.getRange(),
            aTargetRange.getMinimum()));

    return rMapper.RoundDeviceRect(aFinalVisibleRange);
}

void OutputDevice::DrawTransformedBitmapSoftwareFallback(
    const basegfx::B2DHomMatrix& rLogicalTransform,
    const basegfx::B2DHomMatrix& rDeviceTransform,
    const Bitmap& rBitmap,
    bool bSheared, bool bRotated)
{
    assert(bSheared || bRotated);

    const Size aOriginalSizePixel(rBitmap.GetSizePixel());
    double fMaximumArea = lcl_CalculateMaximumArea(aOriginalSizePixel);

    basegfx::B2DRange aVisibleRange(0.0, 0.0, 1.0, 1.0);
    if (!GetVisibleDeviceRange(rDeviceTransform, aVisibleRange, fMaximumArea))
        return;

    if (aVisibleRange.isEmpty())
        return;

    Bitmap aTransformedBmp = vcl::rendercontext::BitmapRenderer::GetTransformedBitmapFallback(
        rBitmap, rDeviceTransform, aVisibleRange, fMaximumArea, bSheared);

    if (aTransformedBmp.IsEmpty())
        return;

    const tools::Rectangle aDestRect = lcl_CalculateDestRect(aVisibleRange, rLogicalTransform, *mpMapper);

    DrawBitmap(aDestRect.TopLeft(), aDestRect.GetSize(), aTransformedBmp);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
