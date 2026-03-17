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

#include <sal/types.h>
#include <osl/diagnose.h>
#include <rtl/math.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <tools/helpers.hxx>
#include <tools/mapunit.hxx>
#include <officecfg/Office/Common.hxx>

#include <vcl/BitmapTools.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/print.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/virdev.hxx>
#include <vcl/BitmapWriteAccess.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <pdf/pdfwriter_impl.hxx>
#include <text/TextLayoutEngine.hxx>
#include <salgdi.hxx>

#include <comphelper/scopeguard.hxx>
#include <list>
#include <memory>
#include <utility>

namespace
{
    /**
     * Perform a safe approximation of a polygon from double-precision
     * coordinates to integer coordinates, to ensure that it has at least 2
     * pixels in both X and Y directions.
     */
    tools::Polygon toPolygon( const basegfx::B2DPolygon& rPoly )
    {
        basegfx::B2DRange aRange = rPoly.getB2DRange();
        double fW = aRange.getWidth(), fH = aRange.getHeight();
        if (0.0 < fW && 0.0 < fH && (fW <= 1.0 || fH <= 1.0))
        {
            // This polygon not empty but is too small to display.  Approximate it
            // with a rectangle large enough to be displayed.
            double nX = aRange.getMinX(), nY = aRange.getMinY();
            double nW = std::max<double>(1.0, rtl::math::round(fW));
            double nH = std::max<double>(1.0, rtl::math::round(fH));

            tools::Polygon aTarget;
            aTarget.Insert(0, Point(nX, nY));
            aTarget.Insert(1, Point(nX+nW, nY));
            aTarget.Insert(2, Point(nX+nW, nY+nH));
            aTarget.Insert(3, Point(nX, nY+nH));
            aTarget.Insert(4, Point(nX, nY));
            return aTarget;
        }
        return tools::Polygon(rPoly);
    }

    tools::PolyPolygon toPolyPolygon( const basegfx::B2DPolyPolygon& rPolyPoly )
    {
        tools::PolyPolygon aTarget;
        for (auto const& rB2DPolygon : rPolyPoly)
            aTarget.Insert(toPolygon(rB2DPolygon));

        return aTarget;
    }
}

// Caution: This method is nearly the same as
// void OutputDevice::DrawPolyPolygon( const basegfx::B2DPolyPolygon& rB2DPolyPoly )
// so when changes are made here do not forget to make changes there, too

void OutputDevice::DrawTransparentWithRasterOp( const basegfx::B2DHomMatrix& rObjectTransform,
                                                const basegfx::B2DPolyPolygon& rB2DPolyPoly,
                                                double fTransparency,
                                                RasterOp eRasterOp )
{
    basegfx::B2DPolyPolygon aTransformed(rB2DPolyPoly);
    aTransformed.transform(rObjectTransform);
    DrawTransparentWithRasterOp(toPolyPolygon(aTransformed),
                                static_cast<sal_uInt16>(fTransparency * 100.0),
                                eRasterOp);
}

void OutputDevice::DrawTransparentWithRasterOp( const tools::PolyPolygon& rPolyPoly,
                                                sal_uInt16 nTransparencePercent,
                                                RasterOp eRasterOp )
{
    RasterOp eOldRasterOp = GetRasterOp();
    bool bChanged = (eOldRasterOp != eRasterOp);

    if (bChanged)
        SetRasterOp(eRasterOp); // This also records the state change to the metafile

    comphelper::ScopeGuard aRasterOpGuard([this, bChanged, eOldRasterOp]() {
        if (bChanged)
            SetRasterOp(eOldRasterOp); // Restore previous op
    });

    maRecorder.RecordTransparent(rPolyPoly, nTransparencePercent);

    if (!IsDeviceOutputNecessary() || IsLayoutCalculationNecessary())
        return;

    // We use software emulation because hardware paths generally don't support XOR/Invert combined with alpha
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    tools::PolyPolygon aPolyPoly(LogicToPixel(rPolyPoly));
    tools::Rectangle aDstRect = GetVisibleDeviceRangePixel(aPolyPoly);

    if (aDstRect.IsEmpty())
        return;

    bool bDrawn = false;

    // #i66849# Added fast path for exactly rectangular polygons
    if (aPolyPoly.IsRect())
    {
        if (!FlushGraphicsState(vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill | vcl::PrepareOutputFlags::Clip))
            return;

        bDrawn = IsOutputCulled();

        if (!bDrawn)
        {
            const tools::Rectangle aPixelRect(LogicToDevicePixel(rPolyPoly.GetBoundRect()));
            bDrawn = mpGraphics->DrawAlphaRect(
                aPixelRect.Left(), aPixelRect.Top(),
                aPixelRect.getOpenWidth(), aPixelRect.getOpenHeight(),
                sal::static_int_cast<sal_uInt8>(nTransparencePercent),
                *this);
        }
    }

    if (!bDrawn)
        DrawTransparentFallback(rPolyPoly, aPolyPoly, aDstRect, nTransparencePercent);
}

void OutputDevice::DrawTransparentFallback(const tools::PolyPolygon& rLogicalPolyPoly,
                                           tools::PolyPolygon aPixelPolyPoly,
                                           const tools::Rectangle& rDstRect,
                                           sal_uInt16 nTransparencePercent)
{
    ScopedVclPtrInstance< VirtualDevice > aVDev(*this);
    const Size aDstSz( rDstRect.GetSize() );
    const sal_uInt8 cTrans = basegfx::fround<sal_uInt8>(nTransparencePercent * 2.55);

    if( rDstRect.Left() || rDstRect.Top() )
        aPixelPolyPoly.Move( -rDstRect.Left(), -rDstRect.Top() );

    if (!aVDev->SetOutputSizePixel(aDstSz))
        return;

    const bool bOldMap = mpMapper->IsMapModeEnabled();

    mpMapper->EnableMapMode( false );

    aVDev->SetLineColor( COL_BLACK );
    aVDev->SetFillColor( COL_BLACK );
    aVDev->DrawPolyPolygon( aPixelPolyPoly );

    Bitmap aPaint( GetBitmap( rDstRect.TopLeft(), aDstSz ) );
    Bitmap aPolyMask( aVDev->GetBitmap( Point(), aDstSz ) );

    // #107766# check for non-empty bitmaps before accessing them
    if (aPaint.IsEmpty() || aPolyMask.IsEmpty())
        return;

    vcl::rendercontext::BitmapRenderer::BlendAlphaBitmap(aPaint, aPolyMask, GetFillColor(), cTrans);

    DrawBitmap( rDstRect.TopLeft(), aPaint );

    mpMapper->EnableMapMode( bOldMap );

    if( mpGraphicsState->mbLineColor )
    {
        auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR);
        SetFillColor();
        DrawPolyPolygon( rLogicalPolyPoly );
    }
}

void OutputDevice::DrawTransparent(
    const basegfx::B2DHomMatrix& rObjectTransform,
    const basegfx::B2DPolyPolygon& rB2DPolyPoly,
    double fTransparency)
{
    assert(!is_double_buffered_window());

    if (GetRasterOp() != RasterOp::OverPaint)
    {
        // tdf#119843 need transformed Polygon here
        basegfx::B2DPolyPolygon aTransformed(rB2DPolyPoly);
        aTransformed.transform(rObjectTransform);
        DrawTransparentWithRasterOp(toPolyPolygon(aTransformed),
                                    static_cast<sal_uInt16>(fTransparency * 100.0),
                                    GetRasterOp());
        return;
    }

    // AW: Do NOT paint empty PolyPolygons
    if(!rB2DPolyPoly.count())
        return;

    if (!FlushGraphicsState(vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill | vcl::PrepareOutputFlags::Clip))
        return;

    // b2dpolygon support not implemented yet on non-UNX platforms
    basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);

    // ensure it is closed
    if(!aB2DPolyPolygon.isClosed())
    {
        // maybe assert, prevents buffering due to making a copy
        aB2DPolyPolygon.setClosed( true );
    }

    // create ObjectToDevice transformation
    const basegfx::B2DHomMatrix aFullTransform(mpMapper->GetDeviceTransformation() * rObjectTransform);
    // TODO: this must not drop transparency for mpAlphaVDev case, but instead use premultiplied
    // alpha... but that requires using premultiplied alpha also for already drawn data

    if (IsFillColor())
    {
        // #i121591#
        // CAUTION: Only non printing (pixel-renderer) VCL commands from OutputDevices
        // should be used when printing. Normally this is avoided by the printer being
        // non-AAed and thus e.g. on WIN GdiPlus calls are not used. It may be necessary
        // to figure out a way of moving this code to its own function that is
        // overridden by the Print class, which will mean we deliberately override the
        // functionality and we use the fallback some lines below (which is not very good,
        // though. For now, WinSalGraphics::drawPolyPolygon will detect printer usage and
        // correct the wrong mapping (see there for details)
        mpGraphics->DrawPolyPolygon(
            aFullTransform,
            aB2DPolyPolygon,
            fTransparency,
            *this);
    }

    if (IsLineColor())
    {
        const bool bPixelSnapHairline(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

        for(auto const& rPolygon : std::as_const(aB2DPolyPolygon))
        {
            mpGraphics->drawPolyLine(
                aFullTransform,
                rPolygon,
                fTransparency,
                0.0, // tdf#124848 hairline
                nullptr, // MM01
                basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline);
        }
    }

    maRecorder.RecordTransparent(rObjectTransform, rB2DPolyPoly, fTransparency);
}

void OutputDevice::DrawTransparent( const tools::PolyPolygon& rPolyPoly,
                                    sal_uInt16 nTransparencePercent )
{
    assert(!is_double_buffered_window());

    if (GetRasterOp() != RasterOp::OverPaint)
    {
        DrawTransparentWithRasterOp(rPolyPoly, nTransparencePercent, GetRasterOp());
        return;
    }

    // short circuit for drawing an opaque polygon
    if( (nTransparencePercent < 1) || (mpGraphicsState->mnDrawMode & DrawModeFlags::NoTransparency) )
    {
        DrawPolyPolygon( rPolyPoly );
        return;
    }

    // short circuit for drawing an invisible polygon
    if( (!mpGraphicsState->mbFillColor && !mpGraphicsState->mbLineColor) || (nTransparencePercent >= 100) )
        return; // tdf#84294: do not record it in metafile

    maRecorder.RecordTransparent(rPolyPoly, nTransparencePercent);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill | vcl::PrepareOutputFlags::Clip))
        return;

    basegfx::B2DPolyPolygon aB2DPolyPolygon(rPolyPoly.getB2DPolyPolygon());
    const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation());
    const double fTransparency = 0.01 * nTransparencePercent;

    if( mpGraphicsState->mbFillColor )
    {
        mpGraphics->DrawPolyPolygon(
            aTransform,
            aB2DPolyPolygon,
            fTransparency,
            *this);
    }

    if( mpGraphicsState->mbLineColor )
    {
        // disable the fill color for now
        mpGraphics->SetFillColor();

        // draw the border line
        const bool bPixelSnapHairline(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

        for(auto const& rPolygon : std::as_const(aB2DPolyPolygon))
        {
            mpGraphics->drawPolyLine(
                aTransform,
                rPolygon,
                fTransparency,
                0.0, // tdf#124848 hairline
                nullptr, // MM01
                basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline);
        }

        // prepare to restore the fill color
        mbFillColorDirty = mpGraphicsState->mbFillColor;
    }
}

void OutputDevice::DrawTransparent( const GDIMetaFile& rMtf, const Point& rPos,
                                    const Size& rSize, const Gradient& rTransparenceGradient )
{
    DrawTransparent( rMtf, rPos, rSize, rPos, rSize, rTransparenceGradient );
}

static bool lcl_IsEffectivelyOpaque(const Gradient& rTransparenceGradient, DrawModeFlags nDrawMode)
{
    // In VCL transparency masks, Black (0) means 100% opaque.
    const bool bGradientIsOpaque = (rTransparenceGradient.GetStartColor() == COL_BLACK) &&
                                   (rTransparenceGradient.GetEndColor() == COL_BLACK);

    const bool bStateDisablesTransparency = (nDrawMode & DrawModeFlags::NoTransparency) != DrawModeFlags::Default;

    return bGradientIsOpaque || bStateDisablesTransparency;
}

static bool lcl_ConfigureOffscreenBuffer(VirtualDevice& rBuffer,
                                         const OutputDevice& rOutDev,
                                         const tools::Rectangle& rDstRect,
                                         bool bNeedsCopyCycle)
{
    rBuffer.SetDPIX(rOutDev.GetDPIX());
    rBuffer.SetDPIY(rOutDev.GetDPIY());

    // Allocate the pixel memory for the workspace
    if (!rBuffer.SetOutputSizePixel(rDstRect.GetSize(), true, true))
        return false;

    if (bNeedsCopyCycle)
        rBuffer.SetAntialiasing(rOutDev.GetAntialiasing());

    // Align the buffer's logical coordinates with the cropped area on the screen
    MapMode aMap(rOutDev.GetMapMode());
    const Point aOutPos(rOutDev.PixelToLogic(rDstRect.TopLeft()));
    aMap.SetOrigin(Point(-aOutPos.X(), -aOutPos.Y()));
    rBuffer.SetMapMode(aMap);

    return true;
}

namespace
{
struct Renderers
{
    const OutputDevice& rOutDev;
    VirtualDevice& rBuffer;
};

struct RenderSource
{
    const GDIMetaFile& rMtf;
    const tools::Rectangle aMtfRect;
};

struct RenderTarget
{
    const tools::Rectangle aLogicalRect;
    const tools::Rectangle aPixelRect;
};

struct CompositionEffect
{
    const Gradient& rGradient;
    bool bNeedsCopyCycle;
};
} // end anonymous namespace

static std::pair<Bitmap, Bitmap> lcl_RenderTransparentComponents(const Renderers& rRenderers,
                                                                 const RenderSource& rSource,
                                                                 const RenderTarget& rTarget,
                                                                 const CompositionEffect& rEffect)
{
    const Size aDstSzPixel = rTarget.aPixelRect.GetSize();
    const bool bBufferMapModeEnabled = rRenderers.rBuffer.IsMapModeEnabled();

    // Ensure the buffer's MapMode is restored if we return early
    comphelper::ScopeGuard aBufferMapGuard([&rRenderers, bBufferMapModeEnabled]() {
        rRenderers.rBuffer.EnableMapMode(bBufferMapModeEnabled);
    });

    // Phase 1: Render Content (Paint)
    if (rEffect.bNeedsCopyCycle)
    {
        rRenderers.rBuffer.EnableMapMode(false);
        rRenderers.rBuffer.DrawOutDev(Point(), aDstSzPixel, rTarget.aPixelRect.TopLeft(),
                                      aDstSzPixel, rRenderers.rOutDev);
    }

    rRenderers.rBuffer.EnableMapMode(bBufferMapModeEnabled);
    const_cast<GDIMetaFile&>(rSource.rMtf).WindStart();
    const_cast<GDIMetaFile&>(rSource.rMtf).Play(rRenderers.rBuffer, rSource.aMtfRect.TopLeft(), rSource.aMtfRect.GetSize());
    const_cast<GDIMetaFile&>(rSource.rMtf).WindStart();

    rRenderers.rBuffer.EnableMapMode(false);
    Bitmap aPaint = rRenderers.rBuffer.GetBitmap(Point(), aDstSzPixel);

    // Phase 2: Render Gradient Mask
    rRenderers.rBuffer.EnableMapMode(bBufferMapModeEnabled);
    rRenderers.rBuffer.SetDrawMode(DrawModeFlags::GrayGradient);
    rRenderers.rBuffer.DrawGradient(rTarget.aLogicalRect, rEffect.rGradient);
    rRenderers.rBuffer.SetDrawMode(DrawModeFlags::Default);

    rRenderers.rBuffer.EnableMapMode(false);
    Bitmap aGradientMask = rRenderers.rBuffer.GetBitmap(Point(), aDstSzPixel);

    return { aPaint, aGradientMask };
}

void OutputDevice::DrawTransparent(const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                                   const Point& rMtfPos, const Size& rMtfSize,
                                   const Gradient& rTransparenceGradient)
{
    assert(!is_double_buffered_window());

    maRecorder.RecordFloatTransparent(rMtf, rPos, rSize, rTransparenceGradient);

    if (!IsDeviceOutputNecessary())
        return;

    if (lcl_IsEffectivelyOpaque(rTransparenceGradient, mpGraphicsState->mnDrawMode))
    {
        const_cast<GDIMetaFile&>(rMtf).WindStart();
        const_cast<GDIMetaFile&>(rMtf).Play(*this, rMtfPos, rMtfSize);
        const_cast<GDIMetaFile&>(rMtf).WindStart();
        return;
    }

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    tools::Rectangle aOutRect(LogicToPixel(tools::Rectangle(rPos, rSize)));
    tools::Rectangle aDstRect = GetVisibleDeviceRangePixel(tools::PolyPolygon(aOutRect));

    if (aDstRect.IsEmpty())
        return;

    const bool bNeedsCopyCycle = (GetAntialiasing() != AntialiasingFlags::NONE || rPos != rMtfPos
                                  || rSize != rMtfSize);

    ScopedVclPtrInstance<VirtualDevice> xOffscreenBuffer(DeviceFormat::WITH_ALPHA);
    if (!lcl_ConfigureOffscreenBuffer(*xOffscreenBuffer, *this, aDstRect, bNeedsCopyCycle))
        return;

    const Renderers aRenderers{ *this, *xOffscreenBuffer };
    const RenderSource aSource{ rMtf, tools::Rectangle(rMtfPos, rMtfSize) };
    const RenderTarget aTarget{ tools::Rectangle(rPos, rSize), aDstRect };
    const CompositionEffect aEffect{ rTransparenceGradient, bNeedsCopyCycle };

    auto [aPaint, aGradientMask] = lcl_RenderTransparentComponents(aRenderers, aSource, aTarget, aEffect);

    xOffscreenBuffer.disposeAndClear();

    Bitmap aResult = vcl::rendercontext::BitmapRenderer::ApplyGradientAlpha(aPaint, aGradientMask);

    {
        const bool bOrigMapModeEnabled = mpMapper->IsMapModeEnabled();
        comphelper::ScopeGuard aMapperGuard([this, bOrigMapModeEnabled]() {
            mpMapper->EnableMapMode(bOrigMapModeEnabled);
        });

        mpMapper->EnableMapMode(false);
        DrawBitmap(aDstRect.TopLeft(), aResult);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
