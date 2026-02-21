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
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <GraphicsState.hxx>

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

bool PrimitiveRenderer::DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                                     double fLineWidth, basegfx::B2DLineJoin eLineJoin,
                                     css::drawing::LineCap eLineCap,
                                     const basegfx::B2DHomMatrix& rObjectTransform,
                                     double fMiterMinimumAngle, double fTransparency,
                                     const std::vector<double>* pStroke) // MM01
{
    auto drawB2DPolyline = [&]() -> bool {
        assert(!rOutDev.is_double_buffered_window());

        if (!rB2DPolygon.count())
            return true;

        if ((!rOutDev.mpGraphics && !rOutDev.AcquireGraphics()) || !rOutDev.CanDrawPolyline())
            return false;

        const basegfx::B2DHomMatrix aTransform(rOutDev.mpMapper->GetDeviceTransformation()
                                               * rObjectTransform);

        if (rOutDev.GetRasterOp() == RasterOp::OverPaint && rOutDev.IsLineColor())
        {
            const bool bPixelSnapHairline
                = (rOutDev.mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
                  && rB2DPolygon.count() < 1000;

            bool bDone = rOutDev.mpGraphics->DrawPolyLine(
                aTransform, rB2DPolygon, fTransparency, fLineWidth, pStroke, eLineJoin, eLineCap,
                fMiterMinimumAngle, bPixelSnapHairline, rOutDev);

            if (bDone)
                return true;
        }

        basegfx::B2DPolygon aDevicePoly(rB2DPolygon);
        aDevicePoly.transform(aTransform);

        auto[aFallbackPolyPoly, aInfo]
            = lcl_SetupStrokeAndLineInfo(aDevicePoly, pStroke, fLineWidth, eLineJoin, eLineCap);

        rOutDev.DrawPolyLineGeometry(aFallbackPolyPoly, aInfo);

        return true;
    };

    bool bSuccess = drawB2DPolyline();

    if (bSuccess && rOutDev.maRecorder.IsActive())
    {
        LineInfo aLineInfo;
        if (fLineWidth != 0.0)
            aLineInfo.SetWidth(std::round(fLineWidth));

        aLineInfo.SetLineJoin(eLineJoin);
        aLineInfo.SetLineCap(eLineCap);

        // Note: We don't record the dashing/stroke here because DrawPolyLineDirect
        // historically records the logical path, but you can adjust this if needed!
        rOutDev.maRecorder.RecordPolyLine(tools::Polygon(rB2DPolygon), aLineInfo);
    }

    return bSuccess;
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
