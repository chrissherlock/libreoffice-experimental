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

#include <span>
#include <vector>

class SalGraphics;
class CoordinateMapper;
class Point;
class Size;
class Color;
class OutputDevice;
class SalLayout;
class LineInfo;
class Gradient;

namespace tools
{
class Polygon;
class PolyPolygon;
class Rectangle;
}

namespace vcl::text
{
struct RotatedGeometry;
}

namespace vcl::rendercontext
{
struct StrokeAttributes;
struct WaveLineGeometry;

class VCL_DLLPUBLIC PrimitiveRenderer
{
public:
    static Color GetPixel(SalGraphics& rGraphics, tools::Long nX, tools::Long nY);

    /** Renders a single pixel at the specified device coordinates using the current line color. */
    static void DrawPixel(SalGraphics& rGraphics, const Point& rDevicePt);

    /** Renders a single pixel at the specified device coordinates with a specific color. */
    static void DrawPixel(SalGraphics& rGraphics, const Point& rDevicePt, const Color& rColor);

    /** Retrieves a single pixel color at the specified device coordinates. */
    static Color GetPixel(SalGraphics& rGraphics, const Point& rDevicePt);

    /** Renders a line between two logical points, with optional Anti-Aliasing. */
    static void DrawLine(SalGraphics& rGraphics, const Point& rDeviceStart, const Point& rDeviceEnd,
                         bool bTryAA = false, bool bPixelSnapHairline = false);

    /** Renders a rectangle using the current line and fill colors. */
    static void DrawRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect);

    static void DrawRoundedRect(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                                sal_uLong nHorzRoundPixel, sal_uLong nVertRoundPixel,
                                bool bFillColor);

    static void DrawCheckered(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                              sal_uInt32 nLen, Color aStart, Color aEnd);

    static bool DrawPolyLine(SalGraphics& rGraphics, const basegfx::B2DPolygon& rPoly,
                             const StrokeAttributes& rStroke,
                             const basegfx::B2DHomMatrix& rObjectTransform, AntialiasingFlags nAA,
                             RasterOp eROP);

    static void DrawPolyLineGeometry(SalGraphics& rGraphics,
                                     const basegfx::B2DPolyPolygon& rPolyPolygon,
                                     const LineInfo& rLineInfo);

    static void DrawPolygon(SalGraphics& rGraphics, const basegfx::B2DHomMatrix& rTransform,
                            const basegfx::B2DPolygon& rDevicePoly, bool bFill);

    static void DrawPolygon(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly, bool bFill);

    static void DrawPolygonGeometry(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly);

    static void DrawClippedPolygon(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly,
                                   const tools::PolyPolygon& rDeviceClipPolyPoly);

    static void DrawPolyPolygon(SalGraphics& rGraphics, const basegfx::B2DHomMatrix& rTransform,
                                const basegfx::B2DPolyPolygon& rDevicePolyPoly, bool bFill);

    static void DrawPolyPolygon(SalGraphics& rGraphics, const tools::PolyPolygon& rDevicePolyPoly,
                                bool bFill);

    static void DrawPolyPolygon(SalGraphics& rGraphics, const tools::PolyPolygon& rPolyPoly,
                                const tools::PolyPolygon* pClipPolyPoly = nullptr);

    static void DrawPolyPolygonGeometry(SalGraphics& rGraphics,
                                        const tools::PolyPolygon& rPolyPolygon);

    static void Invert(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                       InvertFlags nFlags);

    static void Invert(SalGraphics& rGraphics, const tools::Polygon& rDevicePoly,
                       InvertFlags nFlags);

    static void DrawGrid(SalGraphics& rGraphics, const tools::Rectangle& rDeviceRect,
                         const tools::Rectangle& rDeviceDstRect, const Size& rDeviceDist,
                         DrawGridFlags nFlags);

    static void DrawGridOfCrosses(SalGraphics& rGraphics, const tools::Rectangle& rDeviceGridArea,
                                  const Size& rDeviceGridDistance,
                                  const tools::Rectangle& rDeviceDrawingArea);

    static void DrawTextDecoration(SalGraphics& rGraphics,
                                   const vcl::text::RotatedGeometry& rDeviceGeo);

    static void DrawTextLines(SalGraphics& rGraphics,
                              std::span<const vcl::text::RotatedGeometry> rSegments,
                              const Color& rColor);

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

    /**
     * Attempts to draw a gradient natively via SalGraphics.
     * @return true if the hardware handled the gradient, false if fallback is needed.
     */
    static bool DrawGradient(SalGraphics& rGraphics, const tools::PolyPolygon& rDevicePolyPoly,
                             const Gradient& rGradient);

    /**
     * Composes a gradient by decomposing it into individual color-step polygons.
     * This is the software fallback for linear and axial gradients.
     */
    static void DrawGradient(SalGraphics& rGraphics, const tools::Rectangle& rRect,
                             const Gradient& rGradient, tools::Long nStepCount,
                             bool bAvoidVectorOverdraw,
                             const tools::PolyPolygon* pClipPolyPoly = nullptr);

private:
    static void DrawSinglePolygon(SalGraphics& rGraphics, const tools::Polygon& rPoly);

    static void DrawMultiplePolygons(SalGraphics& rGraphics, const tools::PolyPolygon& rPolyPoly);

    static sal_uInt8 GetGradientColorValue(tools::Long nValue);

    /**
     * Composes complex gradient styles (Radial, Elliptical, Square) by decomposing
     * them into nested shrinking polygons.
     * @param rGraphics      The hardware graphics device.
     * @param rRect          The destination rectangle in device pixels.
     * @param rGradient      The logical gradient description.
     * @param nStepCount     The pre-calculated number of steps (respects Printer polymorphism).
     * @param pClipPolyPoly  Optional complex clipping mask for software intersection.
     */
    static void DrawComplexGradient(SalGraphics& rGraphics, const tools::Rectangle& rRect,
                                    const Gradient& rGradient, tools::Long nStepCount,
                                    bool bAvoidVectorOverdraw,
                                    const tools::PolyPolygon* pClipPolyPoly = nullptr);
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
