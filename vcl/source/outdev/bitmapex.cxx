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

#include <rtl/math.hxx>
#include <comphelper/lok.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>

#include <vcl/BitmapMonochromeFilter.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>

#include <bitmap/bmpfast.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <memory>

void OutputDevice::DrawBitmapEx(Point const& rDestPt, BitmapEx const& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.GetTransparentType() == TransparentType::NONE)
        DrawBitmap(rDestPt, rBitmapEx.GetBitmap());

    if (mpMetaFile && rBitmapEx.GetTransparentType() == TransparentType::NONE)
    {
        if (meRasterOp == RasterOp::Invert)
        {
            const Size aSizePix(rBitmapEx.GetSizePixel());
            mpMetaFile->AddAction(
                new MetaRectAction(tools::Rectangle(rDestPt, PixelToLogic(aSizePix))));
            return;
        }

        mpMetaFile->AddAction(
            new MetaBmpExAction(rDestPt, GetDrawModeBitmapEx(rBitmapEx, GetDrawMode())));
    }

    const Size aSizePix(rBitmapEx.GetSizePixel());
    DrawBitmapEx2(rDestPt, PixelToLogic(aSizePix), Point(), aSizePix, rBitmapEx);
}

void OutputDevice::DrawBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                BitmapEx const& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.GetTransparentType() == TransparentType::NONE)
        DrawBitmap(rDestPt, rDestSize, rBitmapEx.GetBitmap());

    if (mpMetaFile && rBitmapEx.GetTransparentType() == TransparentType::NONE)
    {
        if (meRasterOp == RasterOp::Invert)
        {
            mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
            return;
        }

        mpMetaFile->AddAction(new MetaBmpExScaleAction(
            rDestPt, rDestSize, GetDrawModeBitmapEx(rBitmapEx, GetDrawMode())));
    }

    DrawBitmapEx2(rDestPt, rDestSize, Point(), rBitmapEx.GetSizePixel(), rBitmapEx);
}

void OutputDevice::DrawBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                BitmapEx const& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.GetTransparentType() != TransparentType::NONE)
    {
        DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx.GetBitmap());
        return;
    }

    if (mpMetaFile && rBitmapEx.GetTransparentType() != TransparentType::NONE)
    {
        if (meRasterOp == RasterOp::Invert)
        {
            mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
            return;
        }

        mpMetaFile->AddAction(
            new MetaBmpExScalePartAction(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel,
                                         GetDrawModeBitmapEx(rBitmapEx, GetDrawMode())));
    }

    DrawBitmapEx2(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx);
}

void OutputDevice::DrawBitmapEx2(Point const& rDestPt, Size const& rDestSize,
                                 Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                 BitmapEx const& rBitmapEx)
{
    if (rBitmapEx.GetTransparentType() != TransparentType::NONE)
    {
        if (RasterOp::Invert == meRasterOp)
        {
            RenderContext2::DrawRect(tools::Rectangle(rDestPt, rDestSize));
            return;
        }
    }

    BitmapEx aBmpEx(rBitmapEx);

    if (rBitmapEx.GetTransparentType() != TransparentType::NONE)
        aBmpEx = GetDrawModeBitmapEx(rBitmapEx, GetDrawMode());

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    DrawDeviceBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmpEx);
}

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

// Note: this is extremely similar to RenderContext2::DrawTransformedBitmapEx(). Most of the
// common functionality has been refactored into functions, but if the code similarities are
// very similar, it is because the core logic is mostly similar but to split out Metafile
// processing from actually rendering functionality requires mostly the same logic.

void OutputDevice::DrawTransformedBitmapEx(basegfx::B2DHomMatrix const& rTransformation,
                                           BitmapEx const& rBitmapEx, double fAlpha)
{
    if (ImplIsRecordLayout())
        return;

    if (rBitmapEx.IsEmpty())
        return;

    if (rtl::math::approxEqual(fAlpha, 0.0))
        return;

    /*
       tdf#135325 typically in these OutputDevice methods, for the in
       record-to-metafile case the  MetaFile is already written to before the
       test against mbOutputClipped to determine that output to the current
       device would result in no visual output. In this case the metafile is
       written after the test, so we must continue past mbOutputClipped if
       recording to a metafile. It's typical to record with a device of nominal
       size and play back later against something of a totally different size.
     */
    if (mbOutputClipped && !mpMetaFile)
        return;

#ifdef DO_TIME_TEST
    // MM02 start time test when some data (not for trivial stuff). Will
    // trigger and show data when leaving this method by destructing helper
    static const char* pEnableBitmapDrawTimerTimer(getenv("SAL_ENABLE_TIMER_BITMAPDRAW"));
    static bool bUseTimer(nullptr != pEnableBitmapDrawTimerTimer);
    std::unique_ptr<LocalTimeTest> aTimeTest(
        bUseTimer && rBitmapEx.GetSizeBytes() > 10000 ? new LocalTimeTest() : nullptr);
#endif

    // tdf#130768 CAUTION(!) using GetViewTransformation() is *not* enough here, it may
    // be that mnOutOffX/mnOutOffY is used - see AOO bug 75163, mentioned at
    // ImplGetDeviceTransformation declaration
    basegfx::B2DHomMatrix aFullTransform(ImplGetDeviceTransformation() * rTransformation);

    BitmapEx bitmapEx = ApplyAlphaBitmapEx(rBitmapEx, fAlpha);

    // decompose matrix to check rotation and shear
    basegfx::B2DVector aScale, aTranslate;
    double fRotate, fShearX;
    rTransformation.decompose(aScale, aTranslate, fRotate, fShearX);
    const bool bRotated(!basegfx::fTools::equalZero(fRotate));
    const bool bSheared(!basegfx::fTools::equalZero(fShearX));
    const bool bMirrored(basegfx::fTools::less(aScale.getX(), 0.0)
                         || basegfx::fTools::less(aScale.getY(), 0.0));

    if (!bRotated && !bSheared && !bMirrored)
    {
        DrawUntransformedBitmapEx(bitmapEx, aTranslate, aScale);
        return;
    }

    // take the fallback when no rotate and shear, but mirror (else we would have done this above)
    if (!bRotated && !bSheared)
    {
        DrawMirroredBitmapEx(bitmapEx, aTranslate, aScale);
        return;
    }

    // at this point we are either sheared or rotated or both
    assert(bSheared || bRotated);

    basegfx::B2DRange aVisibleRange(0.0, 0.0, 1.0, 1.0);

    // limit maximum area to something looking good for non-pixel-based targets (metafile, printer)
    // by using a fixed minimum (allow at least, but no need to utilize) for good smoothing and an area
    // dependent of original size for good quality when e.g. rotated/sheared. Still, limit to a maximum
    // to avoid crashes/resource problems (ca. 1500x3000 here)
    const Size& rOriginalSizePixel(rBitmapEx.GetSizePixel());
    const double fOrigArea(rOriginalSizePixel.Width() * rOriginalSizePixel.Height() * 0.5);
    const double fOrigAreaScaled(fOrigArea * 1.44);
    double fMaximumArea(std::clamp(fOrigAreaScaled, 1000000.0, 4500000.0));

    if (!mpMetaFile)
    {
        aVisibleRange = ReduceBitmapExVisibleRange(aFullTransform, aVisibleRange);

        if (aVisibleRange.isEmpty())
            return;

        fMaximumArea = GetMaximumBitmapExArea(aVisibleRange);
    }

    if (aVisibleRange.isEmpty())
        return;

    // fallback; create transformed bitmap the hard way (back-transform the pixels) and paint
    Point aDestPt;
    Size aDestSize;
    BitmapEx aTransformed;
    std::tie(aDestPt, aDestSize, aTransformed) = CreateTransformedBitmapFallback(
        bitmapEx, rOriginalSizePixel, rTransformation, aVisibleRange, fMaximumArea);

    DrawBitmapEx(aDestPt, aDestSize, aTransformed);
}

bool OutputDevice::TryDirectBitmapExPaint() const
{
    const bool bInvert(RasterOp::Invert == meRasterOp);
    const bool bBitmapChangedColor(
        mnDrawMode
        & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap | DrawModeFlags::GrayBitmap));

    return (!bInvert && !bBitmapChangedColor && !mpMetaFile);
}

void OutputDevice::DrawUntransformedBitmapEx(BitmapEx const& rBitmapEx,
                                             basegfx::B2DVector const& rTranslate,
                                             basegfx::B2DVector const& rScale)
{
    // with no rotation, shear or mirroring it can be mapped to DrawBitmapEx
    // do *not* execute the mirroring here, it's done in the fallback
    // #i124580# the correct DestSize needs to be calculated based on MaxXY values
    Point aDestPt(basegfx::fround(rTranslate.getX()), basegfx::fround(rTranslate.getY()));
    const Size aDestSize(basegfx::fround(rScale.getX() + rTranslate.getX()) - aDestPt.X(),
                         basegfx::fround(rScale.getY() + rTranslate.getY()) - aDestPt.Y());
    const Point aOrigin = GetMapMode().GetOrigin();

    if (!mpMetaFile && comphelper::LibreOfficeKit::isActive()
        && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        aDestPt.Move(aOrigin.getX(), aOrigin.getY());
        EnableMapMode(false);
    }

    DrawBitmapEx(aDestPt, aDestSize, rBitmapEx);

    if (!mpMetaFile && comphelper::LibreOfficeKit::isActive()
        && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        EnableMapMode();
        aDestPt.Move(-aOrigin.getX(), -aOrigin.getY());
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
