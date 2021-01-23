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
#include <rtl/math.hxx>
#include <sal/log.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <tools/debug.hxx>
#include <tools/helpers.hxx>
#include <tools/stream.hxx>
#include <comphelper/lok.hxx>

#include <vcl/dibtools.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/BitmapFilterStackBlur.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/image.hxx>
#include <vcl/BitmapMonochromeFilter.hxx>

#include <bitmap/BitmapWriteAccess.hxx>
#include <bitmap/bmpfast.hxx>
#include <salgdi.hxx>
#include <salbmp.hxx>

#include <cassert>
#include <cstdlib>
#include <memory>

BitmapEx OutputDevice::GetBitmapEx(const Point& rSrcPt, const Size& rSize) const
{
    // #110958# Extract alpha value from VDev, if any
    if (mpAlphaVDev)
    {
        Bitmap aAlphaBitmap(mpAlphaVDev->GetBitmap(rSrcPt, rSize));

        // ensure 8 bit alpha
        if (aAlphaBitmap.GetBitCount() > 8)
            aAlphaBitmap.Convert(BmpConversion::N8BitNoConversion);

        return BitmapEx(GetBitmap(rSrcPt, rSize), AlphaMask(aAlphaBitmap));
    }

    return RenderContext2::GetBitmapEx(rSrcPt, rSize);
}

void OutputDevice::DrawBitmapEx(const Point& rDestPt, const BitmapEx& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (TransparentType::NONE == rBitmapEx.GetTransparentType())
    {
        DrawBitmap(rDestPt, rBitmapEx.GetBitmap());
    }
    else
    {
        const Size aSizePix(rBitmapEx.GetSizePixel());
        DrawBitmapEx(rDestPt, maGeometry.PixelToLogic(aSizePix), Point(), aSizePix, rBitmapEx,
                     MetaActionType::BMPEX);
    }
}

void OutputDevice::DrawBitmapEx(const Point& rDestPt, const Size& rDestSize,
                                const BitmapEx& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (TransparentType::NONE == rBitmapEx.GetTransparentType())
    {
        DrawBitmap(rDestPt, rDestSize, rBitmapEx.GetBitmap());
    }
    else
    {
        DrawBitmapEx(rDestPt, rDestSize, Point(), rBitmapEx.GetSizePixel(), rBitmapEx,
                     MetaActionType::BMPEXSCALE);
    }
}

void OutputDevice::DrawBitmapEx(const Point& rDestPt, const Size& rDestSize,
                                const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                const BitmapEx& rBitmapEx, const MetaActionType nAction)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.GetTransparentType() == TransparentType::NONE)
    {
        DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx.GetBitmap());
        return;
    }

    if (meRasterOp == RasterOp::Invert)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    DrawTransparentBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx, nAction);
}

void OutputDevice::DrawTransparentBitmapEx(const Point& rDestPt, const Size& rDestSize,
                                           const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                           BitmapEx const& rBitmapEx, const MetaActionType nAction)
{
    BitmapEx aBmpEx(rBitmapEx);

    if (GetDrawMode() & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap))
    {
        Bitmap aColorBmp(aBmpEx.GetSizePixel(), 1);
        sal_uInt8 cCmpVal;

        if (GetDrawMode() & DrawModeFlags::BlackBitmap)
            cCmpVal = 0;
        else
            cCmpVal = 255;

        aColorBmp.Erase(Color(cCmpVal, cCmpVal, cCmpVal));

        if (aBmpEx.IsAlpha())
        {
            // Create one-bit mask out of alpha channel, by
            // thresholding it at alpha=0.5. As
            // DRAWMODE_BLACK/WHITEBITMAP requires monochrome
            // output, having alpha-induced grey levels is not
            // acceptable.
            BitmapEx aMaskEx(aBmpEx.GetAlpha().GetBitmap());
            BitmapFilter::Filter(aMaskEx, BitmapMonochromeFilter(129));
            aBmpEx = BitmapEx(aColorBmp, aMaskEx.GetBitmap());
        }
        else
        {
            aBmpEx = BitmapEx(aColorBmp, aBmpEx.GetMask());
        }
    }

    if (GetDrawMode() & DrawModeFlags::GrayBitmap && !!aBmpEx)
        aBmpEx.Convert(BmpConversion::N8BitGreys);

    if (mpMetaFile)
    {
        switch (nAction)
        {
            case MetaActionType::BMPEX:
                mpMetaFile->AddAction(new MetaBmpExAction(rDestPt, aBmpEx));
                break;

            case MetaActionType::BMPEXSCALE:
                mpMetaFile->AddAction(new MetaBmpExScaleAction(rDestPt, rDestSize, aBmpEx));
                break;

            case MetaActionType::BMPEXSCALEPART:
                mpMetaFile->AddAction(new MetaBmpExScalePartAction(rDestPt, rDestSize, rSrcPtPixel,
                                                                   rSrcSizePixel, aBmpEx));
                break;

            default:
                break;
        }
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    assert(!is_double_buffered_window());

    if (aBmpEx.IsAlpha())
    {
        DrawTransparentAlphaBitmap(aBmpEx.GetBitmap(), aBmpEx.GetAlpha(), rDestPt, rDestSize,
                                   rSrcPtPixel, rSrcSizePixel);
    }
    else if (!!aBmpEx)
    {
        DrawAlphaBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmpEx);
    }
}

void OutputDevice::DrawAlphaBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                     Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                     BitmapEx const& rBitmapEx)
{
    BitmapEx aBmpEx(rBitmapEx);

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                       rSrcSizePixel.Height(), maGeometry.LogicXToDevicePixel(rDestPt.X()),
                       maGeometry.LogicYToDevicePixel(rDestPt.Y()),
                       maGeometry.LogicWidthToDevicePixel(rDestSize.Width()),
                       maGeometry.LogicHeightToDevicePixel(rDestSize.Height()));

    const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aBmpEx.GetSizePixel());

    if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth && aPosAry.mnDestHeight)
    {
        if (nMirrFlags != BmpMirrorFlags::NONE)
            aBmpEx.Mirror(nMirrFlags);

        if (!DrawMaskedAlphaBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmpEx))
        {
            const SalBitmap* pSalSrcBmp = aBmpEx.ImplGetBitmapSalBitmap().get();
            mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *this);

            // #i32109#: Make bitmap area opaque
            if (mpAlphaVDev)
                mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
        }
    }
}

bool OutputDevice::DrawMaskedAlphaBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                           Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                           BitmapEx const& rBitmapEx)
{
    BitmapEx aBmpEx(rBitmapEx);

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                       rSrcSizePixel.Height(), maGeometry.LogicXToDevicePixel(rDestPt.X()),
                       maGeometry.LogicYToDevicePixel(rDestPt.Y()),
                       maGeometry.LogicWidthToDevicePixel(rDestSize.Width()),
                       maGeometry.LogicHeightToDevicePixel(rDestSize.Height()));

    const SalBitmap* pSalSrcBmp = aBmpEx.ImplGetBitmapSalBitmap().get();
    std::shared_ptr<SalBitmap> xMaskBmp = aBmpEx.ImplGetMaskSalBitmap();

    if (!xMaskBmp)
        return false;

    assert(pSalSrcBmp);

    // try to paint as alpha directly. If this worked, we are done (except alpha, see below)
    if (!mpGraphics->DrawAlphaBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *this))
    {
        // #4919452# reduce operation area to bounds of cliprect. since masked transparency involves
        // creation of a large vdev and copying the screen content into that (slooow read from
        // framebuffer), that should considerably increase performance for large bitmaps and small
        // clippings.

        // Note that this optimization is a workaround for a Writer peculiarity, namely, to decompose
        // background graphics into myriads of disjunct, tiny rectangles. That otherwise kills us
        // here, since for transparent output, SAL always prepares the whole bitmap, if aPosAry
        // contains the whole bitmap (and it's _not_ to blame for that).

        // Note the call to ImplPixelToDevicePixel(), since aPosAry already contains the
        // mnOutOff-offsets, they also have to be applied to the region
        tools::Rectangle aClipRegionBounds(maGeometry.PixelToDevicePixel(maRegion).GetBoundRect());

        // TODO: Also respect scaling (that's a bit tricky, since the source points have to move
        // fractional amounts (which is not possible, thus has to be emulated by increases of the
        // copy area)

        // for now, only identity scales allowed
        if (!aClipRegionBounds.IsEmpty() && aPosAry.mnDestWidth == aPosAry.mnSrcWidth
            && aPosAry.mnDestHeight == aPosAry.mnSrcHeight)
        {
            // now intersect dest rect with clip region
            aClipRegionBounds.Intersection(tools::Rectangle(
                aPosAry.mnDestX, aPosAry.mnDestY, aPosAry.mnDestX + aPosAry.mnDestWidth - 1,
                aPosAry.mnDestY + aPosAry.mnDestHeight - 1));

            // Note: I could theoretically optimize away the DrawBitmap below, if the region is empty
            // here. Unfortunately, cannot rule out that somebody relies on the side effects.
            if (!aClipRegionBounds.IsEmpty())
            {
                aPosAry.mnSrcX += aClipRegionBounds.Left() - aPosAry.mnDestX;
                aPosAry.mnSrcY += aClipRegionBounds.Top() - aPosAry.mnDestY;
                aPosAry.mnSrcWidth = aClipRegionBounds.GetWidth();
                aPosAry.mnSrcHeight = aClipRegionBounds.GetHeight();

                aPosAry.mnDestX = aClipRegionBounds.Left();
                aPosAry.mnDestY = aClipRegionBounds.Top();
                aPosAry.mnDestWidth = aClipRegionBounds.GetWidth();
                aPosAry.mnDestHeight = aClipRegionBounds.GetHeight();
            }
        }

        mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *this);
    }

    // #110958# Paint mask to alpha channel. Luckily, the black and white representation of the mask
    // maps to the alpha channel

    // #i25167# Restrict mask painting to _opaque_ areas of the mask, otherwise we spoil areas where
    // no bitmap content was ever visible. Interestingly enough, this can be achieved by taking the
    // mask as the transparency mask of itself
    if (mpAlphaVDev)
        mpAlphaVDev->DrawBitmapEx(rDestPt, rDestSize, BitmapEx(aBmpEx.GetMask(), aBmpEx.GetMask()));

    return true;
}

bool OutputDevice::DrawTransformBitmapExDirect(basegfx::B2DHomMatrix const& aFullTransform,
                                               BitmapEx const& rBitmapEx)
{
    assert(!is_double_buffered_window());

    Bitmap aAlphaBitmap;

    if (rBitmapEx.IsTransparent())
    {
        if (rBitmapEx.IsAlpha())
            aAlphaBitmap = rBitmapEx.GetAlpha();
        else
            aAlphaBitmap = rBitmapEx.GetMask();
    }
    else if (mpAlphaVDev)
    {
        aAlphaBitmap = AlphaMask(rBitmapEx.GetSizePixel());
        aAlphaBitmap.Erase(COL_BLACK); // opaque
    }

    bool bDone
        = RenderContext2::DrawTransformBitmapExDirect(aFullTransform, BitmapEx(aAlphaBitmap));

    if (mpAlphaVDev)
    {
        // Merge bitmap alpha to alpha device
        AlphaMask aBlack(rBitmapEx.GetSizePixel());
        aBlack.Erase(0); // opaque
        mpAlphaVDev->DrawTransformBitmapExDirect(aFullTransform, BitmapEx(aBlack, aAlphaBitmap));
    }

    return bDone;
};

// MM02 add some test class to get a simple timer-based output to be able
// to check if it gets faster - and how much. Uncomment next line or set
// DO_TIME_TEST for compile time if you want to use it
// #define DO_TIME_TEST
#ifdef DO_TIME_TEST
#include <tools/time.hxx>
struct LocalTimeTest
{
    const sal_uInt64 nStartTime;
    LocalTimeTest()
        : nStartTime(tools::Time::GetSystemTicks())
    {
    }
    ~LocalTimeTest()
    {
        const sal_uInt64 nEndTime(tools::Time::GetSystemTicks());
        const sal_uInt64 nDiffTime(nEndTime - nStartTime);

        if (nDiffTime > 0)
        {
            OStringBuffer aOutput("Time: ");
            OString aNumber(OString::number(nDiffTime));
            aOutput.append(aNumber);
            OSL_FAIL(aOutput.getStr());
        }
    }
};
#endif

void OutputDevice::DrawTransformedBitmapEx(const basegfx::B2DHomMatrix& rTransformation,
                                           const BitmapEx& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.IsEmpty())
        return;

    // MM02 compared to other public methods of OutputDevice
    // this test was missing and led to zero-ptr-accesses
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    const bool bMetafile(nullptr != mpMetaFile);
    /*
       tdf#135325 typically in these OutputDevice methods, for the in
       record-to-metafile case the  MetaFile is already written to before the
       test against mbOutputClipped to determine that output to the current
       device would result in no visual output. In this case the metafile is
       written after the test, so we must continue past mbOutputClipped if
       recording to a metafile. It's typical to record with a device of nominal
       size and play back later against something of a totally different size.
     */
    if (mbOutputClipped && !bMetafile)
        return;

#ifdef DO_TIME_TEST
    // MM02 start time test when some data (not for trivial stuff). Will
    // trigger and show data when leaving this method by destructing helper
    static const char* pEnableBitmapDrawTimerTimer(getenv("SAL_ENABLE_TIMER_BITMAPDRAW"));
    static bool bUseTimer(nullptr != pEnableBitmapDrawTimerTimer);
    std::unique_ptr<LocalTimeTest> aTimeTest(
        bUseTimer && rBitmapEx.GetSizeBytes() > 10000 ? new LocalTimeTest() : nullptr);
#endif

    // MM02 reorganize order: Prefer DrawTransformBitmapExDirect due
    // to this having evolved and is improved on quite some systems.
    // Check for exclusion parameters that may prevent using it
    static bool bAllowPreferDirectPaint(true);
    const bool bInvert(RasterOp::Invert == meRasterOp);
    const bool bBitmapChangedColor(
        GetDrawMode()
        & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap | DrawModeFlags::GrayBitmap));
    const bool bTryDirectPaint(!bInvert && !bBitmapChangedColor && !bMetafile);

    if (bAllowPreferDirectPaint && bTryDirectPaint)
    {
        // tdf#130768 CAUTION(!) using GetViewTransformation() is *not* enough here, it may
        // be that mnOutOffX/mnOutOffY is used - see AOO bug 75163, mentioned at
        // GetDeviceTransformation declaration
        const basegfx::B2DHomMatrix aFullTransform(GetDeviceTransformation() * rTransformation);

        if (DrawTransformBitmapExDirect(aFullTransform, rBitmapEx))
        {
            // we are done
            return;
        }
    }

    // decompose matrix to check rotation and shear
    basegfx::B2DVector aScale, aTranslate;
    double fRotate, fShearX;
    rTransformation.decompose(aScale, aTranslate, fRotate, fShearX);
    const bool bRotated(!basegfx::fTools::equalZero(fRotate));
    const bool bSheared(!basegfx::fTools::equalZero(fShearX));
    const bool bMirroredX(basegfx::fTools::less(aScale.getX(), 0.0));
    const bool bMirroredY(basegfx::fTools::less(aScale.getY(), 0.0));

    if (!bRotated && !bSheared && !bMirroredX && !bMirroredY)
    {
        // with no rotation, shear or mirroring it can be mapped to DrawBitmapEx
        // do *not* execute the mirroring here, it's done in the fallback
        // #i124580# the correct DestSize needs to be calculated based on MaxXY values
        Point aDestPt(basegfx::fround(aTranslate.getX()), basegfx::fround(aTranslate.getY()));
        const Size aDestSize(basegfx::fround(aScale.getX() + aTranslate.getX()) - aDestPt.X(),
                             basegfx::fround(aScale.getY() + aTranslate.getY()) - aDestPt.Y());
        const Point aOrigin = GetMapMode().GetOrigin();
        if (!bMetafile && comphelper::LibreOfficeKit::isActive()
            && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
        {
            aDestPt.Move(aOrigin.getX(), aOrigin.getY());
            DisableMapMode();
        }

        DrawBitmapEx(aDestPt, aDestSize, rBitmapEx);
        if (!bMetafile && comphelper::LibreOfficeKit::isActive()
            && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
        {
            EnableMapMode();
            aDestPt.Move(-aOrigin.getX(), -aOrigin.getY());
        }
        return;
    }

    // MM02 bAllowPreferDirectPaint may have been false to allow
    // to specify order of executions, so give bTryDirectPaint a call
    if (bTryDirectPaint)
    {
        // tdf#130768 CAUTION(!) using GetViewTransformation() is *not* enough here, it may
        // be that mnOutOffX/mnOutOffY is used - see AOO bug 75163, mentioned at
        // GetDeviceTransformation declaration
        const basegfx::B2DHomMatrix aFullTransform(GetDeviceTransformation() * rTransformation);

        if (DrawTransformBitmapExDirect(aFullTransform, rBitmapEx))
        {
            // we are done
            return;
        }
    }

    // take the fallback when no rotate and shear, but mirror (else we would have done this above)
    if (!bRotated && !bSheared)
    {
        // with no rotation or shear it can be mapped to DrawBitmapEx
        // do *not* execute the mirroring here, it's done in the fallback
        // #i124580# the correct DestSize needs to be calculated based on MaxXY values
        const Point aDestPt(basegfx::fround(aTranslate.getX()), basegfx::fround(aTranslate.getY()));
        const Size aDestSize(basegfx::fround(aScale.getX() + aTranslate.getX()) - aDestPt.X(),
                             basegfx::fround(aScale.getY() + aTranslate.getY()) - aDestPt.Y());

        DrawBitmapEx(aDestPt, aDestSize, rBitmapEx);
        return;
    }

    // at this point we are either sheared or rotated or both
    assert(bSheared || bRotated);

    // fallback; create transformed bitmap the hard way (back-transform
    // the pixels) and paint
    basegfx::B2DRange aVisibleRange(0.0, 0.0, 1.0, 1.0);

    // limit maximum area to something looking good for non-pixel-based targets (metafile, printer)
    // by using a fixed minimum (allow at least, but no need to utilize) for good smoothing and an area
    // dependent of original size for good quality when e.g. rotated/sheared. Still, limit to a maximum
    // to avoid crashes/resource problems (ca. 1500x3000 here)
    const Size& rOriginalSizePixel(rBitmapEx.GetSizePixel());
    const double fOrigArea(rOriginalSizePixel.Width() * rOriginalSizePixel.Height() * 0.5);
    const double fOrigAreaScaled(fOrigArea * 1.44);
    double fMaximumArea(std::clamp(fOrigAreaScaled, 1000000.0, 4500000.0));
    // tdf#130768 CAUTION(!) using GetViewTransformation() is *not* enough here, it may
    // be that mnOutOffX/mnOutOffY is used - see AOO bug 75163, mentioned at
    // GetDeviceTransformation declaration
    basegfx::B2DHomMatrix aFullTransform(GetDeviceTransformation() * rTransformation);

    if (!bMetafile)
    {
        if (!TransformAndReduceBitmapExToTargetRange(aFullTransform, aVisibleRange, fMaximumArea))
            return;
    }

    if (aVisibleRange.isEmpty())
        return;

    BitmapEx aTransformed(rBitmapEx);

    // #122923# when the result needs an alpha channel due to being rotated or sheared
    // and thus uncovering areas, add these channels so that the own transformer (used
    // in getTransformed) also creates a transformed alpha channel
    if (!aTransformed.IsTransparent() && (bSheared || bRotated))
    {
        // parts will be uncovered, extend aTransformed with a mask bitmap
        const Bitmap aContent(aTransformed.GetBitmap());

        AlphaMask aMaskBmp(aContent.GetSizePixel());
        aMaskBmp.Erase(0);

        aTransformed = BitmapEx(aContent, aMaskBmp);
    }

    // Remove scaling from aFulltransform: we transform due to shearing or rotation, scaling
    // will happen according to aDestSize.
    basegfx::B2DVector aFullScale, aFullTranslate;
    double fFullRotate, fFullShearX;
    aFullTransform.decompose(aFullScale, aFullTranslate, fFullRotate, fFullShearX);
    // Require positive scaling, negative scaling would loose horizontal or vertical flip.
    if (aFullScale.getX() > 0 && aFullScale.getY() > 0)
    {
        basegfx::B2DHomMatrix aTransform = basegfx::utils::createScaleB2DHomMatrix(
            rOriginalSizePixel.getWidth() / aFullScale.getX(),
            rOriginalSizePixel.getHeight() / aFullScale.getY());
        aFullTransform *= aTransform;
    }

    double fSourceRatio = 1.0;
    if (rOriginalSizePixel.getHeight() != 0)
    {
        fSourceRatio = rOriginalSizePixel.getWidth() / rOriginalSizePixel.getHeight();
    }
    double fTargetRatio = 1.0;
    if (aFullScale.getY() != 0)
    {
        fTargetRatio = aFullScale.getX() / aFullScale.getY();
    }
    bool bAspectRatioKept = rtl::math::approxEqual(fSourceRatio, fTargetRatio);
    if (bSheared || !bAspectRatioKept)
    {
        // Not only rotation, or scaling does not keep aspect ratio.
        aTransformed = aTransformed.getTransformed(aFullTransform, aVisibleRange, fMaximumArea);
    }
    else
    {
        // Just rotation, can do that directly.
        fFullRotate = fmod(fFullRotate * -1, F_2PI);
        if (fFullRotate < 0)
        {
            fFullRotate += F_2PI;
        }
        Degree10 nAngle10(basegfx::fround(basegfx::rad2deg(fFullRotate) * 10));
        aTransformed.Rotate(nAngle10, COL_TRANSPARENT);
    }
    basegfx::B2DRange aTargetRange(0.0, 0.0, 1.0, 1.0);

    // get logic object target range
    aTargetRange.transform(rTransformation);

    // get from unified/relative VisibleRange to logoc one
    aVisibleRange.transform(basegfx::utils::createScaleTranslateB2DHomMatrix(
        aTargetRange.getRange(), aTargetRange.getMinimum()));

    // extract point and size; do not remove size, the bitmap may have been prepared reduced by purpose
    // #i124580# the correct DestSize needs to be calculated based on MaxXY values
    const Point aDestPt(basegfx::fround(aVisibleRange.getMinX()),
                        basegfx::fround(aVisibleRange.getMinY()));
    const Size aDestSize(basegfx::fround(aVisibleRange.getMaxX()) - aDestPt.X(),
                         basegfx::fround(aVisibleRange.getMaxY()) - aDestPt.Y());

    DrawBitmapEx(aDestPt, aDestSize, aTransformed);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
