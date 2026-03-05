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
#include <basegfx/polygon/b2dpolypolygoncutter.hxx>
#include <basegfx/polygon/WaveLine.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/rendercontext/WaveLineGeometry.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/text/TextGeometry.hxx>

#include <font/EmphasisMark.hxx>
#include <font/FontController.hxx>
#include <salgdi.hxx>
#include <text/TextLayoutEngine.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

#include <com/sun/star/drawing/LineCap.hpp>
#include <com/sun/star/awt/GradientStyle.hpp>

namespace vcl::rendercontext
{
Color PrimitiveRenderer::GetPixel(SalGraphics& rGraphics, tools::Long nX, tools::Long nY)
{
    return rGraphics.getPixel(nX, nY);
}

void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const Point& rDevicePt)
{
    rGraphics.drawPixel(rDevicePt.X(), rDevicePt.Y());
}

void PrimitiveRenderer::DrawPixel(SalGraphics& rGraphics, const Point& rDevicePt,
                                  const Color& rColor)
{
    rGraphics.drawPixel(rDevicePt.X(), rDevicePt.Y(), rColor);
}

Color PrimitiveRenderer::GetPixel(SalGraphics& rGraphics, const Point& rDevicePt)
{
    return Color(rGraphics.getPixel(rDevicePt.X(), rDevicePt.Y()));
}

void PrimitiveRenderer::DrawLine(SalGraphics& rGraphics, const Point& rDeviceStart,
                                 const Point& rDeviceEnd, bool bTryAA, bool bPixelSnapHairline)
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

void PrimitiveRenderer::DrawRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect)
{
    if (!rDeviceRect.IsEmpty())
    {
        // Naked pure virtual backend call!
        rGraphics.drawRect(rDeviceRect.Left(), rDeviceRect.Top(), rDeviceRect.GetWidth(),
                           rDeviceRect.GetHeight());
    }
}

void PrimitiveRenderer::DrawRoundedRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                                        sal_uLong nHorzRoundPixel, sal_uLong nVertRoundPixel,
                                        bool bFillColor)
{
    if (!nHorzRoundPixel && !nVertRoundPixel)
    {
        rGraphics.drawRect(rDeviceRect.Left(), rDeviceRect.Top(), rDeviceRect.GetWidth(),
                           rDeviceRect.GetHeight());
        return;
    }

    tools::Polygon aRoundRectPoly(rDeviceRect, nHorzRoundPixel, nVertRoundPixel);

    if (aRoundRectPoly.GetSize() < 2)
        return;

    Point* pPtAry = aRoundRectPoly.GetPointAry();

    if (!bFillColor)
        rGraphics.drawPolyLine(aRoundRectPoly.GetSize(), pPtAry);
    else
        rGraphics.drawPolygon(aRoundRectPoly.GetSize(), pPtAry);
}

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

static void lcl_DrawHairlinePolyPolygon(SalGraphics& rGraphics,
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
            bDone = rGraphics.drawPolyLine(basegfx::B2DHomMatrix(), rB2DPolygon, 0.0, 0.0, nullptr,
                                           basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                                           basegfx::deg2rad(15.0), bPixelSnapHairline);
        }
        if (!bDone)
        {
            tools::Polygon aPolygon(rB2DPolygon);
            rGraphics.drawPolyLine(aPolygon.GetSize(), aPolygon.GetConstPointAry());
        }
    }
}

static bool lcl_TryDrawB2DAreaGeometry(SalGraphics& rGraphics,
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
    rGraphics.drawPolyPolygon(basegfx::B2DHomMatrix(), rFillPolyPolygon, 0.0);
    return true;
}

static void lcl_DrawSubdividedAreaGeometry(SalGraphics& rGraphics,
                                           const basegfx::B2DPolyPolygon& rFillPolyPolygon)
{
    for (auto const& rB2DPolygon : rFillPolyPolygon)
    {
        tools::Polygon aPolygon(rB2DPolygon);
        aPolygon.AdaptiveSubdivide(aPolygon);
        rGraphics.drawPolygon(aPolygon.GetSize(), aPolygon.GetConstPointAry());
    }
}

static void lcl_DrawAreaGeometry(SalGraphics& rGraphics,
                                 const basegfx::B2DPolyPolygon& rFillPolyPolygon, bool bFuzzing,
                                 bool bTryB2d)
{
    if (!rFillPolyPolygon.count())
        return;
    // Color state is managed by the orchestrator
    if (lcl_TryDrawB2DAreaGeometry(rGraphics, rFillPolyPolygon, bFuzzing, bTryB2d))
        return;
    lcl_DrawSubdividedAreaGeometry(rGraphics, rFillPolyPolygon);
}

void PrimitiveRenderer::DrawPolyLineGeometry(SalGraphics& rGraphics,
                                             const basegfx::B2DPolyPolygon& rPolyPolygon,
                                             const LineInfo& rLineInfo)
{
    auto[aHairlines, aFillGeometry] = lcl_ProcessLineGeometry(rPolyPolygon, rLineInfo);

    const bool bTryB2d = true;
    const bool bPixelSnapHairline = false;
    static const bool bFuzzing = comphelper::IsFuzzing();

    if (aHairlines.count())
        lcl_DrawHairlinePolyPolygon(rGraphics, aHairlines, bTryB2d, bPixelSnapHairline);

    if (aFillGeometry.count())
        lcl_DrawAreaGeometry(rGraphics, aFillGeometry, bFuzzing, bTryB2d);
}

bool PrimitiveRenderer::DrawPolyLine(SalGraphics& rGraphics, const basegfx::B2DPolygon& rPoly,
                                     const StrokeAttributes& rStroke,
                                     const basegfx::B2DHomMatrix& rFullTransform,
                                     AntialiasingFlags nAA, RasterOp eROP)
{
    if (rPoly.count() == 0)
        return true;

    const basegfx::B2DHomMatrix& aTransform = rFullTransform;
    const bool bPixelSnapHairline
        = (nAA & AntialiasingFlags::PixelSnapHairline) && rPoly.count() < 1000;

    if (eROP == RasterOp::OverPaint)
    {
        if (rGraphics.drawPolyLine(aTransform, rPoly, rStroke.fTransparency, rStroke.fWidth,
                                   rStroke.pDashArray, rStroke.eJoin, rStroke.eCap,
                                   rStroke.fMiterMinimumAngle, bPixelSnapHairline))
            return true;
    }

    basegfx::B2DPolygon aDevicePoly(rPoly);
    aDevicePoly.transform(aTransform);
    basegfx::B2DPolyPolygon aPolyPolygon(aDevicePoly);

    if (rStroke.pDashArray && !rStroke.pDashArray->empty())
    {
        basegfx::B2DPolyPolygon aDashedPolyPoly;
        basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(aDevicePoly), *rStroke.pDashArray,
                                         &aDashedPolyPoly);
        aPolyPolygon = aDashedPolyPoly;
    }

    LineInfo aInfo;
    aInfo.SetWidth(std::round(rStroke.fWidth));
    aInfo.SetLineJoin(rStroke.eJoin);
    aInfo.SetLineCap(rStroke.eCap);

    DrawPolyLineGeometry(rGraphics, aPolyPolygon, aInfo);
    return true;
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

void PrimitiveRenderer::DrawPolyPolygonGeometry(SalGraphics& rGraphics,
                                                const tools::PolyPolygon& rDevicePolyPoly)
{
    if (!rDevicePolyPoly.Count())
        return;

    PolyPolyBuffer aBuffer(rDevicePolyPoly);
    if (aBuffer.mnValidCount == 0)
        return;

    if (aBuffer.mnValidCount == 1)
    {
        DrawPolygonGeometry(rGraphics, rDevicePolyPoly.GetObject(aBuffer.mnLastIndex));
        return;
    }

    if (aBuffer.mbHaveBezier)
    {
        if (!rGraphics.drawPolyPolygonBezier(aBuffer.mnValidCount, aBuffer.pPointAry,
                                             aBuffer.pPointAryAry, aBuffer.pFlagAryAry))
        {
            tools::PolyPolygon aSub = tools::PolyPolygon::SubdivideBezier(rDevicePolyPoly);
            DrawPolyPolygonGeometry(rGraphics, aSub);
        }
        return;
    }

    // Naked pure virtual backend call!
    rGraphics.drawPolyPolygon(aBuffer.mnValidCount, aBuffer.pPointAry, aBuffer.pPointAryAry);
}

namespace
{
struct PolygonRenderBuffer
{
    std::unique_ptr<sal_uInt32[]> pPointAry;
    std::unique_ptr<const Point* []> pPointAryAry;
    sal_uInt16 nValidCount = 0;
    sal_uInt16 nFirstValidIndex = 0;

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
                if (nValidCount == 0)
                    nFirstValidIndex = i;
                pPointAry[nValidCount] = nSize;
                pPointAryAry[nValidCount] = rPoly.GetConstPointAry();
                nValidCount++;
            }
        }
    }
};
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

void PrimitiveRenderer::DrawPolygonGeometry(SalGraphics& rGraphics,
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
        rGraphics.drawPolygon(nPoints, pPtAry);
    }
}

void PrimitiveRenderer::DrawPolyPolygon(SalGraphics& rGraphics, const tools::PolyPolygon& rPolyPoly,
                                        const tools::PolyPolygon* pClipPolyPoly)
{
    auto aClippedData = lcl_GetClippedPolyPolygon(rPolyPoly, pClipPolyPoly);
    tools::PolyPolygon* pPolyPoly = aClippedData.pActive;

    if (pPolyPoly->Count() == 1)
        PrimitiveRenderer::DrawSinglePolygon(rGraphics, pPolyPoly->GetObject(0));
    else if (pPolyPoly->Count())
        PrimitiveRenderer::DrawMultiplePolygons(rGraphics, *pPolyPoly);
}

void PrimitiveRenderer::DrawSinglePolygon(SalGraphics& rGraphics, const tools::Polygon& rPoly)
{
    const sal_uInt16 nSize = rPoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = rPoly.GetConstPointAry();
        rGraphics.drawPolygon(nSize, pPtAry);
    }
}

void PrimitiveRenderer::DrawMultiplePolygons(SalGraphics& rGraphics,
                                             const tools::PolyPolygon& rPolyPoly)
{
    if (!rPolyPoly.Count())
        return;

    PolygonRenderBuffer aBuffer(rPolyPoly);

    if (aBuffer.nValidCount == 1)
    {
        rGraphics.drawPolygon(aBuffer.pPointAry[0], aBuffer.pPointAryAry[0]);
    }
    else if (aBuffer.nValidCount > 1)
    {
        rGraphics.drawPolyPolygon(aBuffer.nValidCount, aBuffer.pPointAry.get(),
                                  aBuffer.pPointAryAry.get());
    }
}

void PrimitiveRenderer::DrawClippedPolygon(SalGraphics& rGraphics,
                                           const tools::Polygon& rDevicePoly,
                                           const tools::PolyPolygon& rDeviceClipPolyPoly)
{
    tools::PolyPolygon aClipped;
    tools::PolyPolygon aTmp(rDevicePoly);
    aTmp.GetIntersection(rDeviceClipPolyPoly, aClipped);

    PrimitiveRenderer::DrawPolyPolygonGeometry(rGraphics, aClipped);
}

void PrimitiveRenderer::Invert(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                               InvertFlags nFlags)
{
    tools::Rectangle aDeviceRect(rDeviceRect);

    if (aDeviceRect.IsEmpty())
        return;

    aDeviceRect.Normalize();

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    rGraphics.invert(aDeviceRect.Left(), aDeviceRect.Top(), aDeviceRect.GetWidth(),
                     aDeviceRect.GetHeight(), nSalFlags);
}

void PrimitiveRenderer::Invert(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly,
                               InvertFlags nFlags)
{
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    if (nPoints < 2)
        return;

    tools::Polygon aDevicePoly(rDevicePoly);

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    const Point* pPtAry = aDevicePoly.GetConstPointAry();
    rGraphics.invert(nPoints, pPtAry, nSalFlags);
}

namespace
{
struct GridGeometry
{
    tools::Long nDistX;
    tools::Long nDistY;

    // Device Pixel boundaries for the lines
    tools::Long nPixStartX;
    tools::Long nPixStartY;
    tools::Long nPixRight;
    tools::Long nPixBottom;

    GridGeometry(const tools::Rectangle& rDeviceRect, const tools::Rectangle& rDeviceDstRect,
                 const Size& rDeviceDist)
    {
        nDistX = std::max(rDeviceDist.Width(), tools::Long(1));
        nDistY = std::max(rDeviceDist.Height(), tools::Long(1));

        // Logical alignment logic calculated purely in device space
        nPixStartX = (rDeviceRect.Left() >= rDeviceDstRect.Left())
                         ? rDeviceRect.Left()
                         : (rDeviceRect.Left()
                            + ((rDeviceDstRect.Left() - rDeviceRect.Left()) / nDistX) * nDistX);
        nPixStartY = (rDeviceRect.Top() >= rDeviceDstRect.Top())
                         ? rDeviceRect.Top()
                         : (rDeviceRect.Top()
                            + ((rDeviceDstRect.Top() - rDeviceRect.Top()) / nDistY) * nDistY);

        nPixRight = rDeviceDstRect.Right();
        nPixBottom = rDeviceDstRect.Bottom();
    }

    std::vector<sal_Int32> CalculateOffsets(bool bIsVertical) const
    {
        std::vector<sal_Int32> aBuf;
        const tools::Long nStart = bIsVertical ? nPixStartY : nPixStartX;
        const tools::Long nEnd = bIsVertical ? nPixBottom : nPixRight;
        const tools::Long nDist = bIsVertical ? nDistY : nDistX;

        // reserve capacity: (distance / step) + 2 for safety
        aBuf.reserve(((nEnd - nStart) / nDist) + 2);

        tools::Long nPos = nStart;
        aBuf.push_back(nPos);
        while ((nPos += nDist) <= nEnd)
        {
            aBuf.push_back(nPos);
        }
        return aBuf;
    }
};
}

void PrimitiveRenderer::DrawGrid(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                                 const tools::Rectangle& rDeviceDstRect, const Size& rDeviceDist,
                                 DrawGridFlags nFlags)
{
    const GridGeometry aGrid(rDeviceRect, rDeviceDstRect, rDeviceDist);

    std::vector<sal_Int32> aVertBuf;
    if (nFlags & (DrawGridFlags::Dots | DrawGridFlags::HorzLines))
        aVertBuf = aGrid.CalculateOffsets(true);

    std::vector<sal_Int32> aHorzBuf;
    if (nFlags & (DrawGridFlags::Dots | DrawGridFlags::VertLines))
        aHorzBuf = aGrid.CalculateOffsets(false);

    if (nFlags & DrawGridFlags::Dots)
    {
        for (const auto& rY : aVertBuf)
        {
            for (const auto& rX : aHorzBuf)
            {
                rGraphics.drawPixel(rX, rY);
            }
        }
    }

    if (nFlags & DrawGridFlags::HorzLines)
    {
        for (const auto& rY : aVertBuf)
        {
            tools::Long nX1 = aGrid.nPixStartX;
            tools::Long nX2 = aGrid.nPixRight;
            // Mirroring logic for horizontal segments in DrawGrid if necessary
            rGraphics.drawLine(nX1, rY, nX2, rY);
        }
    }

    if (nFlags & DrawGridFlags::VertLines)
    {
        for (const auto& rX : aHorzBuf)
        {
            rGraphics.drawLine(rX, aGrid.nPixStartY, rX, aGrid.nPixBottom);
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

    CrossGridGeometry(const tools::Rectangle& rDeviceGridArea, const Size& rDeviceGridDistance,
                      const tools::Rectangle& rDeviceDrawingArea)
    {
        const tools::Long nDistanceX = std::max(rDeviceGridDistance.Width(), tools::Long(1));
        const tools::Long nDistanceY = std::max(rDeviceGridDistance.Height(), tools::Long(1));

        // Generate device horizontal positions
        aHorzBuffer.reserve(rDeviceGridArea.GetWidth() / nDistanceX + 1);
        tools::Long nX = rDeviceGridArea.Left();
        while (nX <= rDeviceGridArea.Right())
        {
            aHorzBuffer.push_back(nX);
            nX += nDistanceX;
        }

        // Generate device vertical positions
        aVertBuffer.reserve(rDeviceGridArea.GetHeight() / nDistanceY + 1);
        tools::Long nY = rDeviceGridArea.Top();
        while (nY <= rDeviceGridArea.Bottom())
        {
            aVertBuffer.push_back(nY);
            nY += nDistanceY;
        }

        // Store device pixel bounds
        nPixTop = rDeviceDrawingArea.Top();
        nPixBottom = rDeviceDrawingArea.Bottom();
        nPixLeft = rDeviceDrawingArea.Left();
        nPixRight = rDeviceDrawingArea.Right();
    }
};
}

void PrimitiveRenderer::DrawGridOfCrosses(SalGraphics& rGraphics,
                                          const tools::Rectangle& rDeviceGridArea,
                                          const Size& rDeviceGridDistance,
                                          const tools::Rectangle& rDeviceDrawingArea)
{
    const CrossGridGeometry aGrid(rDeviceGridArea, rDeviceGridDistance, rDeviceDrawingArea);

    for (const auto& nY : aGrid.aVertBuffer)
    {
        if (nY < aGrid.nPixTop || nY > aGrid.nPixBottom)
            continue;

        for (const auto& nX : aGrid.aHorzBuffer)
        {
            if (nX < aGrid.nPixLeft || nX > aGrid.nPixRight)
                continue;

            // Draw a 3x3 cross centered at (nX, nY) using purely pre-calculated device coords
            rGraphics.drawPixel(nX, nY);
            rGraphics.drawPixel(nX - 1, nY);
            rGraphics.drawPixel(nX + 1, nY);
            rGraphics.drawPixel(nX, nY - 1);
            rGraphics.drawPixel(nX, nY + 1);
        }
    }
}

void PrimitiveRenderer::DrawTextDecoration(SalGraphics& rGraphics,
                                           const vcl::text::RotatedGeometry& rDeviceGeo)
{
    if (rDeviceGeo.mbIsPolygon)
        PrimitiveRenderer::DrawPolygonGeometry(rGraphics, rDeviceGeo.maPoly);
    else
        PrimitiveRenderer::DrawRect(rGraphics, rDeviceGeo.maRect);
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
    rGraphics.drawPolyLine(aRotationMatrix, aWaveLinePolygon, 0.0, nLineWidth,
                           nullptr, // MM01
                           basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                           basegfx::deg2rad(15.0), bPixelSnapHairline);
}

void PrimitiveRenderer::DrawWaveLineHairline(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                             const Color& rColor)
{
    rOutDev.mpGraphics->SetLineColor(rColor);
    rOutDev.mbLineColorDirty = true;

    const Point aLineStart = rGeo.GetLineStart();
    const Point aLineEnd = rGeo.GetLineEnd();

    tools::Long nX1 = aLineStart.X();
    tools::Long nX2 = aLineEnd.X();

    if (rOutDev.IsRTLEnabled())
    {
        nX1 = rOutDev.GetOutputWidthPixel() - nX1;
        nX2 = rOutDev.GetOutputWidthPixel() - nX2;
    }

    rOutDev.mpGraphics->drawLine(nX1, aLineStart.Y(), nX2, aLineEnd.Y());
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
            tools::Long nX = aDrawPt.X();

            if (rOutDev.IsRTLEnabled())
                nX = rOutDev.GetOutputWidthPixel() - nX - rGeo.maWavePixelSize.Width();

            rOutDev.mpGraphics->drawRect(nX, aDrawPt.Y(), rGeo.maWavePixelSize.Width(),
                                         rGeo.maWavePixelSize.Height());
        }
        else
        {
            rOutDev.mpGraphics->drawPixel(aDrawPt.X(), aDrawPt.Y());
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

    const Degree10 nOrientation = rOutDev.mpFontRealization->mxFont->mnOrientation;
    const bool bRTL
        = rOutDev.IsRTLEnabled() || (rOutDev.mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    const tools::Long nFrameWidth = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel()
                                                        : rOutDev.mpGraphics->GetGraphicsWidth();
    const bool bAntiparallel = rOutDev.ImplIsAntiparallel();
    const tools::Long nLeft = rGeo.mnDistX;

    // Lambda to handle the boilerplate of Rotate -> Mirror -> Draw
    auto fnDrawDecoration = [&](tools::Long nPos, tools::Long nHeight) {
        auto aTextGeo = vcl::text::TextGeometry::GetRotatedGeometry(
            rGeo.maOrigin, tools::Rectangle(Point(nLeft, nPos), Size(rGeo.mfWidth, nHeight)),
            nOrientation);

        if (bRTL)
        {
            if (aTextGeo.mbIsPolygon)
                rOutDev.mpMapper->MirrorDevicePixelPolygon(aTextGeo.maPoly, nFrameWidth, bRTL,
                                                           bAntiparallel);
            else
                rOutDev.mpMapper->MirrorDevicePixelRect(aTextGeo.maRect, nFrameWidth, bRTL,
                                                        bAntiparallel);
        }

        // Dispatch to optimized stateless renderer
        PrimitiveRenderer::DrawTextDecoration(*rOutDev.mpGraphics, aTextGeo);
    };

    switch (aMetrics.eUnderline)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_BOLD:
            fnDrawDecoration(aMetrics.nLinePos, aMetrics.nLineHeight);
            break;

        case LINESTYLE_DOUBLE:
            fnDrawDecoration(aMetrics.nLinePos, aMetrics.nLineHeight);
            fnDrawDecoration(aMetrics.nLinePos2, aMetrics.nLineHeight);
            break;

        default:
        {
            // Handle dashed/dotted lines
            std::vector<vcl::text::TextDashSegment> aSegments
                = vcl::text::TextDecorator::CalculateTextLineSegments(
                    rGeo.mfWidth, aMetrics.eUnderline, aMetrics.nLineHeight, rOutDev.GetDPIX(),
                    rOutDev.GetDPIY());

            for (const auto& rSeg : aSegments)
            {
                auto aTextGeo = vcl::text::TextGeometry::GetRotatedGeometry(
                    rGeo.maOrigin,
                    tools::Rectangle(Point(nLeft + rSeg.nX, aMetrics.nLinePos),
                                     Size(rSeg.nWidth, aMetrics.nLineHeight)),
                    nOrientation);

                if (bRTL)
                {
                    if (aTextGeo.mbIsPolygon)
                        rOutDev.mpMapper->MirrorDevicePixelPolygon(aTextGeo.maPoly, nFrameWidth,
                                                                   bRTL, bAntiparallel);
                    else
                        rOutDev.mpMapper->MirrorDevicePixelRect(aTextGeo.maRect, nFrameWidth, bRTL,
                                                                bAntiparallel);
                }

                PrimitiveRenderer::DrawTextDecoration(*rOutDev.mpGraphics, aTextGeo);
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

    vcl::text::StrikeoutGeometry aStrikeoutGeo
        = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
            *rOutDev.mpFontInstance->mxFontMetric, rGeo.meStrikeout, nY);

    if (aStrikeoutGeo.aSegments.empty())
        return;

    if (rOutDev.mpGraphicsState->mbLineColor || rOutDev.mbLineColorDirty)
    {
        rOutDev.mpGraphics->SetLineColor();
        rOutDev.mbLineColorDirty = true;
    }

    rOutDev.mpGraphics->SetFillColor(aColor);
    rOutDev.mbFillColorDirty = true;

    // Cache layout state for the loop
    const Degree10 nOrientation = rOutDev.mpFontRealization->mxFont->mnOrientation;
    const bool bRTL
        = rOutDev.IsRTLEnabled() || (rOutDev.mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    const tools::Long nFrameWidth = rOutDev.IsVirtual() ? rOutDev.GetOutputWidthPixel()
                                                        : rOutDev.mpGraphics->GetGraphicsWidth();
    const bool bAntiparallel = rOutDev.ImplIsAntiparallel();

    for (const auto& rSeg : aStrikeoutGeo.aSegments)
    {
        // 1. Calculate Rotated Geometry (Device Rect or Polygon)
        auto aTextGeo = vcl::text::TextGeometry::GetRotatedGeometry(
            rGeo.maOrigin,
            tools::Rectangle(Point(rGeo.mnDistX, rSeg.nYOffset), Size(rGeo.mfWidth, rSeg.nHeight)),
            nOrientation);

        // 2. Apply RTL Mirroring locally
        if (bRTL)
        {
            if (aTextGeo.mbIsPolygon)
                rOutDev.mpMapper->MirrorDevicePixelPolygon(aTextGeo.maPoly, nFrameWidth, bRTL,
                                                           bAntiparallel);
            else
                rOutDev.mpMapper->MirrorDevicePixelRect(aTextGeo.maRect, nFrameWidth, bRTL,
                                                        bAntiparallel);
        }

        // 3. Dispatch to the stateless renderer
        PrimitiveRenderer::DrawTextDecoration(*rOutDev.mpGraphics, aTextGeo);
    }
}

/**
 * Calculates the rotated and offset origin point for text decorations.
 *
 * @param rOrigin      The base origin of the text (the baseline start).
 * @param nOrientation The font rotation in Degree10 (0.1 degree units).
 * @param nDistX       The horizontal offset along the baseline.
 * @param nY           The vertical offset relative to the baseline.
 * @return             The final Point in logical coordinates.
 */
static Point lcl_GetDecorationOrigin(const Point& rOrigin, Degree10 nOrientation,
                                     tools::Long nDistX, tools::Long nY)
{
    Point aOriginPt = rOrigin;

    if (nDistX || nY)
    {
        tools::Long nTmpX = nDistX;
        tools::Long nTmpY = nY;

        if (nOrientation)
        {
            // Rotate the relative offsets around the (0,0) pivot
            // before applying them to the absolute origin.
            Point aOffset(0, 0);
            aOffset.RotateAround(nTmpX, nTmpY, nOrientation);
        }

        aOriginPt.AdjustX(nTmpX);
        aOriginPt.AdjustY(nTmpY);
    }

    return aOriginPt;
}

/**
 * Pushes a clipping region to the OutputDevice for a text decoration.
 * Uses RAII to ensure the clip is popped.
 */
[[nodiscard]] static auto lcl_BeginDecorationClipping(OutputDevice& rOutDev, const Point& rOrigin,
                                                      double fWidth, tools::Long nAscent,
                                                      tools::Long nDescent)
{
    tools::Rectangle aPixelRect;
    aPixelRect.SetLeft(rOrigin.X());
    aPixelRect.SetRight(aPixelRect.Left() + static_cast<tools::Long>(fWidth));
    aPixelRect.SetBottom(rOrigin.Y() + nDescent);
    aPixelRect.SetTop(rOrigin.Y() - nAscent);

    const LogicalFontInstance& rFontInst = *rOutDev.GetFontInstance();
    const Degree10 nOrientation = rFontInst.mnOrientation;

    if (nOrientation)
    {
        tools::Polygon aPoly(aPixelRect);
        aPoly.Rotate(rOrigin, nOrientation);
        aPixelRect = aPoly.GetBoundRect();
    }

    // Crucial for overlines and rotated text to prevent "inside-out" empty rects
    aPixelRect.Normalize();

    rOutDev.Push(vcl::PushFlags::CLIPREGION);
    rOutDev.IntersectClipRegion(aPixelRect);

    return comphelper::ScopeGuard([&rOutDev]() { rOutDev.Pop(); });
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

    Point aOriginPt = lcl_GetDecorationOrigin(rGeo.maOrigin, rOutDev.mpFontInstance->mnOrientation,
                                              rGeo.mnDistX, nY);

    const Color aOldColor = rOutDev.GetTextColor();
    rOutDev.SetTextColor(aColor);
    rOutDev.ImplInitTextColor();

    comphelper::ScopeGuard aColorGuard([&rOutDev, aOldColor]() {
        rOutDev.SetTextColor(aOldColor);
        rOutDev.ImplInitTextColor();
    });

    {
        // CRITICAL FIX: rGeo.maOrigin already contains rOutDev.mpFontRealization offsets!
        // Do not add them again here, otherwise strikeout characters render completely out of bounds.
        pLayout->DrawBase() = basegfx::B2DPoint(aOriginPt.X(), aOriginPt.Y());

        const LogicalFontInstance& rFontInst = *rOutDev.GetFontInstance();

        // One call handles calculation, rotation, normalization, and pushing the clip stack
        auto aClipGuard = lcl_BeginDecorationClipping(rOutDev, aOriginPt, rGeo.mfWidth,
                                                      rFontInst.mxFontMetric->GetAscent(),
                                                      rFontInst.mxFontMetric->GetDescent());

        pLayout->DrawText(*rOutDev.mpGraphics);
    }
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

    const LogicalFontInstance& rFontInst = *rOutDev.GetFontInstance();
    const tools::Long nAscent = rFontInst.mxFontMetric->GetAscent();
    const tools::Long nDescent = rFontInst.mxFontMetric->GetDescent();

    if (aDrawGeo.meUnderline != LINESTYLE_NONE)
    {
        if (aInfo.bUnderlineIsWave)
        {
            PrimitiveRenderer::DrawWaveTextLine(rOutDev, aDrawGeo, aInfo.nUnderlineOffset,
                                                aUnderlineColor, aDrawGeo.mbUnderlineAbove);
        }
        else
        {
            // Protect the straight line with a clipped region
            Point aUnderlineOrigin
                = lcl_GetDecorationOrigin(aDrawGeo.maOrigin, rFontInst.mnOrientation,
                                          aDrawGeo.mnDistX, aInfo.nUnderlineOffset);

            auto aClipGuard = lcl_BeginDecorationClipping(rOutDev, aUnderlineOrigin,
                                                          aDrawGeo.mfWidth, nAscent, nDescent);

            PrimitiveRenderer::DrawStraightTextLine(rOutDev, aDrawGeo, 0, aUnderlineColor,
                                                    aDrawGeo.mbUnderlineAbove);
        }
    }

    if (aDrawGeo.meOverline != LINESTYLE_NONE)
    {
        // Trick the sub-routines into rendering the overline
        vcl::rendercontext::TextLineGeometry aOverlineGeo = aDrawGeo;
        aOverlineGeo.meUnderline = aDrawGeo.meOverline;

        if (aInfo.bOverlineIsWave)
        {
            PrimitiveRenderer::DrawWaveTextLine(rOutDev, aOverlineGeo, aInfo.nOverlineOffset,
                                                aOverlineColor, true);
        }
        else
        {
            // The Normalize() fix inside lcl_BeginDecorationClipping prevents the overline
            // from vanishing when positioned at negative offsets.
            Point aOverlineOrigin
                = lcl_GetDecorationOrigin(aDrawGeo.maOrigin, rFontInst.mnOrientation,
                                          aDrawGeo.mnDistX, aInfo.nOverlineOffset);

            auto aClipGuard = lcl_BeginDecorationClipping(rOutDev, aOverlineOrigin,
                                                          aDrawGeo.mfWidth, nAscent, nDescent);

            PrimitiveRenderer::DrawStraightTextLine(rOutDev, aOverlineGeo, 0, aOverlineColor, true);
        }
    }

    if (aDrawGeo.meStrikeout != STRIKEOUT_NONE)
    {
        if (aDrawGeo.meStrikeout == STRIKEOUT_SLASH || aDrawGeo.meStrikeout == STRIKEOUT_X)
        {
            PrimitiveRenderer::DrawStrikeoutChar(rOutDev, aDrawGeo, 0, aStrikeoutColor);
        }
        else
        {
            // Line-based strikeouts
            Point aStrikeoutOrigin
                = lcl_GetDecorationOrigin(aDrawGeo.maOrigin, rFontInst.mnOrientation,
                                          aDrawGeo.mnDistX, aInfo.nStrikeoutOffset);

            auto aClipGuard = lcl_BeginDecorationClipping(rOutDev, aStrikeoutOrigin,
                                                          aDrawGeo.mfWidth, nAscent, nDescent);

            PrimitiveRenderer::DrawStrikeoutLine(rOutDev, aDrawGeo, aInfo.nStrikeoutOffset,
                                                 aStrikeoutColor);
        }
    }
}

void PrimitiveRenderer::DrawTextLines(SalGraphics& rGraphics,
                                      std::span<const vcl::text::RotatedGeometry> rSegments,
                                      const Color& rColor)
{
    // Purely device-pixel based; no layout logic allowed here.
    rGraphics.SetFillColor(rColor);

    for (const auto& rGeo : rSegments)
    {
        PrimitiveRenderer::DrawTextDecoration(rGraphics, rGeo);
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
            rOutDev.DrawPolyLine(aPoly);
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move(nX, nY);
            PrimitiveRenderer::DrawPolyPolygon(rGraphics, aPolyPoly);
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
            PrimitiveRenderer::DrawRect(rGraphics, aDeviceRect);
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
            PrimitiveRenderer::DrawRect(rGraphics, aDeviceRect);
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

void PrimitiveRenderer::DrawPolygon(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly,
                                    bool bFill)
{
    sal_uInt16 nSize = rDevicePoly.GetSize();
    if (nSize == 0)
        return;

    const Point* pPtAry = rDevicePoly.GetConstPointAry();

    if (bFill)
        rGraphics.drawPolygon(nSize, pPtAry);
    else
        rGraphics.drawPolyLine(nSize, pPtAry);
}

void PrimitiveRenderer::DrawPolyPolygon(SalGraphics& rGraphics,
                                        const tools::PolyPolygon& rDevicePolyPoly, bool bFill)
{
    sal_uInt16 nPoly = rDevicePolyPoly.Count();
    if (nPoly == 0)
        return;

    if (bFill)
    {
        std::unique_ptr<sal_uInt32[]> pPoints(new sal_uInt32[nPoly]);
        std::unique_ptr<const Point* []> pPtAry(new const Point*[nPoly]);
        for (sal_uInt16 i = 0; i < nPoly; i++)
        {
            pPoints[i] = rDevicePolyPoly[i].GetSize();
            pPtAry[i] = rDevicePolyPoly[i].GetConstPointAry();
        }
        rGraphics.drawPolyPolygon(nPoly, pPoints.get(), pPtAry.get());
    }
}

void PrimitiveRenderer::DrawPolygon(SalGraphics& rGraphics, const basegfx::B2DHomMatrix& rTransform,
                                    const basegfx::B2DPolygon& rDevicePoly, bool bFill)
{
    basegfx::B2DPolyPolygon aPP(rDevicePoly);
    DrawPolyPolygon(rGraphics, rTransform, aPP, bFill);
}

void PrimitiveRenderer::DrawPolyPolygon(SalGraphics& rGraphics,
                                        const basegfx::B2DHomMatrix& rTransform,
                                        const basegfx::B2DPolyPolygon& rDevicePolyPoly, bool bFill)
{
    if (rDevicePolyPoly.count() == 0)
        return;

    if (bFill)
    {
        // Direct floating-point dispatch to the backend
        rGraphics.drawPolyPolygon(rTransform, rDevicePolyPoly, 0.0);
    }
}

sal_uInt8 PrimitiveRenderer::GetGradientColorValue(tools::Long nValue)
{
    return static_cast<sal_uInt8>(std::clamp<tools::Long>(nValue, 0, 255));
}

bool PrimitiveRenderer::DrawGradient(SalGraphics& rGraphics,
                                     const tools::PolyPolygon& rDevicePolyPoly,
                                     const Gradient& rGradient)
{
    // Pure hardware dispatch
    return rGraphics.drawGradient(rDevicePolyPoly, rGradient);
}

void PrimitiveRenderer::DrawGradient(SalGraphics& rGraphics, const tools::Rectangle& rRect,
                                     const Gradient& rGradient, tools::Long nStepCount,
                                     bool bAvoidVectorOverdraw,
                                     const tools::PolyPolygon* pClipPolyPoly)
{
    // 1. Setup basic geometry and rotation
    tools::Rectangle aRect;
    Point aCenter;
    Degree10 nAngle = rGradient.GetAngle() % 3600_deg10;
    rGradient.GetBoundRect(rRect, aRect, aCenter);

    const css::awt::GradientStyle eStyle = rGradient.GetStyle();
    const bool bLinear = (eStyle == css::awt::GradientStyle_LINEAR);
    const bool bAxial = (eStyle == css::awt::GradientStyle_AXIAL);

    // Prepare Colors
    Color aStartCol = rGradient.GetStartColor();
    Color aEndCol = rGradient.GetEndColor();
    tools::Long nStartRed = (aStartCol.GetRed() * rGradient.GetStartIntensity()) / 100;
    tools::Long nStartGreen = (aStartCol.GetGreen() * rGradient.GetStartIntensity()) / 100;
    tools::Long nStartBlue = (aStartCol.GetBlue() * rGradient.GetStartIntensity()) / 100;
    tools::Long nEndRed = (aEndCol.GetRed() * rGradient.GetEndIntensity()) / 100;
    tools::Long nEndGreen = (aEndCol.GetGreen() * rGradient.GetEndIntensity()) / 100;
    tools::Long nEndBlue = (aEndCol.GetBlue() * rGradient.GetEndIntensity()) / 100;

    // Handle Linear/Axial Composition
    if (bLinear || bAxial)
    {
        double fBorder = rGradient.GetBorder() * aRect.GetHeight() / 100.0;
        if (bAxial)
        {
            fBorder /= 2.0;
            std::swap(nStartRed, nEndRed);
            std::swap(nStartGreen, nEndGreen);
            std::swap(nStartBlue, nEndBlue);
        }

        tools::Rectangle aMirrorRect = aRect;
        aMirrorRect.SetTop((aRect.Top() + aRect.Bottom()) / 2);

        // Border Logic: Compose the solid border if necessary
        if (fBorder > 0.0)
        {
            rGraphics.SetFillColor(Color(static_cast<sal_uInt8>(nStartRed),
                                         static_cast<sal_uInt8>(nStartGreen),
                                         static_cast<sal_uInt8>(nStartBlue)));

            tools::Rectangle aBorderRect = aRect;
            aBorderRect.SetBottom(static_cast<tools::Long>(aBorderRect.Top() + fBorder));
            tools::Polygon aBorderPoly(aBorderRect);
            aBorderPoly.Rotate(aCenter, nAngle);

            if (pClipPolyPoly)
                DrawClippedPolygon(rGraphics, aBorderPoly, *pClipPolyPoly);
            else
                DrawPolygonGeometry(rGraphics, aBorderPoly);

            aRect.SetTop(aBorderRect.Bottom());

            if (bAxial)
            {
                aBorderRect = aMirrorRect;
                aBorderRect.SetTop(static_cast<tools::Long>(aBorderRect.Bottom() - fBorder));
                aBorderPoly = tools::Polygon(aBorderRect);
                aBorderPoly.Rotate(aCenter, nAngle);

                if (pClipPolyPoly)
                    DrawClippedPolygon(rGraphics, aBorderPoly, *pClipPolyPoly);
                else
                    DrawPolygonGeometry(rGraphics, aBorderPoly);

                aMirrorRect.SetBottom(aBorderRect.Top());
            }
        }

        if (bAxial)
            aRect.SetBottom(aMirrorRect.Top());

        // Compose Gradient Steps (Clamped)
        tools::Long nAbsRedSteps = std::abs(nEndRed - nStartRed);
        tools::Long nAbsGreenSteps = std::abs(nEndGreen - nStartGreen);
        tools::Long nAbsBlueSteps = std::abs(nEndBlue - nStartBlue);
        tools::Long nMaxColorSteps = std::max({ nAbsRedSteps, nAbsGreenSteps, nAbsBlueSteps });

        tools::Long nSteps = std::min(nStepCount, nMaxColorSteps);
        nSteps = std::max<tools::Long>(nSteps, 3);

        double fScanInc = static_cast<double>(aRect.GetHeight()) / nSteps;

        // FIX: Cache the static starting lines so iteration doesn't exponentially compound!
        double fGradientLine = static_cast<double>(aRect.Top());
        double fMirrorGradientLine = static_cast<double>(aMirrorRect.Bottom());

        const double fStepsMinus1 = static_cast<double>(nSteps) - 1.0;

        // Axial draws one less step in the loop to handle the middle gap
        if (!bLinear)
            nSteps -= 1;

        tools::Polygon aPoly(4);
        for (tools::Long i = 0; i < nSteps; i++)
        {
            const double fAlpha = static_cast<double>(i) / fStepsMinus1;
            rGraphics.SetFillColor(
                Color(GetGradientColorValue(nStartRed * (1.0 - fAlpha) + nEndRed * fAlpha),
                      GetGradientColorValue(nStartGreen * (1.0 - fAlpha) + nEndGreen * fAlpha),
                      GetGradientColorValue(nStartBlue * (1.0 - fAlpha) + nEndBlue * fAlpha)));

            // Use the static fGradientLine base
            aRect.SetTop(static_cast<tools::Long>(fGradientLine + i * fScanInc));
            aRect.SetBottom(static_cast<tools::Long>(fGradientLine + (i + 1) * fScanInc));
            aPoly = tools::Polygon(aRect);
            aPoly.Rotate(aCenter, nAngle);

            if (pClipPolyPoly)
                DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
            else
                DrawPolygonGeometry(rGraphics, aPoly);

            if (bAxial)
            {
                // Use the static fMirrorGradientLine base
                aMirrorRect.SetBottom(static_cast<tools::Long>(fMirrorGradientLine - i * fScanInc));
                aMirrorRect.SetTop(
                    static_cast<tools::Long>(fMirrorGradientLine - (i + 1) * fScanInc));
                aPoly = tools::Polygon(aMirrorRect);
                aPoly.Rotate(aCenter, nAngle);

                if (pClipPolyPoly)
                    DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
                else
                    DrawPolygonGeometry(rGraphics, aPoly);
            }
        }

        // Draw the middle capstone polygon for Axial to prevent rendering gaps
        if (!bLinear)
        {
            rGraphics.SetFillColor(Color(GetGradientColorValue(nEndRed),
                                         GetGradientColorValue(nEndGreen),
                                         GetGradientColorValue(nEndBlue)));

            aRect.SetTop(static_cast<tools::Long>(fGradientLine + nSteps * fScanInc));
            aRect.SetBottom(static_cast<tools::Long>(fMirrorGradientLine - nSteps * fScanInc));
            aPoly = tools::Polygon(aRect);
            aPoly.Rotate(aCenter, nAngle);

            if (pClipPolyPoly)
                DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
            else
                DrawPolygonGeometry(rGraphics, aPoly);
        }
    }
    else // Complex Styles: Radial, Elliptical, Square
    {
        DrawComplexGradient(rGraphics, rRect, rGradient, nStepCount, bAvoidVectorOverdraw,
                            pClipPolyPoly);
    }
}

void PrimitiveRenderer::DrawComplexGradient(SalGraphics& rGraphics, const tools::Rectangle& rRect,
                                            const Gradient& rGradient, tools::Long nStepCount,
                                            bool bAvoidVectorOverdraw,
                                            const tools::PolyPolygon* pClipPolyPoly)
{
    tools::Rectangle aRect;
    Point aCenter;
    Degree10 nAngle = rGradient.GetAngle() % 3600_deg10;
    rGradient.GetBoundRect(rRect, aRect, aCenter);

    const css::awt::GradientStyle eStyle = rGradient.GetStyle();

    Color aStartCol = rGradient.GetStartColor();
    Color aEndCol = rGradient.GetEndColor();
    tools::Long nStartRed = (aStartCol.GetRed() * rGradient.GetStartIntensity()) / 100;
    tools::Long nStartGreen = (aStartCol.GetGreen() * rGradient.GetStartIntensity()) / 100;
    tools::Long nStartBlue = (aStartCol.GetBlue() * rGradient.GetStartIntensity()) / 100;
    tools::Long nEndRed = (aEndCol.GetRed() * rGradient.GetEndIntensity()) / 100;
    tools::Long nEndGreen = (aEndCol.GetGreen() * rGradient.GetEndIntensity()) / 100;
    tools::Long nEndBlue = (aEndCol.GetBlue() * rGradient.GetEndIntensity()) / 100;

    tools::Long nRedSteps = nEndRed - nStartRed;
    tools::Long nGreenSteps = nEndGreen - nStartGreen;
    tools::Long nBlueSteps = nEndBlue - nStartBlue;

    tools::Long nSteps = std::max<tools::Long>(nStepCount, 2);

    tools::Long nMaxColorDiff
        = std::max({ std::abs(nRedSteps), std::abs(nGreenSteps), std::abs(nBlueSteps) });

    if (nMaxColorDiff < nSteps && nMaxColorDiff > 0)
        nSteps = nMaxColorDiff;

    // Calculate Step Increments
    // Complex gradients shrink from the outer boundary toward the center point
    double fScanLeft = aRect.Left();
    double fScanTop = aRect.Top();
    double fScanRight = aRect.Right();
    double fScanBottom = aRect.Bottom();

    double fScanIncX = static_cast<double>(aRect.GetWidth()) / nSteps * 0.5;
    double fScanIncY = static_cast<double>(aRect.GetHeight()) / nSteps * 0.5;

    // Radial and Elliptical styles require proportional shrinking
    if (eStyle != css::awt::GradientStyle_SQUARE)
    {
        fScanIncY = std::min(fScanIncY, fScanIncX);
        fScanIncX = fScanIncY;
    }

    std::optional<tools::PolyPolygon> xPolyPoly;
    if (bAvoidVectorOverdraw)
        xPolyPoly = tools::PolyPolygon(2);

    rGraphics.SetFillColor(Color(static_cast<sal_uInt8>(nStartRed),
                                 static_cast<sal_uInt8>(nStartGreen),
                                 static_cast<sal_uInt8>(nStartBlue)));

    tools::Polygon aPoly;
    if (xPolyPoly)
    {
        aPoly = tools::Polygon(rRect);
        xPolyPoly->Insert(aPoly);
        xPolyPoly->Insert(aPoly);
    }
    else
    {
        tools::Rectangle aExtRect(rRect);
        aExtRect.AdjustLeft(-1);
        aExtRect.AdjustTop(-1);
        aExtRect.AdjustRight(1);
        aExtRect.AdjustBottom(1);

        aPoly = tools::Polygon(aExtRect);
        if (pClipPolyPoly)
            DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
        else
            DrawPolygonGeometry(rGraphics, aPoly);
    }

    bool bPaintLastPolygon = false;

    for (tools::Long i = 1; i < nSteps; i++)
    {
        fScanLeft += fScanIncX;
        fScanTop += fScanIncY;
        fScanRight -= fScanIncX;
        fScanBottom -= fScanIncY;

        if ((fScanRight - fScanLeft) < 1.0 || (fScanBottom - fScanTop) < 1.0)
            break;

        tools::Rectangle aStepRect(
            static_cast<tools::Long>(fScanLeft), static_cast<tools::Long>(fScanTop),
            static_cast<tools::Long>(fScanRight), static_cast<tools::Long>(fScanBottom));

        // Compose shape geometry based on style
        if (eStyle == css::awt::GradientStyle_RADIAL
            || eStyle == css::awt::GradientStyle_ELLIPTICAL)
            aPoly = tools::Polygon(aStepRect.Center(), aStepRect.GetWidth() >> 1,
                                   aStepRect.GetHeight() >> 1);
        else
            aPoly = tools::Polygon(aStepRect);

        aPoly.Rotate(aCenter, nAngle);

        tools::Long nStepIndex = (xPolyPoly ? i : (i + 1));
        sal_uInt8 nRed = GetGradientColorValue(nStartRed + ((nRedSteps * nStepIndex) / nSteps));
        sal_uInt8 nGreen
            = GetGradientColorValue(nStartGreen + ((nGreenSteps * nStepIndex) / nSteps));
        sal_uInt8 nBlue = GetGradientColorValue(nStartBlue + ((nBlueSteps * nStepIndex) / nSteps));

        if (xPolyPoly)
        {
            bPaintLastPolygon = true;
            xPolyPoly->Replace(xPolyPoly->GetObject(1), 0);
            xPolyPoly->Replace(aPoly, 1);

            DrawPolyPolygon(rGraphics, *xPolyPoly, pClipPolyPoly);
            rGraphics.SetFillColor(Color(nRed, nGreen, nBlue));
        }
        else
        {
            rGraphics.SetFillColor(Color(nRed, nGreen, nBlue));
            if (pClipPolyPoly)
                DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
            else
                DrawPolygonGeometry(rGraphics, aPoly);
        }
    }

    if (!xPolyPoly)
        return;

    const tools::Polygon& rLastPoly = xPolyPoly->GetObject(1);
    if (rLastPoly.GetBoundRect().IsEmpty())
        return;

    if (bPaintLastPolygon)
    {
        sal_uInt8 nFinalRed = GetGradientColorValue(nEndRed);
        sal_uInt8 nFinalGreen = GetGradientColorValue(nEndGreen);
        sal_uInt8 nFinalBlue = GetGradientColorValue(nEndBlue);
        rGraphics.SetFillColor(Color(nFinalRed, nFinalGreen, nFinalBlue));
    }

    if (pClipPolyPoly)
        DrawClippedPolygon(rGraphics, aPoly, *pClipPolyPoly);
    else
        DrawPolygonGeometry(rGraphics, aPoly);
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
