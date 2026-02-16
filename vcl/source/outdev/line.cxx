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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <tools/debug.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>

#include <vcl/metafile/MetafileRecorder.hxx>
#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <numeric>

const Color& OutputDevice::GetLineColor() const
{
    return mpGraphicsState->maLineColor;
}

bool OutputDevice::IsLineColor() const
{
    return mpGraphicsState->mbLineColor;
}

void OutputDevice::SetLineColor()
{
    maRecorder.RecordLineColor( Color(), false );

    if (mpGraphicsState->mbLineColor)
    {
        mbLineColorDirty = true;
        mpGraphicsState->mbLineColor = false;
        mpGraphicsState->maLineColor = COL_TRANSPARENT;
    }
}

void OutputDevice::SetLineColor(const Color& rColor)
{
    Color aColor = vcl::drawmode::GetLineColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    maRecorder.RecordLineColor( aColor, true );

    if (mpGraphicsState->maLineColor != aColor)
    {
        mbLineColorDirty = true;
        mpGraphicsState->mbLineColor = true;
        mpGraphicsState->maLineColor = aColor;
    }
}

void OutputDevice::InitLineColor()
{
    DBG_TESTSOLARMUTEX();

    if( mpGraphicsState->mbLineColor )
    {
        if( RasterOp::N0 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::N0 );
        else if( RasterOp::N1 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::N1 );
        else if( RasterOp::Invert == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::Invert );
        else
            mpGraphics->SetLineColor(mpGraphicsState->maLineColor);
    }
    else
    {
        mpGraphics->SetLineColor();
    }

    mbLineColorDirty = false;
}

void OutputDevice::DrawLine( const Point& rStartPt, const Point& rEndPt )
{
    assert(!is_double_buffered_window());

    maRecorder.RecordLine(rStartPt, rEndPt);

    // Unified state flush (passing false because standard lines don't use FillColor)
    if ( !FlushGraphicsState(false) )
        return;

    // Determine state for Anti-Aliasing
    const bool bTryAA = (RasterOp::OverPaint == GetRasterOp() && IsLineColor());
    const bool bPixelSnapHairline = (mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline) == AntialiasingFlags::PixelSnapHairline;

    // Hand off the math and low-level dispatch to the facade
    vcl::rendercontext::PrimitiveRenderer::DrawLine(*mpGraphics, *mpMapper, this,
                                                    rStartPt, rEndPt, bTryAA, bPixelSnapHairline);
}

void OutputDevice::DrawLine( const Point& rStartPt, const Point& rEndPt,
                             const LineInfo& rLineInfo )
{
    assert(!is_double_buffered_window());

    // Fallback for default lines
    if (rLineInfo.IsDefault())
    {
        DrawLine( rStartPt, rEndPt );
        return;
    }

    maRecorder.RecordLine(rStartPt, rEndPt, rLineInfo);

    if (rLineInfo.GetStyle() == LineStyle::NONE)
        return;

    // Unified state flush!
    if ( !FlushGraphicsState(false) )
        return;

    const LineInfo aInfo(mpMapper->LogicToDevicePixel(rLineInfo));
    const bool bDashUsed(LineStyle::Dash == aInfo.GetStyle());
    const bool bLineWidthUsed(aInfo.GetWidth() > 1);

    if (bDashUsed || bLineWidthUsed)
    {
        // Only map coordinates if we are inflating the polygon for dashing/width
        const Point aStartPt(LogicToDevicePixel(rStartPt));
        const Point aEndPt(LogicToDevicePixel(rEndPt));

        basegfx::B2DPolygon aLinePolygon;
        aLinePolygon.append(basegfx::B2DPoint(aStartPt.X(), aStartPt.Y()));
        aLinePolygon.append(basegfx::B2DPoint(aEndPt.X(), aEndPt.Y()));

        drawLine(basegfx::B2DPolyPolygon(aLinePolygon), aInfo);
    }
    else
    {
        // Simple solid hairline
        vcl::rendercontext::PrimitiveRenderer::DrawLine(*mpGraphics, *mpMapper, this, rStartPt, rEndPt, false, false);
    }
}

void OutputDevice::drawLine( basegfx::B2DPolyPolygon aLinePolyPolygon, const LineInfo& rInfo )
{
    static const bool bFuzzing = comphelper::IsFuzzing();
    const bool bTryB2d(RasterOp::OverPaint == GetRasterOp() && IsLineColor());
    basegfx::B2DPolyPolygon aFillPolyPolygon;
    const bool bDashUsed(LineStyle::Dash == rInfo.GetStyle());
    const bool bLineWidthUsed(rInfo.GetWidth() > 1);

    if (!bFuzzing && bDashUsed && aLinePolyPolygon.count())
    {
        ::std::vector< double > fDotDashArray = rInfo.GetDotDashArray();
        const double fAccumulated(::std::accumulate(fDotDashArray.begin(), fDotDashArray.end(), 0.0));

        if(fAccumulated > 0.0)
        {
            basegfx::B2DPolyPolygon aResult;

            for(auto const& rPolygon : std::as_const(aLinePolyPolygon))
            {
                basegfx::B2DPolyPolygon aLineTarget;
                basegfx::utils::applyLineDashing(
                    rPolygon,
                    fDotDashArray,
                    &aLineTarget);
                aResult.append(aLineTarget);
            }

            aLinePolyPolygon = std::move(aResult);
        }
    }

    if(bLineWidthUsed && aLinePolyPolygon.count())
    {
        const double fHalfLineWidth((rInfo.GetWidth() * 0.5) + 0.5);

        if(aLinePolyPolygon.areControlPointsUsed())
        {
            // #i110768# When area geometry has to be created, do not
            // use the fallback bezier decomposition inside createAreaGeometry,
            // but one that is at least as good as ImplSubdivideBezier was.
            // There, Polygon::AdaptiveSubdivide was used with default parameter
            // 1.0 as quality index.
            static int nRecurseLimit = comphelper::IsFuzzing() ? 10 : 30;
            aLinePolyPolygon = basegfx::utils::adaptiveSubdivideByDistance(aLinePolyPolygon, 1.0, nRecurseLimit);
        }

        for(auto const& rPolygon : std::as_const(aLinePolyPolygon))
        {
            aFillPolyPolygon.append(basegfx::utils::createAreaGeometry(
                rPolygon,
                fHalfLineWidth,
                rInfo.GetLineJoin(),
                rInfo.GetLineCap()));
        }

        aLinePolyPolygon.clear();
    }

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);


    if(aLinePolyPolygon.count())
    {
        for(auto const& rB2DPolygon : std::as_const(aLinePolyPolygon))
        {
            const bool bPixelSnapHairline(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);
            bool bDone(false);

            if(bTryB2d)
            {
                bDone = mpGraphics->DrawPolyLine(
                    basegfx::B2DHomMatrix(),
                    rB2DPolygon,
                    0.0,
                    0.0, // tdf#124848 hairline
                    nullptr, // MM01
                    basegfx::B2DLineJoin::NONE,
                    css::drawing::LineCap_BUTT,
                    basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                    bPixelSnapHairline,
                    *this);
            }

            if(!bDone)
            {
                tools::Polygon aPolygon(rB2DPolygon);
                mpGraphics->DrawPolyLine(
                    aPolygon.GetSize(),
                    aPolygon.GetPointAry(),
                    *this);
            }
        }
    }

    if(aFillPolyPolygon.count())
    {
        const Color aOldLineColor(mpGraphicsState->maLineColor);
        const Color aOldFillColor(mpGraphicsState->maFillColor);

        SetLineColor();
        InitLineColor();
        SetFillColor( aOldLineColor );
        InitFillColor();

        bool bDone(false);

        if (bFuzzing)
        {
            const basegfx::B2DRange aRange(aFillPolyPolygon.getB2DRange());
            if (aRange.getMaxX() - aRange.getMinX() > 0x10000000
                || aRange.getMaxY() - aRange.getMinY() > 0x10000000)
            {
                SAL_WARN("vcl.gdi", "drawLine, skipping suspicious range of: "
                                        << aRange << " for fuzzing performance");
                bDone = true;
            }
        }

        if (bTryB2d && !bDone)
        {
            mpGraphics->DrawPolyPolygon(
                basegfx::B2DHomMatrix(),
                aFillPolyPolygon,
                0.0,
                *this);
            bDone = true;
        }

        if(!bDone)
        {
            for(auto const& rB2DPolygon : std::as_const(aFillPolyPolygon))
            {
                tools::Polygon aPolygon(rB2DPolygon);

                // need to subdivide, mpGraphics->DrawPolygon ignores curves
                aPolygon.AdaptiveSubdivide(aPolygon);
                mpGraphics->DrawPolygon(aPolygon.GetSize(), aPolygon.GetConstPointAry(), *this);
            }
        }

        SetFillColor( aOldFillColor );
        SetLineColor( aOldLineColor );
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
