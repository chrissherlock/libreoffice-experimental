#include <vcl/lineinfo.hxx>
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/poly.hxx>

#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>

#include <metafile/MetafileRecorder.hxx>

namespace vcl
{
MetafileRecorder::MetafileRecorder(OutputDevice& rDev)
    : mpMetaFile(rDev.GetConnectMetaFile())
    , mrOutDev(rDev)
{
}

bool MetafileRecorder::IsActive() const { return mpMetaFile != nullptr; }

void MetafileRecorder::RecordBitmap(const Point& rPos, const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaBmpAction(rPos, rBitmap));
}

void MetafileRecorder::RecordBitmapScale(const Point& rPos, const Size& rSz, const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaBmpScaleAction(rPos, rSz, rBitmap));
}

void MetafileRecorder::RecordBitmapScalePart(const Point& rDestPos, const Size& rDestSz,
                                             const Point& rSrcPos, const Size& rSrcSz,
                                             const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(
            new MetaBmpScalePartAction(rDestPos, rDestSz, rSrcPos, rSrcSz, rBitmap));
}

void MetafileRecorder::RecordBitmapEx(const Point& rPos, const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaBmpExAction(rPos, rBitmap));
}

void MetafileRecorder::RecordBitmapExScale(const Point& rPos, const Size& rSz,
                                           const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaBmpExScaleAction(rPos, rSz, rBitmap));
}

void MetafileRecorder::RecordBitmapExScalePart(const Point& rDestPos, const Size& rDestSz,
                                               const Point& rSrcPos, const Size& rSrcSz,
                                               const Bitmap& rBitmap)
{
    if (IsActive())
        mpMetaFile->AddAction(
            new MetaBmpExScalePartAction(rDestPos, rDestSz, rSrcPos, rSrcSz, rBitmap));
}

void MetafileRecorder::RecordClipRegion(const vcl::Region& rRegion, bool bClip)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaClipRegionAction(rRegion, bClip));
}

void MetafileRecorder::RecordMoveClipRegion(long nHorzMove, long nVertMove)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaMoveClipRegionAction(nHorzMove, nVertMove));
}

void MetafileRecorder::RecordIntersectClipRegion(const tools::Rectangle& rRect)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaISectRectClipRegionAction(rRect));
}

void MetafileRecorder::RecordIntersectClipRegion(const vcl::Region& rRegion)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaISectRegionClipRegionAction(rRegion));
}

void MetafileRecorder::RecordEllipse(const tools::Rectangle& rRect)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaEllipseAction(rRect));
}

void MetafileRecorder::RecordArc(const tools::Rectangle& rRect, const Point& rStartPt,
                                 const Point& rEndPt)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaArcAction(rRect, rStartPt, rEndPt));
}

void MetafileRecorder::RecordPie(const tools::Rectangle& rRect, const Point& rStartPt,
                                 const Point& rEndPt)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPieAction(rRect, rStartPt, rEndPt));
}

void MetafileRecorder::RecordChord(const tools::Rectangle& rRect, const Point& rStartPt,
                                   const Point& rEndPt)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaChordAction(rRect, rStartPt, rEndPt));
}

void MetafileRecorder::RecordEPS(const Point& rPoint, const Size& rSize, const GfxLink& rGfxLink,
                                 const GDIMetaFile& rSubst)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaEPSAction(rPoint, rSize, rGfxLink, rSubst));
}

void MetafileRecorder::RecordFillColor(const Color& rColor, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaFillColorAction(rColor, bSet));
}

void MetafileRecorder::RecordLineColor(const Color& rColor, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaLineColorAction(rColor, bSet));
}

void MetafileRecorder::RecordPush(vcl::PushFlags nFlags)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPushAction(nFlags));
}

void MetafileRecorder::RecordPop()
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPopAction());
}

void MetafileRecorder::RecordGradient(const tools::Rectangle& rRect, const Gradient& rGradient)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaGradientAction(rRect, rGradient));
}

void MetafileRecorder::RecordGradient(const tools::PolyPolygon& rPolyPoly,
                                      const Gradient& rGradient)
{
    if (!IsActive())
        return;

    if (!rPolyPoly.Count() || !rPolyPoly[0].GetSize())
        return;

    tools::Rectangle aBoundRect(rPolyPoly.GetBoundRect());
    if (aBoundRect.IsEmpty())
        return;

    Gradient aGradient(rGradient);
    if (mrOutDev.GetDrawMode() & DrawModeFlags::GrayGradient)
        aGradient.MakeGrayscale();

    if (rPolyPoly.IsRect())
    {
        mpMetaFile->AddAction(new MetaGradientAction(aBoundRect, aGradient));
    }
    else
    {
        // Complex Gradient "Sandwich"
        // 1. Start Tag
        ScopedMetaGroup aGroup(mpMetaFile, "XGRAD_SEQ_BEGIN"_ostr, "XGRAD_SEQ_END"_ostr);

        // 2. The Modern Action
        mpMetaFile->AddAction(new MetaGradientExAction(rPolyPoly, aGradient));

        // 3. The Fallback
        mpMetaFile->AddAction(new MetaPushAction(vcl::PushFlags::CLIPREGION));

        // UNIFIED BEHAVIOR:
        // Always use IntersectClipRegion. This is safe for both Screen and Printer.
        // It respects existing clips (like Printer margins) while applying the new gradient shape.
        mpMetaFile->AddAction(new MetaISectRegionClipRegionAction(vcl::Region(rPolyPoly)));

        // GRADIENT(Rect) - The bounding box
        mpMetaFile->AddAction(new MetaGradientAction(aBoundRect, aGradient));

        // POP()
        mpMetaFile->AddAction(new MetaPopAction());
    }
}

void MetafileRecorder::RecordComment(const rtl::OString& rComment)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaCommentAction(rComment));
}

void MetafileRecorder::RecordLine(const Point& rStart, const Point& rEnd)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaLineAction(rStart, rEnd));
}

void MetafileRecorder::RecordHatch(const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaHatchAction(rPolyPoly, rHatch));
}

void MetafileRecorder::RecordLine(const Point& rStart, const Point& rEnd, const LineInfo& rLineInfo)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaLineAction(rStart, rEnd, rLineInfo));
}

void MetafileRecorder::RecordFont(const vcl::Font& rFont)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaFontAction(rFont));
}

void MetafileRecorder::RecordTextAlign(TextAlign eAlign)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextAlignAction(eAlign));
}

void MetafileRecorder::RecordTextColor(const Color& rColor)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextColorAction(rColor));
}

void MetafileRecorder::RecordTextFillColor(const Color& rColor, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextFillColorAction(rColor, bSet));
}

void MetafileRecorder::RecordMapMode(const MapMode& rMapMode)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaMapModeAction(rMapMode));
}

void MetafileRecorder::RecordMask(const Point& rDestPt, const Bitmap& rBitmap,
                                  const Color& rMaskColor)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaMaskAction(rDestPt, rBitmap, rMaskColor));
}

void MetafileRecorder::RecordMaskScale(const Point& rDestPt, const Size& rDestSize,
                                       const Bitmap& rBitmap, const Color& rMaskColor)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaMaskScaleAction(rDestPt, rDestSize, rBitmap, rMaskColor));
}

void MetafileRecorder::RecordMaskScalePart(const Point& rDestPt, const Size& rDestSize,
                                           const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                           const Bitmap& rBitmap, const Color& rMaskColor)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaMaskScalePartAction(rDestPt, rDestSize, rSrcPtPixel,
                                                          rSrcSizePixel, rBitmap, rMaskColor));
}

void MetafileRecorder::RecordRefPoint(const Point& rRefPoint, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaRefPointAction(rRefPoint, bSet));
}

void MetafileRecorder::RecordRasterOp(RasterOp eRasterOp)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaRasterOpAction(eRasterOp));
}

void MetafileRecorder::RecordPixel(const Point& rPt)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPointAction(rPt));
}

void MetafileRecorder::RecordPixel(const Point& rPt, const Color& rColor)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPixelAction(rPt, rColor));
}

void MetafileRecorder::RecordPolygon(const tools::Polygon& rPoly)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPolygonAction(rPoly));
}

void MetafileRecorder::RecordPolyPolygon(const tools::PolyPolygon& rPolyPoly)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPolyPolygonAction(rPolyPoly));
}

} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
