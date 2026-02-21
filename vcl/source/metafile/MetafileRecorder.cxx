/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/poly.hxx>
#include <tools/stream.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>

#include <vcl/gradient.hxx>
#include <vcl/hatch.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>

#include <cmath>

namespace vcl
{
MetafileRecorder::ScopedSwitch::~ScopedSwitch() { mrRecorder.SetConnectMetaFile(mpOldMetaFile); }

MetafileRecorder::MetafileRecorder()
    : mpMetaFile(nullptr)
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

void MetafileRecorder::RecordComment(const rtl::OString& rComment, sal_uInt32 nVal,
                                     const sal_uInt8* pData)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaCommentAction(rComment, nVal, pData));
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

void MetafileRecorder::RecordPolyLine(const tools::Polygon& rPoly)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPolyLineAction(rPoly));
}

void MetafileRecorder::RecordPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaPolyLineAction(rPoly, rLineInfo));
}

void MetafileRecorder::RecordRect(const tools::Rectangle& rRect)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaRectAction(rRect));
}

void MetafileRecorder::RecordRoundRect(const tools::Rectangle& rRect, sal_uLong nHorzRound,
                                       sal_uLong nVertRound)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaRoundRectAction(rRect, nHorzRound, nVertRound));
}

void MetafileRecorder::RecordTransparent(const tools::PolyPolygon& rPolyPoly,
                                         sal_uInt16 nTransparencePercent)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTransparentAction(rPolyPoly, nTransparencePercent));
}

void MetafileRecorder::RecordTransparent(const basegfx::B2DHomMatrix& rObjectTransform,
                                         const basegfx::B2DPolyPolygon& rB2DPolyPoly,
                                         double fTransparency)
{
    if (IsActive())
    {
        // tdf#119843 need transformed Polygon here
        basegfx::B2DPolyPolygon aB2DPolyPoly(rB2DPolyPoly);
        aB2DPolyPoly.transform(rObjectTransform);
        mpMetaFile->AddAction(new MetaTransparentAction(
            tools::PolyPolygon(aB2DPolyPoly), static_cast<sal_uInt16>(fTransparency * 100.0)));
    }
}

void MetafileRecorder::RecordFloatTransparent(const GDIMetaFile& rMtf, const Point& rPos,
                                              const Size& rSize,
                                              const Gradient& rTransparenceGradient)
{
    if (IsActive())
    {
        // missing here is to map the data using the DeviceTransformation
        mpMetaFile->AddAction(
            new MetaFloatTransparentAction(rMtf, rPos, rSize, rTransparenceGradient));
    }
}

void MetafileRecorder::RecordWallpaper(const tools::Rectangle& rRect, const Wallpaper& rWallpaper)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaWallpaperAction(rRect, rWallpaper));
}

void MetafileRecorder::RecordLayoutMode(vcl::text::ComplexTextLayoutFlags nLayoutMode)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaLayoutModeAction(nLayoutMode));
}

void MetafileRecorder::RecordTextLanguage(LanguageType eTextLanguage)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextLanguageAction(eTextLanguage));
}

void MetafileRecorder::RecordDrawText(const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                                      sal_Int32 nLen)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextAction(rStartPt, rStr, nIndex, nLen));
}

void MetafileRecorder::RecordDrawTextArray(const Point& rStartPt, const OUString& rStr,
                                           KernArraySpan aKernArray,
                                           std::span<const sal_Bool> pKashidaAry, sal_Int32 nIndex,
                                           sal_Int32 nLen)
{
    if (IsActive())
        mpMetaFile->AddAction(
            new MetaTextArrayAction(rStartPt, rStr, aKernArray, pKashidaAry, nIndex, nLen));
}

void MetafileRecorder::RecordDrawPartialTextArray(const Point& rStartPt, const OUString& rStr,
                                                  KernArraySpan aKernArray,
                                                  std::span<const sal_Bool> pKashidaAry,
                                                  sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                                  sal_Int32 nIndex, sal_Int32 nLen)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextArrayAction(rStartPt, rStr, aKernArray, pKashidaAry,
                                                      nPartIndex, nPartLen, nIndex, nLen));
}

void MetafileRecorder::RecordDrawStretchText(const Point& rStartPt, sal_Int32 nWidth,
                                             const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaStretchTextAction(rStartPt, nWidth, rStr, nIndex, nLen));
}

void MetafileRecorder::RecordDrawTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                          DrawTextFlags nStyle)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextRectAction(rRect, rStr, nStyle));
}

std::unique_ptr<vcl::ScopedMetaGroup> MetafileRecorder::CreateScopedGroup(const OString& rName)
{
    if (IsActive())
        return std::make_unique<vcl::ScopedMetaGroup>(mpMetaFile, rName);
    return nullptr;
}

void MetafileRecorder::RecordBitmapAction(MetaActionType nAction, const Point& rDestPt,
                                          const Size& rDestSize, const Point& rSrcPt,
                                          const Size& rSrcSize, const Bitmap& rBitmap)
{
    if (!IsActive())
        return;

    switch (nAction)
    {
        case MetaActionType::BMP:
            mpMetaFile->AddAction(new MetaBmpAction(rDestPt, rBitmap));
            break;
        case MetaActionType::BMPSCALE:
            mpMetaFile->AddAction(new MetaBmpScaleAction(rDestPt, rDestSize, rBitmap));
            break;
        case MetaActionType::BMPSCALEPART:
            mpMetaFile->AddAction(
                new MetaBmpScalePartAction(rDestPt, rDestSize, rSrcPt, rSrcSize, rBitmap));
            break;
        case MetaActionType::BMPEX:
            mpMetaFile->AddAction(new MetaBmpExAction(rDestPt, rBitmap));
            break;
        case MetaActionType::BMPEXSCALE:
            mpMetaFile->AddAction(new MetaBmpExScaleAction(rDestPt, rDestSize, rBitmap));
            break;
        case MetaActionType::BMPEXSCALEPART:
            mpMetaFile->AddAction(
                new MetaBmpExScalePartAction(rDestPt, rDestSize, rSrcPt, rSrcSize, rBitmap));
            break;
        default:
            break;
    }
}

void MetafileRecorder::RecordTextLineColor(const Color& rColor, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaTextLineColorAction(rColor, bSet));
}

void MetafileRecorder::RecordOverlineColor(const Color& rColor, bool bSet)
{
    if (IsActive())
        mpMetaFile->AddAction(new MetaOverlineColorAction(rColor, bSet));
}

void MetafileRecorder::RecordTextLine(const Point& rPos, long nWidth, FontStrikeout eStrikeout,
                                      FontLineStyle eUnderline, FontLineStyle eOverline)
{
    if (IsActive())
        mpMetaFile->AddAction(
            new MetaTextLineAction(rPos, nWidth, eStrikeout, eUnderline, eOverline));
}

MetafileRecorder::ScopedSuspend::ScopedSuspend(MetafileRecorder& rRecorder)
    : mrRecorder(rRecorder)
    , mpOldMetaFile(rRecorder.GetConnectMetaFile())
{
    if (mpOldMetaFile)
        mrRecorder.SetConnectMetaFile(nullptr);
}

MetafileRecorder::ScopedSuspend::~ScopedSuspend()
{
    if (mpOldMetaFile)
        mrRecorder.SetConnectMetaFile(mpOldMetaFile);
}

MetafileRecorder::ScopedSwitch::ScopedSwitch(MetafileRecorder& rRecorder, GDIMetaFile* pNewMetaFile)
    : mrRecorder(rRecorder)
    , mpOldMetaFile(rRecorder.GetConnectMetaFile())
{
    // Always switch, even if pNewMetaFile is null (though ScopedSuspend is preferred for that)
    mrRecorder.SetConnectMetaFile(pNewMetaFile);
}

void vcl::MetafileRecorder::RecordB2DPolyLine(const basegfx::B2DPolygon& rB2D,
                                              const vcl::rendercontext::StrokeAttributes& rStroke,
                                              const basegfx::B2DHomMatrix& rObjectTransform,
                                              double fTransparency)
{
    (void)rObjectTransform; // Suppress -Werror for unused parameter
    SvMemoryStream aStream;
    aStream.WriteUInt16(1); // Format Version
    aStream.WriteDouble(rStroke.fWidth);
    aStream.WriteUInt16(static_cast<sal_uInt16>(rStroke.eJoin));
    aStream.WriteUInt16(static_cast<sal_uInt16>(rStroke.eCap));
    aStream.WriteDouble(rStroke.fMiterMinimumAngle);
    aStream.WriteDouble(fTransparency);

    sal_uInt32 nStrokeCount = rStroke.pDashArray ? rStroke.pDashArray->size() : 0;
    aStream.WriteUInt32(nStrokeCount);
    if (rStroke.pDashArray)
    {
        for (double fVal : *rStroke.pDashArray)
            aStream.WriteDouble(fVal);
    }

    RecordComment("XB2DPOLYLINE_SEQ_BEGIN", static_cast<sal_uInt32>(aStream.Tell()),
                  reinterpret_cast<const sal_uInt8*>(aStream.GetData()));

    LineInfo aLineInfo;
    if (rStroke.fWidth != 0.0)
        aLineInfo.SetWidth(std::round(rStroke.fWidth));
    aLineInfo.SetLineJoin(rStroke.eJoin);
    aLineInfo.SetLineCap(rStroke.eCap);
    RecordPolyLine(tools::Polygon(rB2D), aLineInfo);

    RecordComment("XB2DPOLYLINE_SEQ_END", 0, nullptr);
}
} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
