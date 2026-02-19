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
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <comphelper/configuration.hxx>

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

static void lcl_DrawHairlineToolsPolygon(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                         const tools::Polygon& rPoly)
{
    sal_uInt16 nPoints = rPoly.GetSize();
    const Point* pPtAry = rPoly.GetConstPointAry();

    // #100127# Forward beziers to sal, if any
    if (rPoly.HasFlags())
    {
        const PolyFlags* pFlgAry = rPoly.GetConstFlagAry();
        if (!rGraphics.DrawPolyLineBezier(nPoints, pPtAry, pFlgAry, rOutDev))
        {
            tools::Polygon aSubdivided = tools::Polygon::SubdivideBezier(rPoly);
            rGraphics.DrawPolyLine(aSubdivided.GetSize(), aSubdivided.GetConstPointAry(), rOutDev);
        }
    }
    else
    {
        rGraphics.DrawPolyLine(nPoints, pPtAry, rOutDev);
    }
}

static bool lcl_TryDirectHairline(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                  const basegfx::B2DPolygon& rB2DPoly,
                                  const basegfx::B2DHomMatrix& rTransform,
                                  bool bPixelSnap)
{
    return rGraphics.DrawPolyLine(
        rTransform,
        rB2DPoly,
        0.0,
        0.0, // hairline
        nullptr,
        basegfx::B2DLineJoin::NONE,
        css::drawing::LineCap_BUTT,
        basegfx::deg2rad(15.0),
        bPixelSnap,
        rOutDev);
}

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly)
{
    assert(!is_double_buffered_window());

    maRecorder.RecordPolyLine(rPoly);

    if (!PrepareGraphicsOutput(false) || !mpGraphics || rPoly.GetSize() < 2)
        return;

    if (DrawPolyLineDirectInternal(basegfx::B2DHomMatrix(), rPoly.getB2DPolygon()))
        return;

    const basegfx::B2DPolygon aB2DPolyLine(rPoly.getB2DPolygon());
    const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation());
    const bool bPixelSnap = bool(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    if (lcl_TryDirectHairline(*mpGraphics, *this, aB2DPolyLine, aTransform, bPixelSnap))
        return;

    tools::Polygon aDevicePoly = mpMapper->LogicToDevicePixel(rPoly);
    lcl_DrawHairlineToolsPolygon(*mpGraphics, *this, aDevicePoly);
}

void OutputDevice::DrawPolyLine( const tools::Polygon& rPoly, const LineInfo& rLineInfo )
{
    assert(!is_double_buffered_window());

    if ( rLineInfo.IsDefault() )
    {
        DrawPolyLine( rPoly );
        return;
    }

    if (IsDeviceOutputNecessary())
    {
        auto eLineStyle = rLineInfo.GetStyle();
        switch (eLineStyle)
        {
            case LineStyle::NONE:
            case LineStyle::Dash:
                // use drawPolyLine for these
                break;
            case LineStyle::Solid:
                // #i101491# Try direct Fallback to B2D-Version of DrawPolyLine
                DrawPolyLine(
                    rPoly.getB2DPolygon(),
                    rLineInfo.GetWidth(),
                    rLineInfo.GetLineJoin(),
                    rLineInfo.GetLineCap(),
                    basegfx::deg2rad(15.0) /* default fMiterMinimumAngle, value not available in LineInfo */);
                return;
            default:
                SAL_WARN("vcl.gdi", "Unknown LineStyle: " << static_cast<int>(eLineStyle));
                return;
        }
    }

    maRecorder.RecordPolyLine(rPoly, rLineInfo);

    drawPolyLine(rPoly, rLineInfo);
}

void OutputDevice::DrawPolyLine( const basegfx::B2DPolygon& rB2DPolygon,
                                 double fLineWidth,
                                 basegfx::B2DLineJoin eLineJoin,
                                 css::drawing::LineCap eLineCap,
                                 double fMiterMinimumAngle)
{
    assert(!is_double_buffered_window());

    if( maRecorder.IsActive() )
    {
        LineInfo aLineInfo;
        if( fLineWidth != 0.0 )
            aLineInfo.SetWidth( fLineWidth );

        aLineInfo.SetLineJoin(eLineJoin);
        aLineInfo.SetLineCap(eLineCap);

        tools::Polygon aToolsPolygon( rB2DPolygon );
        maRecorder.RecordPolyLine( aToolsPolygon, aLineInfo );
    }

    // Do not paint empty PolyPolygons
    if(!rB2DPolygon.count() || !IsDeviceOutputNecessary())
        return;

    // we need a graphics
    if( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    if( mbLineColorDirty )
        InitLineColor();

    // use b2dpolygon drawing if possible
    if(DrawPolyLineDirectInternal(
        basegfx::B2DHomMatrix(),
        rB2DPolygon,
        fLineWidth,
        0.0,
        nullptr, // MM01
        eLineJoin,
        eLineCap,
        fMiterMinimumAngle))
    {
        return;
    }

    // #i101491#
    // no output yet; fallback to geometry decomposition and use filled polygon paint
    // when line is fat and not too complex. ImplDrawPolyPolygonWithB2DPolyPolygon
    // will do internal needed AA checks etc.
    if(fLineWidth >= 2.5 &&
       rB2DPolygon.count() &&
       rB2DPolygon.count() <= 1000)
    {
        const double fHalfLineWidth((fLineWidth * 0.5) + 0.5);
        const basegfx::B2DPolyPolygon aAreaPolyPolygon(
                basegfx::utils::createAreaGeometry( rB2DPolygon,
                                                    fHalfLineWidth,
                                                    eLineJoin,
                                                    eLineCap,
                                                    fMiterMinimumAngle));
        const Color aOldLineColor(mpGraphicsState->maLineColor);
        const Color aOldFillColor(mpGraphicsState->maFillColor);

        SetLineColor();
        InitLineColor();
        SetFillColor(aOldLineColor);
        InitFillColor();

        // draw using a loop; else the topology will paint a PolyPolygon
        for(auto const& rPolygon : aAreaPolyPolygon)
        {
            ImplDrawPolyPolygonWithB2DPolyPolygon(
                basegfx::B2DPolyPolygon(rPolygon));
        }

        SetLineColor(aOldLineColor);
        InitLineColor();
        SetFillColor(aOldFillColor);
        InitFillColor();

        // when AA it is necessary to also paint the filled polygon's outline
        // to avoid optical gaps
        for(auto const& rPolygon : aAreaPolyPolygon)
        {
            (void)DrawPolyLineDirectInternal(
                basegfx::B2DHomMatrix(),
                rPolygon);
        }
    }
    else
    {
        // fallback to old polygon drawing if needed
        const tools::Polygon aToolsPolygon( rB2DPolygon );
        LineInfo aLineInfo;
        if( fLineWidth != 0.0 )
            aLineInfo.SetWidth( fLineWidth );

        drawPolyLine( aToolsPolygon, aLineInfo );
    }
}

void OutputDevice::drawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    sal_uInt16 nPoints(rPoly.GetSize());

    if ( !IsDeviceOutputNecessary() || !mpGraphicsState->mbLineColor || ( nPoints < 2 ) || ( LineStyle::NONE == rLineInfo.GetStyle() ) || IsLayoutCalculationNecessary() )
        return;

    // we need a graphics
    if ( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    if ( mbLineColorDirty )
        InitLineColor();

    const LineInfo aInfo(mpMapper->LogicToDevicePixel(rLineInfo));

    if (aInfo.GetStyle() == LineStyle::Dash || aInfo.GetWidth() > 1)
    {
        basegfx::B2DPolygon aPoly = mpMapper->LogicToDevicePixel(rPoly.getB2DPolygon());
        DrawPolyLineGeometry(basegfx::B2DPolyPolygon(aPoly), aInfo);
    }
    else
    {
        tools::Polygon aPoly = mpMapper->LogicToDevicePixel(rPoly);

        // #100127# the subdivision HAS to be done here since only a pointer
        // to an array of points is given to the DrawPolyLine method, there is
        // NO way to find out there that it's a curve.
        if( aPoly.HasFlags() )
        {
            aPoly = tools::Polygon::SubdivideBezier( aPoly );
            nPoints = aPoly.GetSize();
        }

        mpGraphics->DrawPolyLine(nPoints, aPoly.GetPointAry(), *this);
    }
}

bool OutputDevice::DrawPolyLineDirect(
    const basegfx::B2DHomMatrix& rObjectTransform,
    const basegfx::B2DPolygon& rB2DPolygon,
    double fLineWidth,
    double fTransparency,
    const std::vector< double >* pStroke, // MM01
    basegfx::B2DLineJoin eLineJoin,
    css::drawing::LineCap eLineCap,
    double fMiterMinimumAngle)
{
    if(DrawPolyLineDirectInternal(rObjectTransform, rB2DPolygon, fLineWidth, fTransparency,
        pStroke, eLineJoin, eLineCap, fMiterMinimumAngle))
    {
        // Worked, add metafile action (if recorded). This is done only here,
        // because this function is public, other OutDev functions already add metafile
        // actions, so they call the internal function directly.
        if( maRecorder.IsActive() )
        {
            LineInfo aLineInfo;
            if( fLineWidth != 0.0 )
                aLineInfo.SetWidth( fLineWidth );
            // Transport known information, might be needed
            aLineInfo.SetLineJoin(eLineJoin);
            aLineInfo.SetLineCap(eLineCap);
            // MiterMinimumAngle does not exist yet in LineInfo
            tools::Polygon aToolsPolygon( rB2DPolygon );
            maRecorder.RecordPolyLine( aToolsPolygon, aLineInfo );
        }
        return true;
    }
    return false;
}

bool OutputDevice::DrawPolyLineDirectInternal(
    const basegfx::B2DHomMatrix& rObjectTransform,
    const basegfx::B2DPolygon& rB2DPolygon,
    double fLineWidth,
    double fTransparency,
    const std::vector< double >* pStroke, // MM01
    basegfx::B2DLineJoin eLineJoin,
    css::drawing::LineCap eLineCap,
    double fMiterMinimumAngle)
{
    assert(!is_double_buffered_window());

    // AW: Do NOT paint empty PolyPolygons
    if(!rB2DPolygon.count())
        return true;

    // we need a graphics
    if( !mpGraphics && !AcquireGraphics() )
        return false;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return true;

    if( mbLineColorDirty )
        InitLineColor();

    const bool bTryB2d(RasterOp::OverPaint == GetRasterOp() && IsLineColor());

    if(bTryB2d)
    {
        // combine rObjectTransform with WorldToDevice
        const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation() * rObjectTransform);
        const bool bPixelSnapHairline((mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline) && rB2DPolygon.count() < 1000);

        // draw the polyline
        return mpGraphics->DrawPolyLine(
            aTransform,
            rB2DPolygon,
            fTransparency,
            fLineWidth, // tdf#124848 use LineWidth direct, do not try to solve for zero-case (aka hairline)
            pStroke, // MM01
            eLineJoin,
            eLineCap,
            fMiterMinimumAngle,
            bPixelSnapHairline,
            *this);
    }
    return false;
}

static basegfx::B2DPolyPolygon lcl_ApplyLineDashing(const basegfx::B2DPolyPolygon& rLinePolyPolygon, const LineInfo& rInfo)
{
    if (!rLinePolyPolygon.count())
        return rLinePolyPolygon;

    std::vector<double> fDotDashArray = rInfo.GetDotDashArray();
    const double fAccumulated = std::accumulate(fDotDashArray.begin(), fDotDashArray.end(), 0.0);

    if (fAccumulated <= 0.0)
        return rLinePolyPolygon;

    basegfx::B2DPolyPolygon aResult;
    for (auto const& rPolygon : rLinePolyPolygon)
    {
        basegfx::B2DPolyPolygon aLineTarget;
        basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(rPolygon), fDotDashArray, &aLineTarget);
        aResult.append(aLineTarget);
    }

    return aResult;
}

static basegfx::B2DPolyPolygon lcl_CreateAreaGeometryForLine(basegfx::B2DPolyPolygon aLinePolyPolygon, const LineInfo& rInfo)
{
    basegfx::B2DPolyPolygon aFillPolyPolygon;

    if (!aLinePolyPolygon.count())
        return aFillPolyPolygon;

    const double fHalfLineWidth((rInfo.GetWidth() * 0.5) + 0.5);

    if (aLinePolyPolygon.areControlPointsUsed())
    {
        // #i110768# When area geometry has to be created, do not
        // use the fallback bezier decomposition inside createAreaGeometry,
        // but one that is at least as good as ImplSubdivideBezier was.
        // There, Polygon::AdaptiveSubdivide was used with default parameter
        // 1.0 as quality index.
        static int nRecurseLimit = comphelper::IsFuzzing() ? 10 : 30;
        aLinePolyPolygon = basegfx::utils::adaptiveSubdivideByDistance(aLinePolyPolygon, 1.0, nRecurseLimit);
    }

    for (auto const& rPolygon : std::as_const(aLinePolyPolygon))
    {
        aFillPolyPolygon.append(basegfx::utils::createAreaGeometry(
            rPolygon,
            fHalfLineWidth,
            rInfo.GetLineJoin(),
            rInfo.GetLineCap()));
    }

    return aFillPolyPolygon;
}

static void lcl_DrawHairlinePolyPolygon(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                        const basegfx::B2DPolyPolygon& rLinePolyPolygon,
                                        bool bTryB2d, bool bPixelSnapHairline)
{
    if (!rLinePolyPolygon.count())
        return;

    for (auto const& rB2DPolygon : std::as_const(rLinePolyPolygon))
    {
        bool bDone = false;

        if (bTryB2d)
        {
            bDone = rGraphics.DrawPolyLine(
                basegfx::B2DHomMatrix(),
                rB2DPolygon,
                0.0,
                0.0, // tdf#124848 hairline
                nullptr, // MM01
                basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline,
                rOutDev);
        }

        if (!bDone)
        {
            tools::Polygon aPolygon(rB2DPolygon);
            rGraphics.DrawPolyLine(
                aPolygon.GetSize(),
                aPolygon.GetConstPointAry(), // Bypasses Copy-On-Write clone!
                rOutDev);
        }
    }
}

static bool lcl_TryDrawB2DAreaGeometry(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                       const basegfx::B2DPolyPolygon& rFillPolyPolygon,
                                       bool bFuzzing, bool bTryB2d)
{
    if (bFuzzing)
    {
        const basegfx::B2DRange aRange(rFillPolyPolygon.getB2DRange());
        if (aRange.getMaxX() - aRange.getMinX() > 0x10000000
            || aRange.getMaxY() - aRange.getMinY() > 0x10000000)
        {
            SAL_WARN("vcl.gdi", "drawLine, skipping suspicious range of: "
                                    << aRange << " for fuzzing performance");
            return true;
        }
    }

    if (!bTryB2d)
        return false;

    rGraphics.DrawPolyPolygon(basegfx::B2DHomMatrix(), rFillPolyPolygon, 0.0, rOutDev);
    return true;
}

static void lcl_DrawSubdividedAreaGeometry(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                           const basegfx::B2DPolyPolygon& rFillPolyPolygon)
{
    for (auto const& rB2DPolygon : rFillPolyPolygon)
    {
        tools::Polygon aPolygon(rB2DPolygon);

        // need to subdivide, mpGraphics->DrawPolygon ignores curves
        aPolygon.AdaptiveSubdivide(aPolygon);
        rGraphics.DrawPolygon(aPolygon.GetSize(), aPolygon.GetConstPointAry(), rOutDev);
    }
}

static void lcl_DrawAreaGeometry(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                 const basegfx::B2DPolyPolygon& rFillPolyPolygon,
                                 bool bFuzzing, bool bTryB2d)
{
    if (!rFillPolyPolygon.count())
        return;

    // Use public accessors since mpGraphicsState is protected
    const Color aOldLineColor(rOutDev.GetLineColor());
    const Color aOldFillColor(rOutDev.GetFillColor());

    comphelper::ScopeGuard aColorGuard([&rOutDev, aOldLineColor, aOldFillColor]() {
        rOutDev.SetFillColor(aOldFillColor);
        rOutDev.SetLineColor(aOldLineColor);
    });

    // Set temporary colors for the thick line/area fill
    rOutDev.SetLineColor();
    rOutDev.SetFillColor(aOldLineColor);

    // Push the temporary colors to the hardware backend
    rOutDev.FlushGraphicsState();

    if (lcl_TryDrawB2DAreaGeometry(rGraphics, rOutDev, rFillPolyPolygon, bFuzzing, bTryB2d))
        return;

    // If B2D rendering failed or wasn't attempted, fallback to subdivided polygons
    lcl_DrawSubdividedAreaGeometry(rGraphics, rOutDev, rFillPolyPolygon);
}

/**
 * Processes line geometry based on dashing and width.
 * Returns a pair: first = remaining hairline geometry, second = generated area geometry.
 */
static std::pair<basegfx::B2DPolyPolygon, basegfx::B2DPolyPolygon>
lcl_ProcessLineGeometry(basegfx::B2DPolyPolygon aLinePolyPolygon, const LineInfo& rInfo)
{
    static const bool bFuzzing = comphelper::IsFuzzing();

    // 1. Apply Dashing
    if (!bFuzzing && rInfo.GetStyle() == LineStyle::Dash)
        aLinePolyPolygon = lcl_ApplyLineDashing(aLinePolyPolygon, rInfo);

    basegfx::B2DPolyPolygon aFillPolyPolygon;

    // 2. Convert thick lines to area geometry
    if (rInfo.GetWidth() > 1 && aLinePolyPolygon.count())
    {
        aFillPolyPolygon.append(lcl_CreateAreaGeometryForLine(aLinePolyPolygon, rInfo));
        aLinePolyPolygon.clear();
    }

    return { std::move(aLinePolyPolygon), std::move(aFillPolyPolygon) };
}

void OutputDevice::DrawPolyLineGeometry(const basegfx::B2DPolyPolygon& rPolyPolygon,
                                        const LineInfo& rLineInfo)
{
    auto [aHairlines, aFillGeometry] = lcl_ProcessLineGeometry(rPolyPolygon, rLineInfo);

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    const bool bTryB2d = (GetRasterOp() == RasterOp::OverPaint && IsLineColor());
    const bool bPixelSnapHairline = bool(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);
    static const bool bFuzzing = comphelper::IsFuzzing();

    lcl_DrawHairlinePolyPolygon(*mpGraphics, *this, aHairlines, bTryB2d, bPixelSnapHairline);
    lcl_DrawAreaGeometry(*mpGraphics, *this, aFillGeometry, bFuzzing, bTryB2d);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
