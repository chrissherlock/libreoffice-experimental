/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/outdev.hxx>

namespace vcl
{
MetafileRecorder::MetafileRecorder(OutputDevice& rDev)
    : mpMetaFile(rDev.GetConnectMetaFile())
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
} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
