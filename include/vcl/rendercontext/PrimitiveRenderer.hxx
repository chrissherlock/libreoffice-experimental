/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/solar.h>
#include <tools/degree.hxx>
#include <tools/fontenum.hxx>
#include <tools/long.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <basegfx/numeric/ftools.hxx>

#include <vcl/dllapi.h>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/InvertFlags.hxx>
#include <vcl/rendercontext/RasterOp.hxx>
#include <vcl/rendercontext/DrawGridFlags.hxx>
#include <vcl/rendercontext/TextLineGeometry.hxx>

#include <com/sun/star/drawing/LineCap.hpp>

#include <vector>

class SalGraphics;
class CoordinateMapper;
class Point;
class Size;
class Color;
class OutputDevice;
class SalLayout;
class LineInfo;

namespace tools
{
class Polygon;
class PolyPolygon;
class Rectangle;
}

namespace vcl::rendercontext
{
struct StrokeAttributes;
struct WaveLineGeometry;

class VCL_DLLPUBLIC PrimitiveRenderer
{
public:
    /** Renders a single pixel at the specified logical coordinates using the current line color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const Point& rLogicalPt);

    /** Renders a single pixel at the specified logical coordinates with a specific color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const Point& rLogicalPt, const Color& rColor);

    /** Retrieves a single pixel color at the specified logical coordinates. */
    static Color GetPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const Point& rLogicalPt);

    /** Renders a line between two logical points, with optional Anti-Aliasing. */
    static void DrawLine(SalGraphics& rGraphics, const Point& rDeviceStart, const Point& rDeviceEnd,
                         bool bTryAA = false, bool bPixelSnapHairline = false);

    /** Renders a rectangle using the current line and fill colors. */
    static void DrawRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect);

    static void DrawRoundedRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                                sal_uLong nHorzRoundPixel, sal_uLong nVertRoundPixel,
                                bool bFillColor);

    static bool DrawPolyLine(SalGraphics& rGraphics, const basegfx::B2DPolygon& rPoly,
                             const StrokeAttributes& rStroke,
                             const basegfx::B2DHomMatrix& rObjectTransform, AntialiasingFlags nAA,
                             RasterOp eROP);

    static void DrawPolyLineGeometry(SalGraphics& rGraphics,
                                     const basegfx::B2DPolyPolygon& rPolyPolygon,
                                     const LineInfo& rLineInfo);

    static void DrawPolygon(SalGraphics& rGraphics, const basegfx::B2DHomMatrix& rTransform,
                            const basegfx::B2DPolygon& rDevicePoly, bool bFill,
                            const StrokeAttributes* pStroke);

    static void DrawPolygon(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly, bool bFill);

    static void DrawPolygonGeometry(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly);

    static void DrawClippedPolygon(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly,
                                   const tools::PolyPolygon& rDeviceClipPolyPoly);

    static void DrawPolyPolygon(SalGraphics& rGraphics, const basegfx::B2DHomMatrix& rTransform,
                                const basegfx::B2DPolyPolygon& rDevicePolyPoly, bool bFill,
                                const StrokeAttributes* pStroke);

    static void DrawDevicePolyPolygon(SalGraphics& rGraphics,
                                      const tools::PolyPolygon& rDevicePolyPoly, bool bFill,
                                      const StrokeAttributes* pStroke);

    static void DrawPolyPolygon(SalGraphics& rGraphics, const tools::PolyPolygon& rPolyPoly,
                                const tools::PolyPolygon* pClipPolyPoly = nullptr);

    static void DrawDevicePolyPolygonGeometry(SalGraphics& rGraphics,
                                              const tools::PolyPolygon& rDevicePolyPoly);

    static void DrawPolyPolygonGeometry(SalGraphics& rGraphics,
                                        const tools::PolyPolygon& rPolyPolygon);

    static void DrawEllipse(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                            const tools::Rectangle& rPixelRect, bool bFill, tools::Long nFrameWidth,
                            bool bRTL);

    static void DrawArc(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                        const tools::Rectangle& rPixelRect, const Point& rPixelStart,
                        const Point& rPixelEnd, tools::Long nFrameWidth, bool bRTL);

    static void DrawPie(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                        const tools::Rectangle& rPixelRect, const Point& rPixelStart,
                        const Point& rPixelEnd, bool bFill, tools::Long nFrameWidth, bool bRTL);

    static void DrawChord(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const tools::Rectangle& rPixelRect, const Point& rPixelStart,
                          const Point& rPixelEnd, bool bFill, tools::Long nFrameWidth, bool bRTL);

    static void Invert(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                       const OutputDevice* pOutDev, const tools::Rectangle& rLogicalRect,
                       InvertFlags nFlags);

    static void Invert(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                       const OutputDevice* pOutDev, const tools::Polygon& rLogicalPoly,
                       InvertFlags nFlags);

    static void DrawGrid(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                         const tools::Rectangle& rRect, const tools::Rectangle& rDstRect,
                         const Size& rDist, DrawGridFlags nFlags);

    static void DrawGridOfCrosses(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                  const tools::Rectangle& rGridArea, const Size& rGridDistance,
                                  const tools::Rectangle& rDrawingArea);

    static void DrawTextRect(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                             const Point& rBasePt, const tools::Rectangle& rRect,
                             Degree10 nOrientation, tools::Long nFrameWidth, bool bRTL,
                             bool bAntiparallel);
    static void DrawTextLines(OutputDevice& rOutDev, SalLayout& rSalLayout,
                              FontStrikeout eStrikeout, FontLineStyle eUnderline,
                              FontLineStyle eOverline, bool bWordLine, bool bUnderlineAbove);
    static void DrawTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo);
    static void DrawEmphasisMark(OutputDevice& rOutDev, SalGraphics& rGraphics, tools::Long nBaseX,
                                 tools::Long nX, tools::Long nY,
                                 const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                 const tools::Rectangle& rRect1, const tools::Rectangle& rRect2);
    static void DrawEmphasisMarks(OutputDevice& rOutDev, SalLayout& rSalLayout);
    static void DrawMnemonicLine(OutputDevice& rOutDev, tools::Long nX, tools::Long nY,
                                 tools::Long nWidth);

    // change to private after full migration
    static void DrawStraightTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo,
                                     tools::Long nY, Color aColor, bool bIsAbove);
    static void DrawWaveLine(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                             const Color& rColor);
    static void DrawWaveTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo,
                                 tools::Long nDistY, Color aColor, bool bIsAbove);
    static void DrawWaveLineHairline(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                     const Color& rColor);
    static void DrawWaveLineRasterized(OutputDevice& rOutDev, const WaveLineGeometry& rGeo,
                                       const Color& rColor);
    static void DrawWaveLineBezier(OutputDevice& rOutDev, SalGraphics& rGraphics,
                                   tools::Long nStartX, tools::Long nStartY, tools::Long nEndX,
                                   tools::Long nEndY, tools::Long nWaveHeight, double fOrientation,
                                   tools::Long nLineWidth);
    static void DrawStrikeoutLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo,
                                  tools::Long nY, Color aColor);
    static void DrawStrikeoutChar(OutputDevice& rOutDev, const TextLineGeometry& rGeo,
                                  tools::Long nY, Color aColor);

private:
    static void DrawSinglePolygon(SalGraphics& rGraphics, const tools::Polygon& rPoly);

    static void DrawMultiplePolygons(SalGraphics& rGraphics, const tools::PolyPolygon& rPolyPoly);
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
