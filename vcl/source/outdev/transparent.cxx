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

    if (bDrawn)
        return;

    ScopedVclPtrInstance< VirtualDevice > aVDev(*this);
    const Size aDstSz( aDstRect.GetSize() );
    const sal_uInt8 cTrans = basegfx::fround<sal_uInt8>(nTransparencePercent * 2.55);

    if( aDstRect.Left() || aDstRect.Top() )
        aPolyPoly.Move( -aDstRect.Left(), -aDstRect.Top() );

    if (!aVDev->SetOutputSizePixel(aDstSz))
        return;

    const bool bOldMap = mpMapper->IsMapModeEnabled();

    mpMapper->EnableMapMode( false );

    aVDev->SetLineColor( COL_BLACK );
    aVDev->SetFillColor( COL_BLACK );
    aVDev->DrawPolyPolygon( aPolyPoly );

    Bitmap aPaint( GetBitmap( aDstRect.TopLeft(), aDstSz ) );
    Bitmap aPolyMask( aVDev->GetBitmap( Point(), aDstSz ) );

    // #107766# check for non-empty bitmaps before accessing them
    if (aPaint.IsEmpty() || aPolyMask.IsEmpty())
        return;

    vcl::rendercontext::BitmapRenderer::BlendAlphaBitmap(aPaint, aPolyMask, GetFillColor(), cTrans);

    DrawBitmap( aDstRect.TopLeft(), aPaint );

    mpMapper->EnableMapMode( bOldMap );

    if( mpGraphicsState->mbLineColor )
    {
        auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR);
        SetFillColor();
        DrawPolyPolygon( rPolyPoly );
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

void OutputDevice::DrawTransparent( const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                                    const Point& rMtfPos, const Size& rMtfSize,
                                    const Gradient& rTransparenceGradient )
{
    assert(!is_double_buffered_window());

    const Color aBlack( COL_BLACK );

    maRecorder.RecordFloatTransparent(rMtf, rPos, rSize, rTransparenceGradient);

    if ( !IsDeviceOutputNecessary() )
        return;

    if( ( rTransparenceGradient.GetStartColor() == aBlack && rTransparenceGradient.GetEndColor() == aBlack ) ||
        ( mpGraphicsState->mnDrawMode & DrawModeFlags::NoTransparency ) )
    {
        const_cast<GDIMetaFile&>(rMtf).WindStart();
        const_cast<GDIMetaFile&>(rMtf).Play(*this, rMtfPos, rMtfSize);
        const_cast<GDIMetaFile&>(rMtf).WindStart();

        return;
    }

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    tools::Rectangle aOutRect( LogicToPixel( tools::Rectangle(rPos, rSize) ) );
    Point aPoint;
    tools::Rectangle aDstRect( aPoint, GetOutputSizePixel() );
    aDstRect.Intersection( aOutRect );

    if (HasClipRegion())
        aDstRect.Intersection( LogicToPixel( GetClipRegion().GetBoundRect() ) );

    if (aDstRect.IsEmpty())
        return;

    // Create transparent buffer
    ScopedVclPtrInstance<VirtualDevice> xVDev(DeviceFormat::WITH_ALPHA);

    xVDev->SetDPIX(GetDPIX());
    xVDev->SetDPIY(GetDPIY());

    if (!xVDev->SetOutputSizePixel(aDstRect.GetSize(), true, true))
        return;

    // tdf#150610 fix broken rendering of text meta actions
    // Even when drawing to a VirtualDevice that has antialiasing
    // disabled, text will still be drawn with some antialiased
    // pixels on HiDPI displays. So, use the antialiasing enabled
    // code to render if there are any text meta actions in the
    // metafile.
    if (GetAntialiasing() != AntialiasingFlags::NONE || rPos != rMtfPos || rSize != rMtfSize)
    {
        // #i102109#
        // For MetaFile replay (see task) it may now be necessary to take
        // into account that the content is AntiAlialiased and needs to be masked
        // like that. Instead of masking, i will use a copy-modify-paste cycle
        // here (as i already use in the VclPrimiziveRenderer with success)
        xVDev->SetAntialiasing(GetAntialiasing());

        // create MapMode for buffer (offset needed) and set
        MapMode aMap(GetMapMode());
        const Point aOutPos(PixelToLogic(aDstRect.TopLeft()));
        aMap.SetOrigin(Point(-aOutPos.X(), -aOutPos.Y()));
        xVDev->SetMapMode(aMap);

        // copy MapMode state and disable for target
        const bool bOrigMapModeEnabled(mpMapper->IsMapModeEnabled());
        mpMapper->EnableMapMode(false);

        // copy MapMode state and disable for buffer
        const bool bBufferMapModeEnabled(xVDev->IsMapModeEnabled());
        xVDev->EnableMapMode(false);

        // copy content from original to buffer
        xVDev->DrawOutDev( aPoint, xVDev->GetOutputSizePixel(), // dest
                           aDstRect.TopLeft(), xVDev->GetOutputSizePixel(), // source
                           *this);

        // draw MetaFile to buffer
        xVDev->EnableMapMode(bBufferMapModeEnabled);
        const_cast<GDIMetaFile&>(rMtf).WindStart();
        const_cast<GDIMetaFile&>(rMtf).Play(*xVDev, rMtfPos, rMtfSize);
        const_cast<GDIMetaFile&>(rMtf).WindStart();

        // get content bitmap from buffer
        xVDev->EnableMapMode(false);

        const Bitmap aPaint(xVDev->GetBitmap(aPoint, xVDev->GetOutputSizePixel()));

        // create alpha mask from gradient and get as Bitmap
        xVDev->EnableMapMode(bBufferMapModeEnabled);
        xVDev->SetDrawMode(DrawModeFlags::GrayGradient);
        // Related tdf#150610 draw gradient to VirtualDevice bounds
        // If we are here and the metafile bounds differs from the
        // VirtualDevice bounds so that we apply the transparency
        // gradient to any pixels drawn outside of the metafile
        // bounds.
        xVDev->DrawGradient(tools::Rectangle(rPos, rSize), rTransparenceGradient);
        xVDev->SetDrawMode(DrawModeFlags::Default);
        xVDev->EnableMapMode(false);

        AlphaMask aAlpha(xVDev->GetBitmap(aPoint, xVDev->GetOutputSizePixel()));
        const AlphaMask aPaintAlpha(aPaint.CreateAlphaMask());
        // The alpha mask is inverted from what
        // is expected so invert it again
        aAlpha.Invert(); // convert to alpha
        aAlpha.BlendWith(aPaintAlpha);

        xVDev.disposeAndClear();

        // draw masked content to target and restore MapMode
        DrawBitmap(aDstRect.TopLeft(), Bitmap(aPaint.CreateColorBitmap(), aAlpha));
        mpMapper->EnableMapMode(bOrigMapModeEnabled);

        return;
    }

    MapMode aMap( GetMapMode() );
    Point aOutPos( PixelToLogic( aDstRect.TopLeft() ) );
    const bool bOldMap = mpMapper->IsMapModeEnabled();

    aMap.SetOrigin( Point( -aOutPos.X(), -aOutPos.Y() ) );
    xVDev->SetMapMode( aMap );
    const bool bVDevOldMap = xVDev->IsMapModeEnabled();

    // create paint bitmap
    const_cast<GDIMetaFile&>(rMtf).WindStart();
    const_cast<GDIMetaFile&>(rMtf).Play(*xVDev, rMtfPos, rMtfSize);
    const_cast<GDIMetaFile&>(rMtf).WindStart();
    xVDev->EnableMapMode( false );
    Bitmap aPaint(xVDev->GetBitmap(Point(), xVDev->GetOutputSizePixel()));
    xVDev->EnableMapMode( bVDevOldMap ); // #i35331#: MUST NOT use EnableMapMode( sal_True ) here!

    // create alpha mask from gradient
    xVDev->SetDrawMode( DrawModeFlags::GrayGradient );
    xVDev->DrawGradient( tools::Rectangle( rMtfPos, rMtfSize ), rTransparenceGradient );
    xVDev->SetDrawMode( DrawModeFlags::Default );
    xVDev->EnableMapMode( false );

    AlphaMask aAlpha(xVDev->GetBitmap(Point(), xVDev->GetOutputSizePixel()));
    const AlphaMask aPaintAlpha(aPaint.CreateAlphaMask());
    // The alpha mask is inverted from what
    // is expected so invert it again
    aAlpha.Invert(); // convert to alpha
    aAlpha.BlendWith(aPaintAlpha);

    xVDev.disposeAndClear();

    mpMapper->EnableMapMode( false );
    DrawBitmap(aDstRect.TopLeft(), Bitmap(aPaint.CreateColorBitmap(), aAlpha));
    mpMapper->EnableMapMode( bOldMap );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
