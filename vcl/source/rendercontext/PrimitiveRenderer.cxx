/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/log.hxx>
#include <tools/gen.hxx>
#include <iostream>
#include <tools/color.hxx>
#include <tools/poly.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <salgdi.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

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

void PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    if (!rOutDev.CanDrawPolygon())
        vcl::rendercontext::PrimitiveRenderer::DrawPolygonGeometry(rOutDev, rPoly);

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
}

bool PrimitiveRenderer::DrawPolyPolygon(OutputDevice& rOutDev,
                                        const basegfx::B2DPolyPolygon& rB2DPolyPoly, bool bFill,
                                        const StrokeAttributes* pStroke, double fLineTransparency)
{
    if (!rOutDev.CanDrawPolygon())
        return false;

    if (!rOutDev.GetGraphics() && !rOutDev.AcquireGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(rOutDev.GetViewTransformation());
    basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);

    if (!aB2DPolyPolygon.isClosed())
        aB2DPolyPolygon.setClosed(true);

    if (bFill)
        rOutDev.GetGraphics()->DrawPolyPolygon(aTransform, aB2DPolyPolygon, 0.0, rOutDev);

    if (pStroke)
    {
        for (auto const& rPolygon : std::as_const(aB2DPolyPolygon))
        {
            if (!PrimitiveRenderer::DrawPolyLine(rOutDev, rPolygon, *pStroke,
                                                 basegfx::B2DHomMatrix(), fLineTransparency))
                return false;
        }
    }

    return true;
}

bool PrimitiveRenderer::DrawPolyPolygon(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly,
                                        bool bFill, const StrokeAttributes* pStroke,
                                        double fLineTransparency)
{
    if (!rOutDev.CanDrawPolygon())
        return false;
    return DrawPolyPolygon(rOutDev, rPolyPoly.getB2DPolyPolygon(), bFill, pStroke,
                           fLineTransparency);
}

bool PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                                    bool bFill, const StrokeAttributes* pStroke,
                                    double fLineTransparency)
{
    basegfx::B2DPolyPolygon aPP(rB2DPolygon);
    return DrawPolyPolygon(rOutDev, aPP, bFill, pStroke, fLineTransparency);
}

bool PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly, bool bFill,
                                    const StrokeAttributes* pStroke, double fLineTransparency)
{
    basegfx::B2DPolygon aB2D(rPoly.getB2DPolygon());
    return DrawPolygon(rOutDev, aB2D, bFill, pStroke, fLineTransparency);
}

// NEW WRAPPER API: Encapsulated stroke attributes
bool PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                                     const StrokeAttributes& rStroke,
                                     const basegfx::B2DHomMatrix& rObjectTransform)
{
    // Phase 1: Route to legacy 5-argument implementation using the encapsulated transparency
    return DrawPolyLine(rOutDev, rB2DPolygon, rStroke, rObjectTransform, rStroke.fTransparency);
}

bool PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2D,
                                     const StrokeAttributes& rStroke,
                                     const basegfx::B2DHomMatrix& rObjectTransform,
                                     double fTransparency)
{
    if (!rB2D.count() || !rOutDev.CanDrawPolyline())
        return true;

    if (!rOutDev.GetGraphics() && !rOutDev.AcquireGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(rOutDev.GetViewTransformation() * rObjectTransform);
    bool bSuccess = false;

    if (rOutDev.GetRasterOp() == RasterOp::OverPaint && rOutDev.IsLineColor())
    {
        const bool bPixelSnapHairline
            = (rOutDev.GetAntialiasing() & AntialiasingFlags::PixelSnapHairline)
              && rB2D.count() < 1000;

        SalGraphics* pGraphics = rOutDev.GetGraphics();
        if (pGraphics
            && pGraphics->DrawPolyLine(aTransform, rB2D, fTransparency, rStroke.fWidth,
                                       rStroke.pDashArray, rStroke.eJoin, rStroke.eCap,
                                       rStroke.fMiterMinimumAngle, bPixelSnapHairline, rOutDev))
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

namespace
{
constexpr sal_uInt16 OUTDEV_POLYPOLY_STACKBUF = 32;

struct PolyPolyBuffer
{
    // The fast stack buffers
    sal_uInt32 aStackAry1[OUTDEV_POLYPOLY_STACKBUF];
    const Point* aStackAry2[OUTDEV_POLYPOLY_STACKBUF];
    const PolyFlags* aStackAry3[OUTDEV_POLYPOLY_STACKBUF];

    // The active pointers (will point to stack OR heap)
    sal_uInt32* pPointAry;
    const Point** pPointAryAry;
    const PolyFlags** pFlagAryAry;

    bool bUseHeap;

    // Extracted state variables
    sal_uInt16 mnValidCount = 0;
    sal_uInt16 mnLastIndex = 0;
    bool mbHaveBezier = false;

    explicit PolyPolyBuffer(const tools::PolyPolygon& rPolyPoly)
        : bUseHeap(rPolyPoly.Count() > OUTDEV_POLYPOLY_STACKBUF)
    {
        sal_uInt16 nPoly = rPolyPoly.Count();

        if (bUseHeap)
        {
            pPointAry = new sal_uInt32[nPoly];
            pPointAryAry = new const Point*[nPoly];
            pFlagAryAry = new const PolyFlags*[nPoly];
        }
        else
        {
            pPointAry = aStackAry1;
            pPointAryAry = aStackAry2;
            pFlagAryAry = aStackAry3;
        }

        // Flattens valid sub-polygons into parallel C-arrays for the graphics
        // backend and detects Bézier curves.

        for (sal_uInt16 i = 0; i < nPoly; ++i)
        {
            const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();

            if (nSize)
            {
                pPointAry[mnValidCount] = nSize;
                pPointAryAry[mnValidCount] = rPoly.GetConstPointAry();
                pFlagAryAry[mnValidCount] = rPoly.GetConstFlagAry();
                mnLastIndex = i;

                if (pFlagAryAry[mnValidCount])
                    mbHaveBezier = true;

                ++mnValidCount;
            }
        }
    }

    ~PolyPolyBuffer()
    {
        if (bUseHeap)
        {
            delete[] pPointAry;
            delete[] pPointAryAry;
            delete[] pFlagAryAry;
        }
    }
};

} // end anonymous namespace

void PrimitiveRenderer::DrawPolyPolygonGeometry(OutputDevice& rOutDev,
                                                const tools::PolyPolygon& rPolyPoly)
{
    if (!rPolyPoly.Count())
        return;

    SalGraphics* pGraphics = rOutDev.GetGraphics();
    if (!pGraphics && !rOutDev.AcquireGraphics())
        return;
    pGraphics = rOutDev.GetGraphics();

    PolyPolyBuffer aBuffer(rPolyPoly);
    if (aBuffer.mnValidCount == 0)
        return;

    // Single polygon optimization
    if (aBuffer.mnValidCount == 1)
    {
        const tools::Polygon& rPoly = rPolyPoly.GetObject(aBuffer.mnLastIndex);
        DrawPolygonGeometry(rOutDev, rPoly);
        return;
    }

    // Hardware dispatch with Bézier support check
    if (aBuffer.mbHaveBezier)
    {
        if (!pGraphics->DrawPolyPolygonBezier(aBuffer.mnValidCount, aBuffer.pPointAry,
                                              aBuffer.pPointAryAry, aBuffer.pFlagAryAry, rOutDev))
        {
            tools::PolyPolygon aSub = tools::PolyPolygon::SubdivideBezier(rPolyPoly);
            DrawPolyPolygonGeometry(rOutDev, aSub);
        }
        return;
    }

    pGraphics->DrawPolyPolygon(aBuffer.mnValidCount, aBuffer.pPointAry, aBuffer.pPointAryAry,
                               rOutDev);
}

void PrimitiveRenderer::DrawPolygonGeometry(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    sal_uInt16 nPoints = rPoly.GetSize();
    if (nPoints < 2)
        return;

    SalGraphics* pGraphics = rOutDev.GetGraphics();
    if (!pGraphics && !rOutDev.AcquireGraphics())
        return;
    pGraphics = rOutDev.GetGraphics();

    const Point* pPtAry = rPoly.GetConstPointAry();

    if (rPoly.HasFlags())
    {
        const PolyFlags* pFlgAry = rPoly.GetConstFlagAry();
        if (!pGraphics->DrawPolygonBezier(nPoints, pPtAry, pFlgAry, rOutDev))
        {
            tools::Polygon aSub = tools::Polygon::SubdivideBezier(rPoly);
            pGraphics->DrawPolygon(aSub.GetSize(), aSub.GetConstPointAry(), rOutDev);
        }
    }
    else
    {
        pGraphics->DrawPolygon(nPoints, pPtAry, rOutDev);
    }
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
