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
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/WaveLine.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/text/TextDecorator.hxx>

#include <vcl/rendercontext/WaveLineGeometry.hxx>

#include <font/EmphasisMark.hxx>
#include <font/FontController.hxx>
#include <salgdi.hxx>
#include <text/TextLayoutEngine.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

#include <com/sun/star/drawing/LineCap.hpp>

namespace vcl::rendercontext
{
void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const Point& rLogicalPt)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    rGraphics.drawPixel(aDevicePt.X(), aDevicePt.Y());
}

void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const Point& rLogicalPt, const Color& rColor)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    rGraphics.drawPixel(aDevicePt.X(), aDevicePt.Y(), rColor);
}

Color PrimitiveRenderer::GetPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const Point& rLogicalPt)
{
    Point aDevicePt = rMapper.LogicToDevicePixel(rLogicalPt);
    return Color(rGraphics.getPixel(aDevicePt.X(), aDevicePt.Y()));
}

void PrimitiveRenderer::DrawDeviceLine(SalGraphics& rGraphics, const Point& rDeviceStart,
                                       const Point& rDeviceEnd, bool bTryAA,
                                       bool bPixelSnapHairline)
{
    bool bDrawn = false;

    if (bTryAA)
    {
        basegfx::B2DPolygon aB2DPolyLine;
        aB2DPolyLine.append(basegfx::B2DPoint(rDeviceStart.X(), rDeviceStart.Y()));
        aB2DPolyLine.append(basegfx::B2DPoint(rDeviceEnd.X(), rDeviceEnd.Y()));

        // Call the pure virtual backend directly, bypassing the SalGraphics wrapper
        bDrawn = rGraphics.drawPolyLine(basegfx::B2DHomMatrix(), aB2DPolyLine, 0.0, 0.0, nullptr,
                                        basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                                        basegfx::deg2rad(15.0), bPixelSnapHairline);
    }

    if (!bDrawn)
    {
        // Call the pure virtual backend directly!
        rGraphics.drawLine(rDeviceStart.X(), rDeviceStart.Y(), rDeviceEnd.X(), rDeviceEnd.Y());
    }
}

void PrimitiveRenderer::DrawDeviceRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect)
{
    if (!rDeviceRect.IsEmpty())
    {
        // Naked pure virtual backend call!
        rGraphics.drawRect(rDeviceRect.Left(), rDeviceRect.Top(), rDeviceRect.GetWidth(),
                           rDeviceRect.GetHeight());
    }
}

void PrimitiveRenderer::DrawRoundedRect(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                        const tools::Rectangle& rLogicalRect, sal_uLong nHorzRound,
                                        sal_uLong nVertRound, bool bFillColor)
{
    const tools::Rectangle aDeviceRect(rMapper.LogicToDevicePixel(rLogicalRect));

    nHorzRound = rMapper.LogicWidthToDevicePixel(nHorzRound);
    nVertRound = rMapper.LogicHeightToDevicePixel(nVertRound);

    if (!nHorzRound && !nVertRound)
    {
        rGraphics.drawRect(aDeviceRect.Left(), aDeviceRect.Top(), aDeviceRect.GetWidth(),
                           aDeviceRect.GetHeight());
        return;
    }

    tools::Polygon aRoundRectPoly(aDeviceRect, nHorzRound, nVertRound);

    if (aRoundRectPoly.GetSize() < 2)
        return;

    Point* pPtAry = aRoundRectPoly.GetPointAry();

    if (!bFillColor)
        rGraphics.drawPolyLine(aRoundRectPoly.GetSize(), pPtAry);
    else
        rGraphics.drawPolygon(aRoundRectPoly.GetSize(), pPtAry);
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

bool PrimitiveRenderer::DrawPolyPolygon(OutputDevice& rOutDev,
                                        const basegfx::B2DPolyPolygon& rB2DPolyPoly, bool bFill,
                                        const StrokeAttributes* pStroke)
{
    if (!rB2DPolyPoly.count() || !rOutDev.CanDrawPolygon())
        return true;

    if (!rOutDev.GetGraphics() && !rOutDev.AcquireGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(rOutDev.GetViewTransformation());
    basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);

    if (!aB2DPolyPolygon.isClosed())
        aB2DPolyPolygon.setClosed(true);

    if (bFill)
        rOutDev.GetGraphics()->DrawPolyPolygon(aTransform, aB2DPolyPolygon, 0.0, rOutDev);

    bool bSuccess = true;

    if (pStroke)
    {
        for (auto const& rPolygon : std::as_const(aB2DPolyPolygon))
        {
            if (!PrimitiveRenderer::DrawPolyLine(rOutDev, rPolygon, *pStroke))
            {
                bSuccess = false;
                break;
            }
        }
    }

    if (!bSuccess)
    {
        const tools::PolyPolygon aToolsPolyPolygon(rB2DPolyPoly);
        const tools::PolyPolygon aPixelPolyPolygon
            = rOutDev.mpMapper->LogicToDevicePixel(aToolsPolyPolygon);
        PrimitiveRenderer::DrawPolyPolygonGeometry(rOutDev, aPixelPolyPolygon);
    }

    return true;
}

void PrimitiveRenderer::DrawPolyPolygonFallback(OutputDevice& rOutDev,
                                                const tools::PolyPolygon& rPolyPoly)
{
    const sal_uInt16 nPoly = rPolyPoly.Count();
    if (nPoly == 1)
    {
        const tools::Polygon& rPoly = rPolyPoly.GetObject(0);
        if (rPoly.GetSize() >= 2)
        {
            // We draw the single polygon. Since recording is handled in the
            // high-level OutputDevice::DrawPolygon, we don't need ScopedSuspend here
            // if we are already inside a renderer call.
            rOutDev.DrawPolygon(rPoly);
        }
    }
    else if (nPoly > 1)
    {
        // Direct pixel-based geometry rendering
        PrimitiveRenderer::DrawPolyPolygonGeometry(rOutDev,
                                                   rOutDev.mpMapper->LogicToDevicePixel(rPolyPoly));
    }
}

bool PrimitiveRenderer::DrawPolyPolygon(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly,
                                        bool bFill, const StrokeAttributes* pStroke)
{
    if (!rOutDev.CanDrawPolygon())
        return false;
    return DrawPolyPolygon(rOutDev, rPolyPoly.getB2DPolyPolygon(), bFill, pStroke);
}

bool PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                                    bool bFill, const StrokeAttributes* pStroke)
{
    basegfx::B2DPolyPolygon aPP(rB2DPolygon);
    return DrawPolyPolygon(rOutDev, aPP, bFill, pStroke);
}

bool PrimitiveRenderer::DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly, bool bFill,
                                    const StrokeAttributes* pStroke)
{
    basegfx::B2DPolygon aB2D(rPoly.getB2DPolygon());
    return DrawPolygon(rOutDev, aB2D, bFill, pStroke);
}

bool PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                                     const StrokeAttributes& rStroke,
                                     const basegfx::B2DHomMatrix& rObjectTransform)
{
    if (rB2DPolygon.count() == 0 || !rOutDev.CanDrawPolyline())
        return true;

    // 1. Handle Metafile Recording
    if (rOutDev.maRecorder.IsActive())
    {
        basegfx::B2DPolygon aRecordPoly(rB2DPolygon);
        if (!rObjectTransform.isIdentity())
            aRecordPoly.transform(rObjectTransform);

        rOutDev.maRecorder.RecordB2DPolyLine(aRecordPoly, rStroke);
    }

    if (!rOutDev.IsDeviceOutputNecessary())
        return true;

    // 2. Prepare Graphics
    if (!rOutDev.GetGraphics() && !rOutDev.AcquireGraphics())
        return false;

    rOutDev.FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(rOutDev.GetViewTransformation() * rObjectTransform);
    bool bSuccess = false;

    // 3. Attempt direct SalGraphics rendering
    if (rOutDev.GetRasterOp() == RasterOp::OverPaint && rOutDev.IsLineColor())
    {
        const bool bPixelSnapHairline
            = (rOutDev.GetAntialiasing() & AntialiasingFlags::PixelSnapHairline)
              && rB2DPolygon.count() < 1000;

        SalGraphics* pGraphics = rOutDev.GetGraphics();
        if (pGraphics
            && pGraphics->DrawPolyLine(aTransform, rB2DPolygon, rStroke.fTransparency,
                                       rStroke.fWidth, rStroke.pDashArray, rStroke.eJoin,
                                       rStroke.eCap, rStroke.fMiterMinimumAngle, bPixelSnapHairline,
                                       rOutDev))
        {
            bSuccess = true;
        }
    }

    // 4. Fallback to geometry decomposition if direct rendering fails
    if (!bSuccess)
    {
        basegfx::B2DPolygon aDevicePoly(rB2DPolygon);
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

void PrimitiveRenderer::DrawDevicePolyPolygonGeometry(SalGraphics& rGraphics,
                                                      const tools::PolyPolygon& rDevicePolyPoly)
{
    if (!rDevicePolyPoly.Count())
        return;

    PolyPolyBuffer aBuffer(rDevicePolyPoly);
    if (aBuffer.mnValidCount == 0)
        return;

    if (aBuffer.mnValidCount == 1)
    {
        DrawDevicePolygonGeometry(rGraphics, rDevicePolyPoly.GetObject(aBuffer.mnLastIndex));
        return;
    }

    if (aBuffer.mbHaveBezier)
    {
        if (!rGraphics.drawPolyPolygonBezier(aBuffer.mnValidCount, aBuffer.pPointAry,
                                             aBuffer.pPointAryAry, aBuffer.pFlagAryAry))
        {
            tools::PolyPolygon aSub = tools::PolyPolygon::SubdivideBezier(rDevicePolyPoly);
            DrawDevicePolyPolygonGeometry(rGraphics, aSub);
        }
        return;
    }

    // Naked pure virtual backend call!
    rGraphics.drawPolyPolygon(aBuffer.mnValidCount, aBuffer.pPointAry, aBuffer.pPointAryAry);
}

void PrimitiveRenderer::DrawPolyPolygonGeometry(OutputDevice& rOutDev,
                                                const tools::PolyPolygon& rPolyPoly)
{
    SalGraphics* pGraphics = rOutDev.GetGraphics();
    if (!pGraphics && !rOutDev.AcquireGraphics())
        return;
    pGraphics = rOutDev.GetGraphics();

    tools::PolyPolygon aDevicePolyPoly = rPolyPoly;
    bool bRTL = rOutDev.IsRTLEnabled() || (pGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    bool bAntiparallel = rOutDev.ImplIsAntiparallel();
    tools::Long nFrameWidth
        = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel() : pGraphics->GetGraphicsWidth();

    rOutDev.mpMapper->MirrorDevicePixelPolyPolygon(aDevicePolyPoly, nFrameWidth, bRTL,
                                                   bAntiparallel);
    DrawDevicePolyPolygonGeometry(*pGraphics, aDevicePolyPoly);
}

void PrimitiveRenderer::DrawDevicePolygonGeometry(SalGraphics& rGraphics,
                                                  const tools::Polygon& rDevicePoly)
{
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    if (nPoints < 2)
        return;

    const Point* pPtAry = rDevicePoly.GetConstPointAry();

    if (rDevicePoly.HasFlags())
    {
        const PolyFlags* pFlgAry = rDevicePoly.GetConstFlagAry();
        if (!rGraphics.drawPolygonBezier(nPoints, pPtAry, pFlgAry))
        {
            tools::Polygon aSub = tools::Polygon::SubdivideBezier(rDevicePoly);
            rGraphics.drawPolygon(aSub.GetSize(), aSub.GetConstPointAry());
        }
    }
    else
    {
        // Naked pure virtual backend call!
        rGraphics.drawPolygon(nPoints, pPtAry);
    }
}

void PrimitiveRenderer::DrawPolygonGeometry(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    SalGraphics* pGraphics = rOutDev.GetGraphics();
    if (!pGraphics && !rOutDev.AcquireGraphics())
        return;
    pGraphics = rOutDev.GetGraphics();

    tools::Polygon aDevicePoly = rPoly;
    bool bRTL = rOutDev.IsRTLEnabled() || (pGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    bool bAntiparallel = rOutDev.ImplIsAntiparallel();
    tools::Long nFrameWidth
        = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel() : pGraphics->GetGraphicsWidth();

    rOutDev.mpMapper->MirrorDevicePixelPolygon(aDevicePoly, nFrameWidth, bRTL, bAntiparallel);
    DrawDevicePolygonGeometry(*pGraphics, aDevicePoly);
}

namespace
{
struct ClippedPolygonData
{
    std::unique_ptr<tools::PolyPolygon> pAllocated;
    tools::PolyPolygon* pActive = nullptr;
};
}

static ClippedPolygonData lcl_GetClippedPolyPolygon(const tools::PolyPolygon& rPolyPoly,
                                                    const tools::PolyPolygon* pClipPolyPoly)
{
    ClippedPolygonData aData;

    if (pClipPolyPoly)
    {
        aData.pAllocated = std::make_unique<tools::PolyPolygon>();
        aData.pActive = aData.pAllocated.get();
        rPolyPoly.GetIntersection(*pClipPolyPoly, *aData.pActive);
    }
    else
    {
        aData.pActive = const_cast<tools::PolyPolygon*>(&rPolyPoly);
    }

    return aData;
}

void PrimitiveRenderer::DrawPolyPolygon(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly,
                                        const tools::PolyPolygon* pClipPolyPoly)
{
    auto aClippedData = lcl_GetClippedPolyPolygon(rPolyPoly, pClipPolyPoly);
    tools::PolyPolygon* pPolyPoly = aClippedData.pActive;

    if (pPolyPoly->Count() == 1)
        PrimitiveRenderer::DrawSinglePolygon(rOutDev, pPolyPoly->GetObject(0));
    else if (pPolyPoly->Count())
        PrimitiveRenderer::DrawMultiplePolygons(rOutDev, *pPolyPoly);
}

void PrimitiveRenderer::DrawSinglePolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly)
{
    const sal_uInt16 nSize = rPoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = rPoly.GetConstPointAry();
        rOutDev.mpGraphics->DrawPolygon(nSize, pPtAry, rOutDev);
    }
}

namespace
{
struct PolygonRenderBuffer
{
    std::unique_ptr<sal_uInt32[]> pPointAry;
    std::unique_ptr<const Point* []> pPointAryAry;
    sal_uInt16 nValidCount = 0;

    explicit PolygonRenderBuffer(const tools::PolyPolygon& rPolyPoly)
    {
        sal_uInt16 nTotalCount = rPolyPoly.Count();

        pPointAry.reset(new sal_uInt32[nTotalCount]);
        pPointAryAry.reset(new const Point*[nTotalCount]);

        for (sal_uInt16 i = 0; i < nTotalCount; ++i)
        {
            const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();

            if (nSize >= 2)
            {
                pPointAry[nValidCount] = nSize;
                pPointAryAry[nValidCount] = rPoly.GetConstPointAry();
                nValidCount++;
            }
        }
    }
};
}

void PrimitiveRenderer::DrawMultiplePolygons(OutputDevice& rOutDev,
                                             const tools::PolyPolygon& rPolyPoly)
{
    if (!rPolyPoly.Count())
        return;

    PolygonRenderBuffer aBuffer(rPolyPoly);

    if (aBuffer.nValidCount == 1)
    {
        rOutDev.mpGraphics->DrawPolygon(aBuffer.pPointAry[0], aBuffer.pPointAryAry[0], rOutDev);
    }
    else if (aBuffer.nValidCount > 1)
    {
        rOutDev.mpGraphics->DrawPolyPolygon(aBuffer.nValidCount, aBuffer.pPointAry.get(),
                                            aBuffer.pPointAryAry.get(), rOutDev);
    }
}

void PrimitiveRenderer::DrawClippedPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly,
                                           const tools::PolyPolygon& rClipPolyPoly)
{
    // Handle clipping via intersection then dispatch
    tools::PolyPolygon aClipped;
    tools::PolyPolygon(rPoly).GetIntersection(rClipPolyPoly, aClipped);

    PrimitiveRenderer::DrawPolyPolygonGeometry(rOutDev, aClipped);
}

void PrimitiveRenderer::DrawEllipse(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                                    bool bFill)
{
    tools::Polygon aRectPoly(rPixelRect.Center(), rPixelRect.GetWidth() >> 1,
                             rPixelRect.GetHeight() >> 1);
    const sal_uInt16 nSize = aRectPoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = aRectPoly.GetConstPointAry();
        if (!bFill)
            rOutDev.mpGraphics->DrawPolyLine(nSize, pPtAry, rOutDev);
        else
            rOutDev.mpGraphics->DrawPolygon(nSize, pPtAry, rOutDev);
    }
}

void PrimitiveRenderer::DrawArc(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                                const Point& rPixelStart, const Point& rPixelEnd)
{
    tools::Polygon aArcPoly(rPixelRect, rPixelStart, rPixelEnd, PolyStyle::Arc);
    const sal_uInt16 nSize = aArcPoly.GetSize();

    if (nSize >= 2)
    {
        // Arcs are never filled
        rOutDev.mpGraphics->DrawPolyLine(nSize, aArcPoly.GetConstPointAry(), rOutDev);
    }
}

void PrimitiveRenderer::DrawPie(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                                const Point& rPixelStart, const Point& rPixelEnd, bool bFill)
{
    tools::Polygon aPiePoly(rPixelRect, rPixelStart, rPixelEnd, PolyStyle::Pie);
    const sal_uInt16 nSize = aPiePoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = aPiePoly.GetConstPointAry();
        if (!bFill)
            rOutDev.mpGraphics->DrawPolyLine(nSize, pPtAry, rOutDev);
        else
            rOutDev.mpGraphics->DrawPolygon(nSize, pPtAry, rOutDev);
    }
}

void PrimitiveRenderer::DrawChord(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                                  const Point& rPixelStart, const Point& rPixelEnd, bool bFill)
{
    tools::Polygon aChordPoly(rPixelRect, rPixelStart, rPixelEnd, PolyStyle::Chord);
    const sal_uInt16 nSize = aChordPoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = aChordPoly.GetConstPointAry();
        if (!bFill)
            rOutDev.mpGraphics->DrawPolyLine(nSize, pPtAry, rOutDev);
        else
            rOutDev.mpGraphics->DrawPolygon(nSize, pPtAry, rOutDev);
    }
}

void PrimitiveRenderer::Invert(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                               const OutputDevice* pOutDev, const tools::Rectangle& rLogicalRect,
                               InvertFlags nFlags)
{
    tools::Rectangle aDeviceRect(rMapper.LogicToDevicePixel(rLogicalRect));

    if (aDeviceRect.IsEmpty())
        return;

    aDeviceRect.Normalize();

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    rGraphics.Invert(aDeviceRect.Left(), aDeviceRect.Top(), aDeviceRect.GetWidth(),
                     aDeviceRect.GetHeight(), nSalFlags, *pOutDev);
}

void PrimitiveRenderer::Invert(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                               const OutputDevice* pOutDev, const tools::Polygon& rLogicalPoly,
                               InvertFlags nFlags)
{
    sal_uInt16 nPoints = rLogicalPoly.GetSize();
    if (nPoints < 2)
        return;

    tools::Polygon aDevicePoly(rMapper.LogicToDevicePixel(rLogicalPoly));

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    const Point* pPtAry = aDevicePoly.GetConstPointAry();
    rGraphics.Invert(nPoints, pPtAry, nSalFlags, *pOutDev);
}

namespace
{
struct GridGeometry
{
    tools::Long nDistX;
    tools::Long nDistY;
    tools::Long nLogStartX;
    tools::Long nLogStartY;
    tools::Long nLogRight;
    tools::Long nLogBottom;

    // Device Pixel boundaries for the lines
    tools::Long nPixStartX;
    tools::Long nPixStartY;
    tools::Long nPixRight;
    tools::Long nPixBottom;

    GridGeometry(const CoordinateMapper& rMapper, const tools::Rectangle& rRect,
                 const tools::Rectangle& aDstRect, const Size& rDist)
    {
        nDistX = std::max(rDist.Width(), tools::Long(1));
        nDistY = std::max(rDist.Height(), tools::Long(1));

        // Logical alignment logic
        nLogStartX = (rRect.Left() >= aDstRect.Left())
                         ? rRect.Left()
                         : (rRect.Left() + ((aDstRect.Left() - rRect.Left()) / nDistX) * nDistX);
        nLogStartY = (rRect.Top() >= aDstRect.Top())
                         ? rRect.Top()
                         : (rRect.Top() + ((aDstRect.Top() - rRect.Top()) / nDistY) * nDistY);

        nLogRight = aDstRect.Right();
        nLogBottom = aDstRect.Bottom();

        // Pre-cache the device pixel boundaries
        nPixStartX = rMapper.LogicXToDevicePixel(nLogStartX);
        nPixStartY = rMapper.LogicYToDevicePixel(nLogStartY);
        nPixRight = rMapper.LogicXToDevicePixel(nLogRight);
        nPixBottom = rMapper.LogicYToDevicePixel(nLogBottom);
    }

    std::vector<sal_Int32> CalculateOffsets(const CoordinateMapper& rMapper, bool bIsVertical) const
    {
        std::vector<sal_Int32> aBuf;
        const tools::Long nStart = bIsVertical ? nLogStartY : nLogStartX;
        const tools::Long nEnd = bIsVertical ? nLogBottom : nLogRight;
        const tools::Long nDist = bIsVertical ? nDistY : nDistX;

        // reserve capacity: (distance / step) + 2 for safety
        aBuf.reserve(((nEnd - nStart) / nDist) + 2);

        tools::Long nPos = nStart;
        auto fnMap = [&rMapper, bIsVertical](tools::Long v) {
            return bIsVertical ? rMapper.LogicYToDevicePixel(v) : rMapper.LogicXToDevicePixel(v);
        };

        aBuf.push_back(fnMap(nPos));
        while ((nPos += nDist) <= nEnd)
        {
            aBuf.push_back(fnMap(nPos));
        }
        return aBuf;
    }
};
}

void PrimitiveRenderer::DrawGrid(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                 const OutputDevice* pOutDev, const tools::Rectangle& rRect,
                                 const tools::Rectangle& rDstRect, const Size& rDist,
                                 DrawGridFlags nFlags)
{
    const GridGeometry aGrid(rMapper, rRect, rDstRect, rDist);

    std::vector<sal_Int32> aVertBuf;
    if (nFlags & (DrawGridFlags::Dots | DrawGridFlags::HorzLines))
        aVertBuf = aGrid.CalculateOffsets(rMapper, true);

    std::vector<sal_Int32> aHorzBuf;
    if (nFlags & (DrawGridFlags::Dots | DrawGridFlags::VertLines))
        aHorzBuf = aGrid.CalculateOffsets(rMapper, false);

    if (nFlags & DrawGridFlags::Dots)
    {
        for (const auto& rY : aVertBuf)
        {
            for (const auto& rX : aHorzBuf)
            {
                rGraphics.DrawPixel(rX, rY, *pOutDev);
            }
        }
    }

    if (nFlags & DrawGridFlags::HorzLines)
    {
        for (const auto& rY : aVertBuf)
        {
            rGraphics.DrawLine(aGrid.nPixStartX, rY, aGrid.nPixRight, rY, *pOutDev);
        }
    }

    if (nFlags & DrawGridFlags::VertLines)
    {
        for (const auto& rX : aHorzBuf)
        {
            rGraphics.DrawLine(rX, aGrid.nPixStartY, rX, aGrid.nPixBottom, *pOutDev);
        }
    }
}

namespace
{
struct CrossGridGeometry
{
    std::vector<tools::Long> aHorzBuffer;
    std::vector<tools::Long> aVertBuffer;

    // Drawing area boundaries in device pixels
    tools::Long nPixTop;
    tools::Long nPixBottom;
    tools::Long nPixLeft;
    tools::Long nPixRight;

    CrossGridGeometry(const CoordinateMapper& rMapper, const tools::Rectangle& rGridArea,
                      const Size& rGridDistance, const tools::Rectangle& rDrawingArea)
    {
        const tools::Long nDistanceX = std::max(rGridDistance.Width(), tools::Long(1));
        const tools::Long nDistanceY = std::max(rGridDistance.Height(), tools::Long(1));

        // Generate logical horizontal positions
        aHorzBuffer.reserve(rGridArea.GetWidth() / nDistanceX + 1);
        tools::Long nX = rGridArea.Left();
        while (nX <= rGridArea.Right())
        {
            aHorzBuffer.push_back(nX);
            nX += nDistanceX;
        }

        // Generate logical vertical positions
        aVertBuffer.reserve(rGridArea.GetHeight() / nDistanceY + 1);
        tools::Long nY = rGridArea.Top();
        while (nY <= rGridArea.Bottom())
        {
            aVertBuffer.push_back(nY);
            nY += nDistanceY;
        }

        // Map drawing area to device pixels
        nPixTop = rMapper.LogicYToDevicePixel(rDrawingArea.Top());
        nPixBottom = rMapper.LogicYToDevicePixel(rDrawingArea.Bottom());
        nPixLeft = rMapper.LogicXToDevicePixel(rDrawingArea.Left());
        nPixRight = rMapper.LogicXToDevicePixel(rDrawingArea.Right());
    }
};
}

void PrimitiveRenderer::DrawGridOfCrosses(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                          const OutputDevice* pOutDev,
                                          const tools::Rectangle& rGridArea,
                                          const Size& rGridDistance,
                                          const tools::Rectangle& rDrawingArea)
{
    const CrossGridGeometry aGrid(rMapper, rGridArea, rGridDistance, rDrawingArea);

    for (const auto& rLogY : aGrid.aVertBuffer)
    {
        const tools::Long nY = rMapper.LogicYToDevicePixel(rLogY);
        if (nY < aGrid.nPixTop || nY > aGrid.nPixBottom)
            continue;

        for (const auto& rLogX : aGrid.aHorzBuffer)
        {
            const tools::Long nX = rMapper.LogicXToDevicePixel(rLogX);
            if (nX < aGrid.nPixLeft || nX > aGrid.nPixRight)
                continue;

            // Draw a 3x3 cross centered at (nX, nY)
            rGraphics.DrawPixel(nX, nY, *pOutDev);
            rGraphics.DrawPixel(nX - 1, nY, *pOutDev);
            rGraphics.DrawPixel(nX + 1, nY, *pOutDev);
            rGraphics.DrawPixel(nX, nY - 1, *pOutDev);
            rGraphics.DrawPixel(nX, nY + 1, *pOutDev);
        }
    }
}

void PrimitiveRenderer::DrawTextRect(SalGraphics& rGraphics, OutputDevice* pOutDev,
                                     const Point& rBasePt, const tools::Rectangle& rRect,
                                     Degree10 nOrientation)
{
    auto aGeo = vcl::text::TextGeometry::GetRotatedGeometry(rBasePt, rRect, nOrientation);

    if (aGeo.mbIsPolygon)
        PrimitiveRenderer::DrawPolygonGeometry(*pOutDev, aGeo.maPoly);
    else
    {
        tools::Rectangle aDeviceRect(aGeo.maRect);
        bool bRTL = pOutDev->IsRTLEnabled()
                    || (pOutDev->GetGraphics()
                        && (pOutDev->GetGraphics()->GetLayout() & SalLayoutFlags::BiDiRtl));
        bool bAntiparallel = pOutDev->ImplIsAntiparallel();
        tools::Long nFrameWidth = pOutDev->IsVirtual() ? pOutDev->GetOutputWidthPixel()
                                                       : pOutDev->GetGraphics()->GetGraphicsWidth();
        pOutDev->mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, bAntiparallel);
        PrimitiveRenderer::DrawDeviceRect(rGraphics, aDeviceRect);
    }
}

void PrimitiveRenderer::DrawWaveLineBezier(OutputDevice& rOutDev, SalGraphics& rGraphics,
                                           tools::Long nStartX, tools::Long nStartY,
                                           tools::Long nEndX, tools::Long nEndY,
                                           tools::Long nWaveHeight, double fOrientation,
                                           tools::Long nLineWidth)
{
    const basegfx::B2DRectangle aWaveLineRectangle(nStartX, nStartY, nEndX, nEndY + nWaveHeight);
    const basegfx::B2DPolygon aWaveLinePolygon = basegfx::createWaveLinePolygon(aWaveLineRectangle);
    const basegfx::B2DHomMatrix aRotationMatrix = basegfx::utils::createRotateAroundPoint(
        nStartX, nStartY, basegfx::deg2rad(-fOrientation));
    const bool bPixelSnapHairline(rOutDev.mpGraphicsState->mnAntialiasing
                                  & AntialiasingFlags::PixelSnapHairline);

    rGraphics.SetLineColor(rOutDev.GetLineColor());
    rGraphics.DrawPolyLine(aRotationMatrix, aWaveLinePolygon, 0.0, nLineWidth,
                           nullptr, // MM01
                           basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                           basegfx::deg2rad(15.0), bPixelSnapHairline, rOutDev);
}

void PrimitiveRenderer::DrawWaveLineHairline(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                             const Color& rColor)
{
    rOutDev.mpGraphics->SetLineColor(rColor);
    rOutDev.mbLineColorDirty = true;

    const Point aLineStart = rGeo.GetLineStart();
    const Point aLineEnd = rGeo.GetLineEnd();

    rOutDev.mpGraphics->DrawLine(aLineStart.X(), aLineStart.Y(), aLineEnd.X(), aLineEnd.Y(),
                                 rOutDev);
}

void PrimitiveRenderer::DrawWaveLineRasterized(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                               const Color& rColor)
{
    rOutDev.SetWaveLineColors(rColor, rGeo.maWavePixelSize.Height());

    for (Point aDrawPt : rGeo.GetRegion())
    {
        if (rGeo.mnOrientation)
            rGeo.maBase.RotateAround(aDrawPt, rGeo.mnOrientation);

        if (rGeo.mbDrawAsRect)
        {
            rOutDev.mpGraphics->DrawRect(aDrawPt.X(), aDrawPt.Y(), rGeo.maWavePixelSize.Width(),
                                         rGeo.maWavePixelSize.Height(), rOutDev);
        }
        else
        {
            rOutDev.mpGraphics->DrawPixel(aDrawPt.X(), aDrawPt.Y(), rOutDev);
        }
    }
}

void PrimitiveRenderer::DrawWaveLine(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                     const Color& rColor)
{
    if (rGeo.maWavePixelSize.Height() == 1 && rGeo.maSize.Height() == 1)
    {
        PrimitiveRenderer::DrawWaveLineHairline(rOutDev, rGeo, rColor);
        return;
    }

    PrimitiveRenderer::DrawWaveLineRasterized(rOutDev, rGeo, rColor);
}

void PrimitiveRenderer::DrawWaveTextLine(OutputDevice& rOutDev,
                                         const vcl::rendercontext::TextLineGeometry& rGeo,
                                         tools::Long nY, Color aColor, bool bIsAbove)
{
    vcl::text::WaveLineGeometry aWaveStyle = vcl::text::TextDecorator::CalculateWaveLineGeometry(
        *rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline, bIsAbove, nY, rOutDev.GetDPIX(),
        rOutDev.GetDPIY());

    const Size aWavePixelSize = rOutDev.GetWaveLineSize(aWaveStyle.nLineWidth);
    const bool bDrawAsRect = rOutDev.shouldDrawWavePixelAsRect(aWaveStyle.nLineWidth);

    Degree10 nOrientation = rOutDev.mpFontInstance->mnOrientation;

    for (const auto& rSeg : aWaveStyle.aSegments)
    {
        WaveLineGeometry aWaveGeo(rGeo.maOrigin.X(), rGeo.maOrigin.Y(), rGeo.mnDistX, rSeg.nYOffset,
                                  rGeo.mfWidth, rSeg.nHeight, nOrientation, aWavePixelSize,
                                  bDrawAsRect);

        PrimitiveRenderer::DrawWaveLine(rOutDev, aWaveGeo, aColor);
    }
}

void PrimitiveRenderer::DrawStraightTextLine(OutputDevice& rOutDev,
                                             const vcl::rendercontext::TextLineGeometry& rGeo,
                                             tools::Long nY, Color aColor, bool bIsAbove)
{
    static bool bFuzzing = comphelper::IsFuzzing();
    if (bFuzzing && rGeo.mfWidth > 25000)
    {
        SAL_WARN("vcl.gdi", "drawLine, skipping suspicious TextLine of length: "
                                << rGeo.mfWidth << " for fuzzing performance");
        return;
    }

    // Ask TextDecorator to calculate the metrics based on the font data
    vcl::text::StraightLineMetrics aMetrics(*rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline,
                                            nY, bIsAbove);

    if (!aMetrics.nLineHeight)
        return;

    if (rOutDev.mpGraphicsState->mbLineColor || rOutDev.mbLineColorDirty)
    {
        rOutDev.mpGraphics->SetLineColor();
        rOutDev.mbLineColorDirty = true;
    }

    rOutDev.mpGraphics->SetFillColor(aColor);
    rOutDev.mbFillColorDirty = true;

    tools::Long nLeft = rGeo.mnDistX;

    // Dispatch to the actual rendering calls using the sanitized metrics
    switch (aMetrics.eUnderline)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_BOLD:
            PrimitiveRenderer::DrawTextRect(
                *rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin,
                tools::Rectangle(Point(nLeft, aMetrics.nLinePos),
                                 Size(rGeo.mfWidth, aMetrics.nLineHeight)),
                rOutDev.mpFontRealization->mxFont->mnOrientation);
            break;
        case LINESTYLE_DOUBLE:
            PrimitiveRenderer::DrawTextRect(
                *rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin,
                tools::Rectangle(Point(nLeft, aMetrics.nLinePos),
                                 Size(rGeo.mfWidth, aMetrics.nLineHeight)),
                rOutDev.mpFontRealization->mxFont->mnOrientation);
            PrimitiveRenderer::DrawTextRect(
                *rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin,
                tools::Rectangle(Point(nLeft, aMetrics.nLinePos2),
                                 Size(rGeo.mfWidth, aMetrics.nLineHeight)),
                rOutDev.mpFontRealization->mxFont->mnOrientation);
            break;
        default:
        {
            std::vector<vcl::text::TextDashSegment> aSegments
                = vcl::text::TextDecorator::CalculateTextLineSegments(
                    rGeo.mfWidth, aMetrics.eUnderline, aMetrics.nLineHeight, rOutDev.GetDPIX(),
                    rOutDev.GetDPIY());

            for (const auto& rSeg : aSegments)
            {
                PrimitiveRenderer::DrawTextRect(
                    *rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin,
                    tools::Rectangle(Point(nLeft + rSeg.nX, aMetrics.nLinePos),
                                     Size(rSeg.nWidth, aMetrics.nLineHeight)),
                    rOutDev.mpFontRealization->mxFont->mnOrientation);
            }
        }
        break;
    }
}

void PrimitiveRenderer::DrawStrikeoutLine(OutputDevice& rOutDev,
                                          const vcl::rendercontext::TextLineGeometry& rGeo,
                                          tools::Long nY, Color aColor)
{
    if (!rGeo.mfWidth)
        return;

    vcl::text::StrikeoutGeometry aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
        *rOutDev.mpFontInstance->mxFontMetric, rGeo.meStrikeout, nY);

    if (aGeo.aSegments.empty())
        return;

    if (rOutDev.mpGraphicsState->mbLineColor || rOutDev.mbLineColorDirty)
    {
        rOutDev.mpGraphics->SetLineColor();
        rOutDev.mbLineColorDirty = true;
    }

    rOutDev.mpGraphics->SetFillColor(aColor);
    rOutDev.mbFillColorDirty = true;

    for (const auto& rSeg : aGeo.aSegments)
    {
        PrimitiveRenderer::DrawTextRect(
            *rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin,
            tools::Rectangle(Point(rGeo.mnDistX, rSeg.nYOffset), Size(rGeo.mfWidth, rSeg.nHeight)),
            rOutDev.mpFontRealization->mxFont->mnOrientation);
    }
}

void PrimitiveRenderer::DrawStrikeoutChar(OutputDevice& rOutDev,
                                          const vcl::rendercontext::TextLineGeometry& rGeo,
                                          tools::Long nY, Color aColor)
{
    if (!rGeo.mfWidth)
        return;

    vcl::text::LayoutResources aRes{ rOutDev.mpFontInstance.get(),
                                     *rOutDev.mpMapper,
                                     &rOutDev.GetFontCache(),
                                     rOutDev.GetFontCollection(),
                                     nullptr, // pForcedFallback
                                     [&]() { return rOutDev.mpGraphics; },
                                     rOutDev.IsRTLEnabled(),
                                     false, // bSubpixelPositioning
                                     *rOutDev.mpGraphicsState,
                                     *rOutDev.mpFontRealization };

    std::unique_ptr<SalLayout> pLayout
        = vcl::text::TextGeometry::GetStrikeoutCharLayout(aRes, rGeo.mfWidth, rGeo.meStrikeout);

    if (!pLayout)
        return;

    Point aOriginPt = rGeo.maOrigin;
    if (rGeo.mnDistX || nY)
    {
        tools::Long nTmpX = rGeo.mnDistX;
        tools::Long nTmpY = nY;

        if (rOutDev.mpFontInstance->mnOrientation)
        {
            Point aPivot(0, 0);
            aPivot.RotateAround(nTmpX, nTmpY, rOutDev.mpFontInstance->mnOrientation);
        }

        aOriginPt.AdjustX(nTmpX);
        aOriginPt.AdjustY(nTmpY);
    }

    const Color aOldColor = rOutDev.GetTextColor();
    rOutDev.SetTextColor(aColor);
    rOutDev.ImplInitTextColor();

    // CRITICAL FIX: rGeo.maOrigin already contains rOutDev.mpFontRealization offsets!
    // Do not add them again here, otherwise strikeout characters render completely out of bounds.
    pLayout->DrawBase() = basegfx::B2DPoint(aOriginPt.X(), aOriginPt.Y());

    // Fix the clipping rectangle to also use the un-shifted origin
    tools::Rectangle aPixelRect;
    aPixelRect.SetLeft(aOriginPt.X());
    aPixelRect.SetRight(aPixelRect.Left() + rGeo.mfWidth);
    aPixelRect.SetBottom(aOriginPt.Y() + rOutDev.mpFontInstance->mxFontMetric->GetDescent());
    aPixelRect.SetTop(aOriginPt.Y() - rOutDev.mpFontInstance->mxFontMetric->GetAscent());

    if (rOutDev.mpFontInstance->mnOrientation)
    {
        tools::Polygon aPoly(aPixelRect);
        aPoly.Rotate(aOriginPt, rOutDev.mpFontInstance->mnOrientation);
        aPixelRect = aPoly.GetBoundRect();
    }

    pLayout->DrawText(*rOutDev.mpGraphics);

    rOutDev.SetTextColor(aOldColor);
    rOutDev.ImplInitTextColor();
}

void PrimitiveRenderer::DrawTextLine(OutputDevice& rOutDev,
                                     const vcl::rendercontext::TextLineGeometry& rGeo)
{
    // Ask TextDecorator to calculate the vertical offsets based on the font data
    vcl::text::TextLineOffsetInfo aInfo(*rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline,
                                        rGeo.meOverline, rGeo.mbUnderlineAbove);

    Color aStrikeoutColor = rOutDev.GetTextColor();
    Color aUnderlineColor = rOutDev.GetTextLineColor();
    Color aOverlineColor = rOutDev.GetOverlineColor();

    if (!rOutDev.IsTextLineColor())
        aUnderlineColor = rOutDev.GetTextColor();

    if (!rOutDev.IsOverlineColor())
        aOverlineColor = rOutDev.GetTextColor();

    vcl::rendercontext::TextLineGeometry aDrawGeo = rGeo;
    if (rOutDev.IsRTLEnabled())
    {
        tools::Long nXAdd = aDrawGeo.mfWidth - aDrawGeo.mnDistX;
        if (rOutDev.mpFontInstance->mnOrientation)
            nXAdd = basegfx::fround<tools::Long>(
                nXAdd * cos(toRadians(rOutDev.mpFontInstance->mnOrientation)));
        aDrawGeo.maOrigin.AdjustX(nXAdd - 1);
    }

    if (aDrawGeo.meUnderline != LINESTYLE_NONE)
    {
        if (aInfo.bUnderlineIsWave)
            PrimitiveRenderer::DrawWaveTextLine(rOutDev, aDrawGeo, aInfo.nUnderlineOffset,
                                                aUnderlineColor, aDrawGeo.mbUnderlineAbove);
        else
            // Straight lines manage their own offsets mathematically; pass 0
            PrimitiveRenderer::DrawStraightTextLine(rOutDev, aDrawGeo, 0, aUnderlineColor,
                                                    aDrawGeo.mbUnderlineAbove);
    }

    if (aDrawGeo.meOverline != LINESTYLE_NONE)
    {
        // Trick the sub-routines into rendering the overline
        vcl::rendercontext::TextLineGeometry aOverlineGeo = aDrawGeo;
        aOverlineGeo.meUnderline = aDrawGeo.meOverline;

        if (aInfo.bOverlineIsWave)
            PrimitiveRenderer::DrawWaveTextLine(rOutDev, aOverlineGeo, aInfo.nOverlineOffset,
                                                aOverlineColor, true);
        else
            // Straight lines manage their own offsets mathematically; pass 0
            PrimitiveRenderer::DrawStraightTextLine(rOutDev, aOverlineGeo, 0, aOverlineColor, true);
    }

    if (aDrawGeo.meStrikeout != STRIKEOUT_NONE)
    {
        if (aDrawGeo.meStrikeout == STRIKEOUT_SLASH || aDrawGeo.meStrikeout == STRIKEOUT_X)
            PrimitiveRenderer::DrawStrikeoutChar(rOutDev, aDrawGeo, 0, aStrikeoutColor);
        else
            PrimitiveRenderer::DrawStrikeoutLine(rOutDev, aDrawGeo, aInfo.nStrikeoutOffset,
                                                 aStrikeoutColor);
    }
}

void PrimitiveRenderer::DrawTextLines(OutputDevice& rOutDev, SalLayout& rSalLayout,
                                      FontStrikeout eStrikeout, FontLineStyle eUnderline,
                                      FontLineStyle eOverline, bool bWordLine, bool bUnderlineAbove)
{
    if (bWordLine)
    {
        const basegfx::B2DPoint aStartPt = rSalLayout.DrawBase();
        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(rSalLayout, *rOutDev.mpFontRealization,
                                                     aSegments);
        for (const auto& rSeg : aSegments)
        {
            {
                vcl::rendercontext::TextLineGeometry aLineGeo(
                    Point(aStartPt.getX(), aStartPt.getY()), static_cast<tools::Long>(rSeg.first),
                    rSeg.second, eStrikeout, eUnderline, eOverline, bUnderlineAbove);
                aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
                PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
            }
        }
    }
    else
    {
        basegfx::B2DPoint aStartPt = rSalLayout.GetDrawPosition();
        {
            vcl::rendercontext::TextLineGeometry aLineGeo(Point(aStartPt.getX(), aStartPt.getY()),
                                                          0, rSalLayout.GetTextWidth(), eStrikeout,
                                                          eUnderline, eOverline, bUnderlineAbove);
            aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
            PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
        }
    }
}

void PrimitiveRenderer::DrawMnemonicLine(OutputDevice& rOutDev, tools::Long nX, tools::Long nY,
                                         tools::Long nWidth)
{
    tools::Long nBaseX = nX;
    if (rOutDev.IsRTLEnabled())
    {
        // FIXME we need to resolve this, but this reverts the hack that will be done later in DrawTextLine
        nX = nBaseX - nWidth - (nX - nBaseX - 1);
    }

    {
        vcl::rendercontext::TextLineGeometry aLineGeo(Point(nX, nY), 0, nWidth, STRIKEOUT_NONE,
                                                      LINESTYLE_SINGLE, LINESTYLE_NONE, false);
        aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
        PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
    }
}

void PrimitiveRenderer::DrawEmphasisMark(OutputDevice& rOutDev, SalGraphics& rGraphics,
                                         tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                         const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                         const tools::Rectangle& rRect1,
                                         const tools::Rectangle& rRect2)
{
    if (rOutDev.IsRTLEnabled())
        nX = nBaseX - (nX - nBaseX - 1);

    nX -= rOutDev.GetOutOffXPixel();
    nY -= rOutDev.GetOutOffYPixel();

    if (rPolyPoly.Count())
    {
        if (bPolyLine)
        {
            tools::Polygon aPoly = rPolyPoly.GetObject(0);
            aPoly.Move(nX, nY);
            PrimitiveRenderer::DrawPolyLine(rOutDev, aPoly);
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move(nX, nY);
            PrimitiveRenderer::DrawPolyPolygon(rOutDev, aPolyPoly);
        }
    }

    if (!rRect1.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect1.Left(), nY + rRect1.Top()), rRect1.GetSize());
        {
            tools::Rectangle aDeviceRect = rOutDev.mpMapper->LogicToDevicePixel(aRect);
            bool bRTL = rOutDev.IsRTLEnabled()
                        || (rOutDev.mpGraphics
                            && (rOutDev.mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
            bool bAntiparallel = rOutDev.ImplIsAntiparallel();
            tools::Long nFrameWidth = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel()
                                                          : rOutDev.mpGraphics->GetGraphicsWidth();
            rOutDev.mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, bAntiparallel);
            PrimitiveRenderer::DrawDeviceRect(rGraphics, aDeviceRect);
        }
    }

    if (!rRect2.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect2.Left(), nY + rRect2.Top()), rRect2.GetSize());
        {
            tools::Rectangle aDeviceRect = rOutDev.mpMapper->LogicToDevicePixel(aRect);
            bool bRTL = rOutDev.IsRTLEnabled()
                        || (rOutDev.mpGraphics
                            && (rOutDev.mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
            bool bAntiparallel = rOutDev.ImplIsAntiparallel();
            tools::Long nFrameWidth = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel()
                                                          : rOutDev.mpGraphics->GetGraphicsWidth();
            rOutDev.mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, bAntiparallel);
            PrimitiveRenderer::DrawDeviceRect(rGraphics, aDeviceRect);
        }
    }
}

void PrimitiveRenderer::DrawEmphasisMarks(OutputDevice& rOutDev, SalLayout& rSalLayout)
{
    vcl::font::FontRealization const* pRealization = rOutDev.mpFontRealization.get();
    if (!pRealization || !pRealization->mxFont)
        return;

    auto popIt = rOutDev.ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR
                                    | vcl::PushFlags::MAPMODE);
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(rOutDev.maRecorder);
    rOutDev.mpMapper->EnableMapMode(false);

    FontEmphasisMark nEmphasisMark = rOutDev.mpGraphicsState->maFont.GetEmphasisMarkStyle();
    const bool bBelow = bool(nEmphasisMark & FontEmphasisMark::PosBelow);

    tools::Long nEmphasisHeight
        = bBelow ? pRealization->nEmphasisDescent : pRealization->nEmphasisAscent;
    vcl::font::EmphasisMark aEmphasisMark(nEmphasisMark, nEmphasisHeight, rOutDev.GetDPIY());

    if (aEmphasisMark.IsShapePolyLine())
    {
        rOutDev.SetLineColor(rOutDev.GetTextColor());
        rOutDev.SetFillColor();
    }
    else
    {
        rOutDev.SetLineColor();
        rOutDev.SetFillColor(rOutDev.GetTextColor());
    }

    if (!rOutDev.mpGraphics && !rOutDev.AcquireGraphics())
        return;

    std::vector<Point> aPositions;
    vcl::text::TextDecorator::GetEmphasisMarkPositions(rSalLayout, *pRealization, aEmphasisMark,
                                                       bBelow, aPositions);

    for (const Point& rPos : aPositions)
    {
        PrimitiveRenderer::DrawEmphasisMark(
            rOutDev, *rOutDev.mpGraphics, rSalLayout.DrawBase().getX(), rPos.X(), rPos.Y(),
            aEmphasisMark.GetShape(), aEmphasisMark.IsShapePolyLine(), aEmphasisMark.GetRect1(),
            aEmphasisMark.GetRect2());
    }
}

void PrimitiveRenderer::DrawLine(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                 const Point& rStart, const Point& rEnd, bool bTryAA,
                                 bool bPixelSnapHairline)
{
    Point aDeviceStart = rMapper.LogicToDevicePixel(rStart);
    Point aDeviceEnd = rMapper.LogicToDevicePixel(rEnd);
    DrawDeviceLine(rGraphics, aDeviceStart, aDeviceEnd, bTryAA, bPixelSnapHairline);
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
