/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/gen.hxx>
#include <iostream>
#include <tools/color.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <sal/log.hxx>
#include <comphelper/scopeguard.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <GraphicsState.hxx>
#include <comphelper/configuration.hxx>
#include <vcl/lineinfo.hxx>

#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/outdev.hxx>

#include <salgdi.hxx>
#include <CoordinateMapper.hxx>

#include <com/sun/star/drawing/LineCap.hpp>

namespace vcl::rendercontext
{
void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const OutputDevice* pOutDev, const Point& rLogicalPt)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    rGraphics.DrawPixel(aDevicePt.X(), aDevicePt.Y(), *pOutDev);
}

void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const OutputDevice* pOutDev, const Point& rLogicalPt,
                                  const Color& rColor)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    rGraphics.DrawPixel(aDevicePt.X(), aDevicePt.Y(), rColor, *pOutDev);
}

Color PrimitiveRenderer::GetPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const OutputDevice* pOutDev, const Point& rLogicalPt)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    return rGraphics.GetPixel(aDevicePt.X(), aDevicePt.Y(), *pOutDev);
}

void PrimitiveRenderer::DrawLine(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                 const OutputDevice* pOutDev, const Point& rLogicalStart,
                                 const Point& rLogicalEnd, bool bTryAA, bool bPixelSnapHairline)
{
    std::cerr << "\n=== FACADE TRACE ===\n";
    std::cerr << "[DrawLine] Logical: " << rLogicalStart.X() << "," << rLogicalStart.Y() << " to "
              << rLogicalEnd.X() << "," << rLogicalEnd.Y() << "\n";
    std::cerr << "[DrawLine] Device: " << rMapper.LogicToDevicePixel(rLogicalStart).X() << ","
              << rMapper.LogicToDevicePixel(rLogicalStart).Y() << " to "
              << rMapper.LogicToDevicePixel(rLogicalEnd).X() << ","
              << rMapper.LogicToDevicePixel(rLogicalEnd).Y() << "\n";
    std::cerr << "[DrawLine] bTryAA: " << bTryAA << ", bPixelSnap: " << bPixelSnapHairline << "\n";

    bool bDrawn = false;

    if (bTryAA)
    {
        const basegfx::B2DHomMatrix aTransform(rMapper.GetDeviceTransformation());
        basegfx::B2DPolygon aB2DPolyLine;

        aB2DPolyLine.append(basegfx::B2DPoint(rLogicalStart.X(), rLogicalStart.Y()));
        aB2DPolyLine.append(basegfx::B2DPoint(rLogicalEnd.X(), rLogicalEnd.Y()));
        aB2DPolyLine.transform(aTransform);

        bDrawn = rGraphics.DrawPolyLine(basegfx::B2DHomMatrix(), aB2DPolyLine, 0.0,
                                        0.0, // tdf#124848 hairline
                                        nullptr, // MM01
                                        basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                                        basegfx::deg2rad(15.0), // default MiterMinimumAngle
                                        bPixelSnapHairline, *pOutDev);
    }

    if (bDrawn)
        return;

    const Point aStartDevicePt(rMapper.LogicToDevicePixel(rLogicalStart));
    const Point aEndDevicePt(rMapper.LogicToDevicePixel(rLogicalEnd));

    rGraphics.DrawLine(aStartDevicePt.X(), aStartDevicePt.Y(), aEndDevicePt.X(), aEndDevicePt.Y(),
                       *pOutDev);
}

void PrimitiveRenderer::DrawRect(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                 const OutputDevice* pOutDev, const tools::Rectangle& rLogicalRect)
{
    const tools::Rectangle aDeviceRect(rMapper.LogicToDevicePixel(rLogicalRect));

    if (!aDeviceRect.IsEmpty())
    {
        rGraphics.DrawRect(aDeviceRect.Left(), aDeviceRect.Top(), aDeviceRect.GetWidth(),
                           aDeviceRect.GetHeight(), *pOutDev);
    }
}

namespace
{
static basegfx::B2DPolyPolygon lcl_ApplyLineDashing(const basegfx::B2DPolyPolygon& rLinePolyPolygon,
                                                    const LineInfo& rInfo)
{
    if (!rLinePolyPolygon.count())
        return rLinePolyPolygon;
    std::vector<double> fDotDashArray = rInfo.GetDotDashArray();
    if (std::accumulate(fDotDashArray.begin(), fDotDashArray.end(), 0.0) <= 0.0)
        return rLinePolyPolygon;
    basegfx::B2DPolyPolygon aResult;
    for (auto const& rPolygon : rLinePolyPolygon)
    {
        basegfx::B2DPolyPolygon aLineTarget;
        basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(rPolygon), fDotDashArray,
                                         &aLineTarget);
        aResult.append(aLineTarget);
    }
    return aResult;
}

static basegfx::B2DPolyPolygon
lcl_CreateAreaGeometryForLine(basegfx::B2DPolyPolygon aLinePolyPolygon, const LineInfo& rInfo)
{
    basegfx::B2DPolyPolygon aFillPolyPolygon;
    if (!aLinePolyPolygon.count())
        return aFillPolyPolygon;
    const double fHalfLineWidth((rInfo.GetWidth() * 0.5) + 0.5);
    if (aLinePolyPolygon.areControlPointsUsed())
    {
        static int nRecurseLimit = comphelper::IsFuzzing() ? 10 : 30;
        aLinePolyPolygon
            = basegfx::utils::adaptiveSubdivideByDistance(aLinePolyPolygon, 1.0, nRecurseLimit);
    }
    for (auto const& rPolygon : std::as_const(aLinePolyPolygon))
    {
        aFillPolyPolygon.append(basegfx::utils::createAreaGeometry(
            rPolygon, fHalfLineWidth, rInfo.GetLineJoin(), rInfo.GetLineCap()));
    }
    return aFillPolyPolygon;
}

static std::pair<basegfx::B2DPolyPolygon, basegfx::B2DPolyPolygon>
lcl_ProcessLineGeometry(basegfx::B2DPolyPolygon aLinePolyPolygon, const LineInfo& rInfo)
{
    if (rInfo.GetStyle() == LineStyle::Dash)
        aLinePolyPolygon = lcl_ApplyLineDashing(aLinePolyPolygon, rInfo);
    basegfx::B2DPolyPolygon aFillPolyPolygon;
    if (rInfo.GetWidth() > 1 && aLinePolyPolygon.count())
    {
        aFillPolyPolygon.append(lcl_CreateAreaGeometryForLine(aLinePolyPolygon, rInfo));
        aLinePolyPolygon.clear();
    }
    return { std::move(aLinePolyPolygon), std::move(aFillPolyPolygon) };
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
            bDone = rGraphics.DrawPolyLine(basegfx::B2DHomMatrix(), rB2DPolygon, 0.0, 0.0, nullptr,
                                           basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                                           basegfx::deg2rad(15.0), bPixelSnapHairline, rOutDev);
        }
        if (!bDone)
        {
            tools::Polygon aPolygon(rB2DPolygon);
            rGraphics.DrawPolyLine(aPolygon.GetSize(), aPolygon.GetConstPointAry(), rOutDev);
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
            SAL_WARN("vcl.gdi", "drawLine, skipping suspicious range");
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
        aPolygon.AdaptiveSubdivide(aPolygon);
        rGraphics.DrawPolygon(aPolygon.GetSize(), aPolygon.GetConstPointAry(), rOutDev);
    }
}

static void lcl_DrawAreaGeometry(SalGraphics& rGraphics, OutputDevice& rOutDev,
                                 const basegfx::B2DPolyPolygon& rFillPolyPolygon, bool bFuzzing,
                                 bool bTryB2d)
{
    if (!rFillPolyPolygon.count())
        return;
    const Color aOldLineColor(rOutDev.GetLineColor());
    const Color aOldFillColor(rOutDev.GetFillColor());
    comphelper::ScopeGuard aColorGuard([&rOutDev, aOldLineColor, aOldFillColor]() {
        rOutDev.SetFillColor(aOldFillColor);
        rOutDev.SetLineColor(aOldLineColor);
    });
    rOutDev.SetLineColor();
    rOutDev.SetFillColor(aOldLineColor);
    rOutDev.FlushGraphicsState();
    if (lcl_TryDrawB2DAreaGeometry(rGraphics, rOutDev, rFillPolyPolygon, bFuzzing, bTryB2d))
        return;
    lcl_DrawSubdividedAreaGeometry(rGraphics, rOutDev, rFillPolyPolygon);
}

static std::pair<basegfx::B2DPolyPolygon, LineInfo>
lcl_SetupStrokeAndLineInfo(const basegfx::B2DPolygon& rDevicePoly,
                           const std::vector<double>* pStroke, double fLineWidth,
                           basegfx::B2DLineJoin eLineJoin, css::drawing::LineCap eLineCap)
{
    basegfx::B2DPolyPolygon aPolyPolygon(rDevicePoly);

    // Handle Custom Dashing directly using basegfx
    if (pStroke && !pStroke->empty())
    {
        basegfx::B2DPolyPolygon aDashedPolyPoly;
        basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(rDevicePoly), *pStroke,
                                         &aDashedPolyPoly);
        aPolyPolygon = aDashedPolyPoly;
    }

    // Package the remaining attributes into VCL's legacy struct
    LineInfo aInfo;
    aInfo.SetWidth(std::round(fLineWidth));
    aInfo.SetLineJoin(eLineJoin);
    aInfo.SetLineCap(eLineCap);

    return { std::move(aPolyPolygon), aInfo };
}
}

bool PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    if (!rOutDev.CanDrawPolygon())
        return false;

    // In headless tests, AcquireGraphics might fail or return a null mpGraphics.
    // If we can't get a graphics context, we must bail out immediately.
    if (!rOutDev.GetGraphics() && !rOutDev.AcquireGraphics())
        return false;

    // Safety check for the internal mapper
    if (!rOutDev.GetGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    // Now it is safe to ask for the transformation
    const basegfx::B2DHomMatrix aTransform(rOutDev.GetViewTransformation());
    basegfx::B2DPolygon aB2DPolygon(rPoly.getB2DPolygon());

    if (!aB2DPolygon.isClosed())
        aB2DPolygon.setClosed(true);

    if (rOutDev.IsFillColor())
    {
        rOutDev.GetGraphics()->DrawPolyPolygon(aTransform, basegfx::B2DPolyPolygon(aB2DPolygon),
                                               0.0, rOutDev);
    }

    if (rOutDev.IsLineColor())
    {
        vcl::rendercontext::StrokeAttributes aStroke;
        aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        PrimitiveRenderer::DrawPolyLine(rOutDev, aB2DPolygon, aStroke, basegfx::B2DHomMatrix(),
                                        0.0);
    }

    return true;
}

bool PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2D,
                                     const StrokeAttributes& rStroke,
                                     const basegfx::B2DHomMatrix& rObjectTransform,
                                     double fTransparency)
{
    if (!rB2D.count() || !rOutDev.CanDrawPolyline())
        return true;

    if (!rOutDev.mpGraphics && !rOutDev.AcquireGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(rOutDev.mpMapper->GetDeviceTransformation()
                                           * rObjectTransform);
    bool bSuccess = false;

    if (rOutDev.GetRasterOp() == RasterOp::OverPaint && rOutDev.IsLineColor())
    {
        const bool bPixelSnapHairline
            = (rOutDev.mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
              && rB2D.count() < 1000;

        if (rOutDev.mpGraphics->DrawPolyLine(
                aTransform, rB2D, fTransparency, rStroke.fWidth, rStroke.pDashArray, rStroke.eJoin,
                rStroke.eCap, rStroke.fMiterMinimumAngle, bPixelSnapHairline, rOutDev))
        {
            bSuccess = true;
        }
    }

    if (!bSuccess)
    {
        basegfx::B2DPolygon aDevicePoly(rB2D);
        aDevicePoly.transform(aTransform);

        auto[aFallbackPolyPoly, aInfo] = lcl_SetupStrokeAndLineInfo(
            aDevicePoly, rStroke.pDashArray, rStroke.fWidth, rStroke.eJoin, rStroke.eCap);

        PrimitiveRenderer::DrawPolyLineGeometry(rOutDev, aFallbackPolyPoly, aInfo);
        bSuccess = true;
    }

    // THIS IS WHAT WAS MISSING: Restored Metafile Recording
    if (bSuccess && rOutDev.maRecorder.IsActive())
    {
        LineInfo aLineInfo;
        if (rStroke.fWidth != 0.0)
            aLineInfo.SetWidth(std::round(rStroke.fWidth));

        aLineInfo.SetLineJoin(rStroke.eJoin);
        aLineInfo.SetLineCap(rStroke.eCap);

        rOutDev.maRecorder.RecordPolyLine(tools::Polygon(rB2D), aLineInfo);
    }

    return bSuccess;
}

void PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    PrimitiveRenderer::DrawPolyLine(rOutDev, rPoly, LineInfo());
}

void PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const tools::Polygon& rPoly,
                                     const LineInfo& rLineInfo)
{
    if (rPoly.GetSize() < 2 || !rOutDev.CanDrawPolyline())
        return;

    rOutDev.FlushGraphicsState();

    if (RasterOp::OverPaint == rOutDev.GetRasterOp() && rOutDev.IsLineColor())
    {
        const bool bPixelSnapHairline
            = (rOutDev.mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
              && rPoly.GetSize() < 1000;

        if (rOutDev.mpGraphics->DrawPolyLine(basegfx::B2DHomMatrix(), rPoly.getB2DPolygon(), 0.0,
                                             rLineInfo.GetWidth(), nullptr, rLineInfo.GetLineJoin(),
                                             rLineInfo.GetLineCap(), basegfx::deg2rad(15.0),
                                             bPixelSnapHairline, rOutDev))
        {
            return;
        }
    }

    if (rLineInfo.GetStyle() == LineStyle::Dash || rLineInfo.GetWidth() > 1)
    {
        basegfx::B2DPolygon aPoly = rOutDev.mpMapper->LogicToDevicePixel(rPoly.getB2DPolygon());
        PrimitiveRenderer::DrawPolyLineGeometry(rOutDev, basegfx::B2DPolyPolygon(aPoly), rLineInfo);
    }
    else
    {
        rOutDev.DrawPolygon(rPoly);
    }
}

void PrimitiveRenderer::DrawPolyLineGeometry(OutputDevice& rOutDev,
                                             const basegfx::B2DPolyPolygon& rPolyPolygon,
                                             const LineInfo& rLineInfo)
{
    if (!rOutDev.mpGraphics && !rOutDev.AcquireGraphics())
        return;

    auto[aHairlines, aFillGeometry] = lcl_ProcessLineGeometry(rPolyPolygon, rLineInfo);

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(rOutDev.maRecorder);

    const bool bTryB2d = (rOutDev.GetRasterOp() == RasterOp::OverPaint && rOutDev.IsLineColor());
    const bool bPixelSnapHairline
        = bool(rOutDev.mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);
    static const bool bFuzzing = comphelper::IsFuzzing();

    rOutDev.FlushGraphicsState();

    lcl_DrawHairlinePolyPolygon(*rOutDev.mpGraphics, rOutDev, aHairlines, bTryB2d,
                                bPixelSnapHairline);
    lcl_DrawAreaGeometry(*rOutDev.mpGraphics, rOutDev, aFillGeometry, bFuzzing, bTryB2d);
}

} // namespace vcl::rendercontext
