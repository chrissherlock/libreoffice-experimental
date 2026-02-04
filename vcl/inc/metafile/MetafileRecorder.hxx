#include <vcl/vclenum.hxx>
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <tools/solar.h>

#include <vcl/dllapi.h>
#include <vcl/bitmap.hxx>
#include <vcl/region.hxx>
#include <vcl/rendercontext/State.hxx>

namespace basegfx
{
class B2DHomMatrix;
class B2DPolyPolygon;
}

class Wallpaper;
class LineInfo;
class OutputDevice;
class GDIMetaFile;
class GfxLink;
class Point;
class Size;
class Gradient;
class Hatch;
namespace tools
{
class PolyPolygon;
}
namespace rtl
{
class OString;
}

namespace vcl
{
// Facade class for recording high-level OutputDevice operations
// into a GDIMetaFile. Hides the complexity of MetaAction construction.
class MetafileRecorder
{
private:
    GDIMetaFile* mpMetaFile;
    OutputDevice& mrOutDev;

public:
    explicit MetafileRecorder(OutputDevice& rDev);

    bool IsActive() const;

    // --- Bitmap Actions (MetaBmpAction) ---
    void RecordBitmap(const Point& rPos, const Bitmap& rBitmap);
    void RecordBitmapScale(const Point& rPos, const Size& rSz, const Bitmap& rBitmap);
    void RecordBitmapScalePart(const Point& rDestPos, const Size& rDestSz, const Point& rSrcPos,
                               const Size& rSrcSz, const Bitmap& rBitmap);

    // --- Bitmap with Alpha Actions (MetaBmpExAction) ---
    // Note: Takes Bitmap, as BitmapEx was removed/merged.
    void RecordBitmapEx(const Point& rPos, const Bitmap& rBitmap);
    void RecordBitmapExScale(const Point& rPos, const Size& rSz, const Bitmap& rBitmap);
    void RecordBitmapExScalePart(const Point& rDestPos, const Size& rDestSz, const Point& rSrcPos,
                                 const Size& rSrcSz, const Bitmap& rBitmap);

    // --- Clipping Actions ---
    void RecordClipRegion(const vcl::Region& rRegion, bool bClip);
    void RecordMoveClipRegion(long nHorzMove, long nVertMove);
    void RecordIntersectClipRegion(const tools::Rectangle& rRect);
    void RecordIntersectClipRegion(const vcl::Region& rRegion);

    // --- Geometric Shape Actions ---
    void RecordEllipse(const tools::Rectangle& rRect);
    void RecordArc(const tools::Rectangle& rRect, const Point& rStartPt, const Point& rEndPt);
    void RecordPie(const tools::Rectangle& rRect, const Point& rStartPt, const Point& rEndPt);
    void RecordChord(const tools::Rectangle& rRect, const Point& rStartPt, const Point& rEndPt);

    // --- EPS Actions ---
    void RecordEPS(const Point& rPoint, const Size& rSize, const GfxLink& rGfxLink,
                   const GDIMetaFile& rSubst);

    // --- State Actions ---
    void RecordFillColor(const Color& rColor, bool bSet);
    void RecordLineColor(const Color& rColor, bool bSet);

    // --- State Stack Actions ---
    void RecordPush(vcl::PushFlags nFlags);
    void RecordPop();

    // --- Gradient Actions ---
    void RecordGradient(const tools::Rectangle& rRect, const Gradient& rGradient);
    void RecordGradient(const tools::PolyPolygon& rPolyPoly, const Gradient& rGradient);

    // --- Drawing Actions ---
    void RecordLine(const Point& rStart, const Point& rEnd);
    void RecordPolygon(const tools::Polygon& rPoly);
    void RecordPolyLine(const tools::Polygon& rPoly);
    void RecordRect(const tools::Rectangle& rRect);
    void RecordRoundRect(const tools::Rectangle& rRect, sal_uLong nHorzRound, sal_uLong nVertRound);

    void RecordPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo);

    void RecordPolyPolygon(const tools::PolyPolygon& rPolyPoly);

    void RecordPixel(const Point& rPt);
    void RecordPixel(const Point& rPt, const Color& rColor);

    void RecordMapMode(const MapMode& rMapMode);
    void RecordRefPoint(const Point& rRefPoint, bool bSet);
    void RecordRasterOp(RasterOp eRasterOp);

    void RecordLine(const Point& rStart, const Point& rEnd, const LineInfo& rLineInfo);
    void RecordHatch(const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch);
    void RecordFont(const vcl::Font& rFont);
    void RecordTextAlign(TextAlign eAlign);
    void RecordTextColor(const Color& rColor);
    void RecordTextFillColor(const Color& rColor, bool bSet);
    void RecordMask(const Point& rDestPt, const Bitmap& rBitmap, const Color& rMaskColor);
    void RecordMaskScale(const Point& rDestPt, const Size& rDestSize, const Bitmap& rBitmap,
                         const Color& rMaskColor);
    void RecordMaskScalePart(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                             const Size& rSrcSizePixel, const Bitmap& rBitmap,
                             const Color& rMaskColor);

    void RecordTransparent(const tools::PolyPolygon& rPolyPoly, sal_uInt16 nTransparencePercent);
    void RecordTransparent(const basegfx::B2DHomMatrix& rObjectTransform,
                           const basegfx::B2DPolyPolygon& rB2DPolyPoly, double fTransparency);
    void RecordWallpaper(const tools::Rectangle& rRect, const Wallpaper& rWallpaper);
    void RecordFloatTransparent(const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                                const Gradient& rTransparenceGradient);

    // --- Comment Actions ---
    void RecordComment(const rtl::OString& rComment);
};

} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
