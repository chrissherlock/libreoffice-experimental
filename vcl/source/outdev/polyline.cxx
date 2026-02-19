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

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly)
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolyLine(rPoly);

    if (rPoly.GetSize() < 2)
        return;



    if (!CanDrawPolyline())
        return;

    if (RasterOp::OverPaint == GetRasterOp() && IsLineColor())
    {
        basegfx::B2DPolygon aB2DPoly(rPoly.getB2DPolygon());
        const bool bPixelSnapHairline = (mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
                                        && aB2DPoly.count() < 1000;

        if (mpGraphics->DrawPolyLine(mpMapper->GetDeviceTransformation(), aB2DPoly, 0.0, 0.0, nullptr,
                                     basegfx::B2DLineJoin::Miter, css::drawing::LineCap::LineCap_BUTT, 15.0,
                                     bPixelSnapHairline, *this))
        {
            return;
        }
    }

    tools::Polygon aDevicePoly = mpMapper->LogicToDevicePixel(rPoly);
    lcl_DrawHairlineToolsPolygon(*mpGraphics, *this, aDevicePoly);
}

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    assert(!is_double_buffered_window());

    if (rLineInfo.IsDefault())
    {
        DrawPolyLine(rPoly);
        return;
    }

    if (IsDeviceOutputNecessary())
    {
        auto eLineStyle = rLineInfo.GetStyle();
        switch (eLineStyle)
        {
            case LineStyle::NONE:
            case LineStyle::Dash:
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

    if (!PrepareGraphicsOutput(false) || !mpGraphics || rPoly.GetSize() < 2)
        return;

    const LineInfo aInfo(mpMapper->LogicToDevicePixel(rLineInfo));

    if (aInfo.GetStyle() == LineStyle::Dash || aInfo.GetWidth() > 1)
    {
        basegfx::B2DPolygon aPoly = mpMapper->LogicToDevicePixel(rPoly.getB2DPolygon());
        DrawPolyLineGeometry(basegfx::B2DPolyPolygon(aPoly), aInfo);
    }
    else
    {
        tools::Polygon aDevicePoly = mpMapper->LogicToDevicePixel(rPoly);
        lcl_DrawHairlineToolsPolygon(*mpGraphics, *this, aDevicePoly);
    }
}

void OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2DPolygon,
                                double fLineWidth,
                                basegfx::B2DLineJoin eLineJoin,
                                css::drawing::LineCap eLineCap,
                                double fMiterMinimumAngle)
{
    // Delegate entirely to our new master pipeline!
    DrawPolyLineDirect(basegfx::B2DHomMatrix(), rB2DPolygon, fLineWidth, 0.0,
                       nullptr, eLineJoin, eLineCap, fMiterMinimumAngle);
}

static std::pair<basegfx::B2DPolyPolygon, LineInfo>
lcl_SetupStrokeAndLineInfo(const basegfx::B2DPolygon& rDevicePoly,
                           const std::vector<double>* pStroke,
                           double fLineWidth,
                           basegfx::B2DLineJoin eLineJoin,
                           css::drawing::LineCap eLineCap)
{
    basegfx::B2DPolyPolygon aPolyPolygon(rDevicePoly);

    // Handle Custom Dashing directly using basegfx
    if (pStroke && !pStroke->empty())
    {
        basegfx::B2DPolyPolygon aDashedPolyPoly;
        basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(rDevicePoly), *pStroke, &aDashedPolyPoly);
        aPolyPolygon = aDashedPolyPoly;
    }

    // Package the remaining attributes into VCL's legacy struct
    LineInfo aInfo;
    aInfo.SetWidth(std::round(fLineWidth));
    aInfo.SetLineJoin(eLineJoin);
    aInfo.SetLineCap(eLineCap);

    return { std::move(aPolyPolygon), aInfo };
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
    auto drawB2DPolyline = [&]() -> bool
    {
        assert(!is_double_buffered_window());

        if (!rB2DPolygon.count())
            return true;

        if ((!mpGraphics && !AcquireGraphics()) || !CanDrawPolyline())
            return false;

        const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation() * rObjectTransform);

        if (GetRasterOp() == RasterOp::OverPaint && IsLineColor())
        {
            const bool bPixelSnapHairline = (mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
                                            && rB2DPolygon.count() < 1000;

            bool bDone = mpGraphics->DrawPolyLine(
                aTransform,
                rB2DPolygon,
                fTransparency,
                fLineWidth,
                pStroke,
                eLineJoin,
                eLineCap,
                fMiterMinimumAngle,
                bPixelSnapHairline,
                *this);

            if (bDone)
                return true;
        }

        basegfx::B2DPolygon aDevicePoly(rB2DPolygon);
        aDevicePoly.transform(aTransform);

        auto [aFallbackPolyPoly, aInfo] = lcl_SetupStrokeAndLineInfo(
            aDevicePoly, pStroke, fLineWidth, eLineJoin, eLineCap);

        DrawPolyLineGeometry(aFallbackPolyPoly, aInfo);

        return true;
    };

    bool bSuccess = drawB2DPolyline();

    if (bSuccess && maRecorder.IsActive())
    {
        LineInfo aLineInfo;
        if (fLineWidth != 0.0)
            aLineInfo.SetWidth(std::round(fLineWidth));

        aLineInfo.SetLineJoin(eLineJoin);
        aLineInfo.SetLineCap(eLineCap);

        // Note: We don't record the dashing/stroke here because DrawPolyLineDirect
        // historically records the logical path, but you can adjust this if needed!
        maRecorder.RecordPolyLine(tools::Polygon(rB2DPolygon), aLineInfo);
    }

    return bSuccess;
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

    if (!bFuzzing && rInfo.GetStyle() == LineStyle::Dash)
        aLinePolyPolygon = lcl_ApplyLineDashing(aLinePolyPolygon, rInfo);

    basegfx::B2DPolyPolygon aFillPolyPolygon;

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
