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
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <basegfx/numeric/ftools.hxx>

#include <vcl/dllapi.h>

#include <com/sun/star/drawing/LineCap.hpp>

#include <vector>

class SalGraphics;
class CoordinateMapper;
class Point;
class Color;
class OutputDevice;
class LineInfo;
namespace tools
{
class Polygon;
class PolyPolygon;
}

namespace tools
{
class Rectangle;
}

namespace vcl::rendercontext
{
struct StrokeAttributes;

class VCL_DLLPUBLIC PrimitiveRenderer
{
public:
    /** Renders a single pixel at the specified logical coordinates using the current line color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt);

    /** Renders a single pixel at the specified logical coordinates with a specific color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt,
                          const Color& rColor);

    /** Retrieves a single pixel color at the specified logical coordinates. */
    static Color GetPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt);

    /** Renders a line between two logical points, with optional Anti-Aliasing. */
    static void DrawLine(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                         const OutputDevice* pOutDev, const Point& rLogicalStart,
                         const Point& rLogicalEnd, bool bTryAA = false,
                         bool bPixelSnapHairline = false);

    /** Renders a rectangle using the current line and fill colors. */
    static void DrawRect(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                         const OutputDevice* pOutDev, const tools::Rectangle& rLogicalRect);

    static void DrawRoundedRect(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                                const OutputDevice* pOutDev, const tools::Rectangle& rLogicalRect,
                                sal_uLong nHorzRound, sal_uLong nVertRound, bool bFillColor);

    static void DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly);

    static bool DrawPolygon(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                            bool bFill, const StrokeAttributes* pStroke);

    static bool DrawPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly, bool bFill,
                            const StrokeAttributes* pStroke);

    static bool DrawPolyPolygon(OutputDevice& rOutDev, const basegfx::B2DPolyPolygon& rB2DPolyPoly,
                                bool bFill, const StrokeAttributes* pStroke);

    static bool DrawPolyPolygon(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly,
                                bool bFill, const StrokeAttributes* pStroke);

    static void DrawPolyPolygon(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly,
                                const tools::PolyPolygon* pClipPolyPoly);

    static void DrawPolyPolygonFallback(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly);

    static void DrawPolyPolygonGeometry(OutputDevice& rOutDev,
                                        const tools::PolyPolygon& rPolyPolygon);

    static void DrawPolygonGeometry(OutputDevice& rOutDev, const tools::Polygon& rPoly);

    static void DrawPolyLine(OutputDevice& rOutDev, const tools::Polygon& rPoly);

    static void DrawPolyLine(OutputDevice& rOutDev, const tools::Polygon& rPoly,
                             const LineInfo& rLineInfo);

    static bool DrawPolyLine(OutputDevice& rOutDev, const basegfx::B2DPolygon& rB2DPolygon,
                             const StrokeAttributes& rStroke,
                             const basegfx::B2DHomMatrix& rObjectTransform
                             = basegfx::B2DHomMatrix());

    static void DrawPolyLineGeometry(OutputDevice& rOutDev,
                                     const basegfx::B2DPolyPolygon& rPolyPolygon,
                                     const LineInfo& rLineInfo);

    static void DrawClippedPolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly,
                                   const tools::PolyPolygon& rClipPolyPoly);

    static void DrawEllipse(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect, bool bFill);

    static void DrawArc(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                        const Point& rPixelStart, const Point& rPixelEnd);

    static void DrawPie(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                        const Point& rPixelStart, const Point& rPixelEnd, bool bFill);

    static void DrawChord(OutputDevice& rOutDev, const tools::Rectangle& rPixelRect,
                          const Point& rPixelStart, const Point& rPixelEnd, bool bFill);

private:
    static void DrawSinglePolygon(OutputDevice& rOutDev, const tools::Polygon& rPoly);

    static void DrawMultiplePolygons(OutputDevice& rOutDev, const tools::PolyPolygon& rPolyPoly);
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
