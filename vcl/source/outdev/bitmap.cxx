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

void OutputDevice::DrawDeviceBitmap( const Point& rDestPt, const Size& rDestSize,
                                     const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                     const Bitmap& rBitmap ) // Keep const, but we will make a local copy if needed
{
    if (!FlushGraphicsState())
        return;

    if (rBitmap.IsEmpty())
        return;

    vcl::DispatchDevice(*this, [&](auto& rConcreteDevice) {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        if constexpr (!vcl::AlphaCapable<DeviceType>)
        {
            if (rBitmap.HasAlpha())
            {
                Bitmap aBlendedBmp(rBitmap.CreateColorBitmap());
                aBlendedBmp.Blend(rBitmap.CreateAlphaMask(), COL_WHITE);
                DrawDeviceBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBlendedBmp);
                return;
            }
        }

        if constexpr (vcl::BandedPrinting<DeviceType>)
        {
             rConcreteDevice.ImplPrintTransparent(rBitmap, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel);
             return;
        }

        if constexpr (vcl::PhysicalDevice<DeviceType>)
        {
            SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(), rSrcSizePixel.Height(),
                               mpMapper->LogicXToDevicePixel(rDestPt.X()), mpMapper->LogicYToDevicePixel(rDestPt.Y()),
                               mpMapper->LogicWidthToDevicePixel(rDestSize.Width()),
                               mpMapper->LogicHeightToDevicePixel(rDestSize.Height()));

            if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
                return;

            // Handle inverted coordinate systems (Negative scaling)
            Bitmap aLocalBmp(rBitmap);
            const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aLocalBmp.GetSizePixel());
            if (nMirrFlags != BmpMirrorFlags::NONE)
                aLocalBmp.Mirror(nMirrFlags);

            if (!aPosAry.mnSrcWidth || !aPosAry.mnSrcHeight || !aPosAry.mnDestWidth || !aPosAry.mnDestHeight)
                return;

            // Subsampling (High-quality downscale)
            if (CanSubsampleBitmap())
            {
                double nScaleX = aPosAry.mnDestWidth  / static_cast<double>(aPosAry.mnSrcWidth);
                double nScaleY = aPosAry.mnDestHeight / static_cast<double>(aPosAry.mnSrcHeight);

                // hidpi surfaces like cairo have their own scale, so don't downscale
                // past the surface scaling which can retain the extra detail
                double fScale(1.0);
                if (mpGraphics && mpGraphics->ShouldDownscaleIconsAtSurface(fScale))
                {
                    nScaleX *= fScale;
                    nScaleY *= fScale;
                }

                if ( nScaleX < 1.0 || nScaleY < 1.0 )
                {
                    aLocalBmp.Scale(nScaleX, nScaleY);
                    aPosAry.mnSrcWidth = aPosAry.mnDestWidth * fScale;
                    aPosAry.mnSrcHeight = aPosAry.mnDestHeight * fScale;
                }
            }

            if (mpGraphics)
            {
                ImplMirrorIfRTL(aPosAry);
                vcl::rendercontext::BitmapRenderer::DrawBitmap(*mpGraphics, aPosAry, aLocalBmp);
            }

            return;
        }
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
        nFinalX = GetOutputWidthPixel() - aClippedRect.GetWidth() - (nFinalX - GetOutOffXPixel()) + GetOutOffXPixel();

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

void OutputDevice::DrawScaledAndTranslatedBitmap(
        const basegfx::B2DVector& rScale, const basegfx::B2DVector& rTranslate,
        const Bitmap& rBitmap)
{
    // with no rotation, shear or mirroring it can be mapped to DrawBitmap
    // do *not* execute the mirroring here, it's done in the fallback
    // #i124580# the correct DestSize needs to be calculated based on MaxXY values
    Point aDestPt(basegfx::fround<tools::Long>(rTranslate.getX()), basegfx::fround<tools::Long>(rTranslate.getY()));

    const Size aDestSize(
        basegfx::fround<tools::Long>(rScale.getX() + rTranslate.getX()) - aDestPt.X(),
        basegfx::fround<tools::Long>(rScale.getY() + rTranslate.getY()) - aDestPt.Y());

    const Point aOrigin = GetMapMode().GetOrigin();

    if (!GetConnectMetaFile() && comphelper::LibreOfficeKit::isActive() && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        aDestPt.Move(aOrigin.getX(), aOrigin.getY());
        EnableMapMode(false);
    }

    DrawBitmap(aDestPt, aDestSize, rBitmap);
    if (!GetConnectMetaFile() && comphelper::LibreOfficeKit::isActive() && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        EnableMapMode();
        aDestPt.Move(-aOrigin.getX(), -aOrigin.getY());
    }
    return;
}

void OutputDevice::DrawMirroredBitmap(
        const basegfx::B2DVector& rScale, const basegfx::B2DVector& rTranslate,
        const Bitmap& rBitmap)
{
    // with no rotation or shear it can be mapped to DrawBitmap
    // do *not* execute the mirroring here, it's done in the fallback
    // #i124580# the correct DestSize needs to be calculated based on MaxXY values
    const Point aDestPt(basegfx::fround<tools::Long>(rTranslate.getX()), basegfx::fround<tools::Long>(rTranslate.getY()));
    const Size aDestSize(
        basegfx::fround<tools::Long>(rScale.getX() + rTranslate.getX()) - aDestPt.X(),
        basegfx::fround<tools::Long>(rScale.getY() + rTranslate.getY()) - aDestPt.Y());

    DrawBitmap(aDestPt, aDestSize, rBitmap);
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
    if (!rtl::math::approxEqual( fAlpha, 1.0))
    {
        if(bTryDirectPaint)
        {
            if (DrawDeviceTransformedBitmap(aFullTransform, bitmap, fAlpha))
            {
                // we are done
                return;
            }
        }
        // Apply the alpha manually.
        sal_uInt8 nTransparency(static_cast<sal_uInt8>(basegfx::fround( 255.0*(1.0 - fAlpha) + .5)));
        AlphaMask aAlpha( bitmap.GetSizePixel(), &nTransparency );

        if (bitmap.HasAlpha())
            aAlpha.BlendWith(bitmap.CreateAlphaMask());

        bitmap = Bitmap( bitmap.CreateColorBitmap(), aAlpha );
    }

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

void OutputDevice::DrawTransformedBitmapSoftwareFallback(
    const basegfx::B2DHomMatrix& rLogicalTransform,
    const basegfx::B2DHomMatrix& rDeviceTransform,
    const Bitmap& rBitmap,
    bool bSheared, bool bRotated)
{
    assert(bSheared || bRotated);

    // limit maximum area to something looking good for non-pixel-based targets
    const Size aOriginalSizePixel(rBitmap.GetSizePixel());
    double fMaximumArea = lcl_CalculateMaximumArea(aOriginalSizePixel);

    basegfx::B2DRange aVisibleRange(0.0, 0.0, 1.0, 1.0);

    if (!GetVisibleDeviceRange(rDeviceTransform, aVisibleRange, fMaximumArea))
        return;

    if (aVisibleRange.isEmpty())
        return;

    Bitmap aTransformed(rBitmap);

    // #122923# add alpha channels for uncovered areas if rotated/sheared
    if(!aTransformed.HasAlpha())
    {
        const Bitmap aContent(aTransformed.CreateColorBitmap());
        AlphaMask aMaskBmp(aContent.GetSizePixel());
        aMaskBmp.Erase(0);
        aTransformed = Bitmap(aContent, aMaskBmp);
    }

    basegfx::B2DVector aFullScale, aFullTranslate;
    double fFullRotate, fFullShearX;

    // We mutate the transform to avoid downscaling, so make a local copy
    basegfx::B2DHomMatrix aDeviceTransform(rDeviceTransform);
    aDeviceTransform.decompose(aFullScale, aFullTranslate, fFullRotate, fFullShearX);

    if (aFullScale.getX() > 0 && aFullScale.getY() > 0
        && aOriginalSizePixel.getWidth() > aFullScale.getX()
        && aOriginalSizePixel.getHeight() > aFullScale.getY())
    {
        basegfx::B2DHomMatrix aTransform = basegfx::utils::createScaleB2DHomMatrix(
                aOriginalSizePixel.getWidth() / aFullScale.getX(),
                aOriginalSizePixel.getHeight() / aFullScale.getY());
        aDeviceTransform *= aTransform;
    }

    double fSourceRatio = 1.0;
    if (aOriginalSizePixel.getHeight() != 0)
        fSourceRatio = aOriginalSizePixel.getWidth() / static_cast<double>(aOriginalSizePixel.getHeight());

    double fTargetRatio = 1.0;
    if (aFullScale.getY() != 0)
        fTargetRatio = aFullScale.getX() / aFullScale.getY();

    bool bAspectRatioKept = rtl::math::approxEqual(fSourceRatio, fTargetRatio);
    if (bSheared || !bAspectRatioKept)
    {
        // Not only rotation, or scaling does not keep aspect ratio.
        aTransformed = aTransformed.getTransformed(aDeviceTransform, aVisibleRange, fMaximumArea);
    }
    else
    {
        // Just rotation, can do that directly.
        fFullRotate = fmod(fFullRotate * -1, 2 * M_PI);
        if (fFullRotate < 0)
            fFullRotate += 2 * M_PI;

        Degree10 nAngle10(basegfx::fround(basegfx::rad2deg<10>(fFullRotate)));
        aTransformed.Rotate(nAngle10, COL_TRANSPARENT);
    }

    basegfx::B2DRange aTargetRange(0.0, 0.0, 1.0, 1.0);

    // get logic object target range
    aTargetRange.transform(rLogicalTransform);

    // get from unified/relative VisibleRange to logic one
    aVisibleRange.transform(
        basegfx::utils::createScaleTranslateB2DHomMatrix(
            aTargetRange.getRange(),
            aTargetRange.getMinimum()));

    const Point aDestPt(basegfx::fround<tools::Long>(aVisibleRange.getMinX()),
                        basegfx::fround<tools::Long>(aVisibleRange.getMinY()));
    const Size aDestSize(
        basegfx::fround<tools::Long>(aVisibleRange.getMaxX()) - aDestPt.X(),
        basegfx::fround<tools::Long>(aVisibleRange.getMaxY()) - aDestPt.Y());

    DrawBitmap(aDestPt, aDestSize, aTransformed);
}

void OutputDevice::ImplMirrorIfRTL(SalTwoRect& rPosAry) const
{
    if (!mpGraphics)
        return;

    const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    if (!bRTL)
        return;

    // Polymorphic fetch of the mirroring axis (No more IsVirtual!)
    tools::Long nFrameWidth = GetRTLFrameWidth();

    tools::Rectangle aDestRect(Point(rPosAry.mnDestX, rPosAry.mnDestY),
                               Size(rPosAry.mnDestWidth, rPosAry.mnDestHeight));

    mpMapper->MirrorDevicePixelRect(aDestRect, nFrameWidth, bRTL, ImplIsAntiparallel());

    rPosAry.mnDestX = aDestRect.Left();
    rPosAry.mnDestY = aDestRect.Top();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
