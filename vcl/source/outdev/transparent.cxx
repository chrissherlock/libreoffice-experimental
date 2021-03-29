/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>

#include <polygontools.hxx>

#include <cassert>

void OutputDevice::DrawTransparent(basegfx::B2DHomMatrix const& rObjectTransform,
                                   basegfx::B2DPolyPolygon const& rB2DPolyPoly,
                                   double fTransparency)
{
    assert(!is_double_buffered_window());

    if (mpMetaFile)
    {
        if (GetRasterOp() == RasterOp::OverPaint)
        {
            // tdf#119843 need transformed Polygon here
            basegfx::B2DPolyPolygon aB2DPolyPoly(rB2DPolyPoly);
            aB2DPolyPoly.transform(rObjectTransform);
            mpMetaFile->AddAction(
                new MetaTransparentAction(tools::PolyPolygon(aB2DPolyPoly),
                                          static_cast<sal_uInt16>(fTransparency * 100.0)));
        }
        else
        {
            // fallback to old polygon drawing if needed
            // tdf#119843 need transformed Polygon here
            basegfx::B2DPolyPolygon aB2DPolyPoly(rB2DPolyPoly);
            aB2DPolyPoly.transform(rObjectTransform);
            DrawTransparent(toPolyPolygon(aB2DPolyPoly), static_cast<sal_uInt16>(fTransparency * 100.0));

            return;
        }
    }

    RenderContext2::DrawTransparent(rObjectTransform, rB2DPolyPoly, fTransparency);
}

void OutputDevice::DrawTransparent(tools::PolyPolygon const& rPolyPoly,
                                   sal_uInt16 nTransparencePercent)
{
    assert(!is_double_buffered_window());

    // short circuit for drawing an opaque polygon
    if ((nTransparencePercent < 1) || (mnDrawMode & DrawModeFlags::NoTransparency))
    {
        mpMetaFile->AddAction(new MetaPolyPolygonAction(rPolyPoly));
        return;
    }

    // short circuit for drawing an invisible polygon
    if (!mbFillColor || (nTransparencePercent >= 100))
    {
        // short circuit if the polygon border is invisible too
        if (!mbLineColor)
            return;

        // we assume that the border is NOT to be drawn transparently???
        if (mpMetaFile)
        {
            mpMetaFile->AddAction(new MetaPushAction(PushFlags::FILLCOLOR));
            mpMetaFile->AddAction(new MetaFillColorAction(Color(), false));
            mpMetaFile->AddAction(new MetaPolyPolygonAction(rPolyPoly));
            mpMetaFile->AddAction(new MetaPopAction());
        }

        return; // tdf#84294: do not record it in metafile
    }

    // handle metafile recording
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTransparentAction(rPolyPoly, nTransparencePercent));

    RenderContext2::DrawTransparent(rPolyPoly, nTransparencePercent);
}

void OutputDevice::DrawTransparent(const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                                   const Gradient& rTransparenceGradient)
{
    assert(!is_double_buffered_window());

    const Color aBlack(COL_BLACK);

    if (mpMetaFile)
    {
        // missing here is to map the data using the DeviceTransformation
        mpMetaFile->AddAction(
            new MetaFloatTransparentAction(rMtf, rPos, rSize, rTransparenceGradient));
    }

    if (!IsDeviceOutputNecessary())
        return;

    if ((rTransparenceGradient.GetStartColor() == aBlack
         && rTransparenceGradient.GetEndColor() == aBlack)
        || (mnDrawMode & DrawModeFlags::NoTransparency))
    {
        const_cast<GDIMetaFile&>(rMtf).WindStart();
        const_cast<GDIMetaFile&>(rMtf).Play(this, rPos, rSize);
        const_cast<GDIMetaFile&>(rMtf).WindStart();
    }
    else
    {
        GDIMetaFile* pOldMetaFile = mpMetaFile;
        tools::Rectangle aOutRect(LogicToPixel(rPos), LogicToPixel(rSize));
        Point aPoint;
        tools::Rectangle aDstRect(aPoint, GetOutputSizePixel());

        mpMetaFile = nullptr;
        aDstRect.Intersection(aOutRect);

        ClipToPaintRegion(aDstRect);

        if (!aDstRect.IsEmpty())
        {
            // Create transparent buffer
            ScopedVclPtrInstance<VirtualDevice> xVDev(DeviceFormat::DEFAULT, DeviceFormat::DEFAULT);

            xVDev->SetDPIX(GetDPIX());
            xVDev->SetDPIY(GetDPIY());

            if (xVDev->SetOutputSizePixel(aDstRect.GetSize()))
            {
                if (GetAntialiasing() != AntialiasingFlags::NONE)
                {
                    // #i102109#
                    // For MetaFile replay (see task) it may now be necessary to take
                    // into account that the content is AntiAlialiased and needs to be masked
                    // like that. Instead of masking, i will use a copy-modify-paste cycle
                    // here (as i already use in the VclPrimiziveRenderer with success)
                    xVDev->SetAntialiasing(GetAntialiasing());

                    // create MapMode for buffer (offset needed) and set
                    MapMode aMap(GetMapMode());
                    const Point aOutPos(PixelToLogic(aDstRect.TopLeft()));
                    aMap.SetOrigin(Point(-aOutPos.X(), -aOutPos.Y()));
                    xVDev->SetMapMode(aMap);

                    // copy MapMode state and disable for target
                    const bool bOrigMapModeEnabled(IsMapModeEnabled());
                    EnableMapMode(false);

                    // copy MapMode state and disable for buffer
                    const bool bBufferMapModeEnabled(xVDev->IsMapModeEnabled());
                    xVDev->EnableMapMode(false);

                    // copy content from original to buffer
                    xVDev->DrawOutDev(aPoint, xVDev->GetOutputSizePixel(), // dest
                                      aDstRect.TopLeft(), xVDev->GetOutputSizePixel(), // source
                                      *this);

                    // draw MetaFile to buffer
                    xVDev->EnableMapMode(bBufferMapModeEnabled);
                    const_cast<GDIMetaFile&>(rMtf).WindStart();
                    const_cast<GDIMetaFile&>(rMtf).Play(xVDev.get(), rPos, rSize);
                    const_cast<GDIMetaFile&>(rMtf).WindStart();

                    // get content bitmap from buffer
                    xVDev->EnableMapMode(false);

                    const Bitmap aPaint(xVDev->GetBitmap(aPoint, xVDev->GetOutputSizePixel()));

                    // create alpha mask from gradient and get as Bitmap
                    xVDev->EnableMapMode(bBufferMapModeEnabled);
                    xVDev->SetDrawMode(DrawModeFlags::GrayGradient);
                    xVDev->DrawGradient(tools::Rectangle(rPos, rSize), rTransparenceGradient);
                    xVDev->SetDrawMode(DrawModeFlags::Default);
                    xVDev->EnableMapMode(false);

                    const AlphaMask aAlpha(xVDev->GetBitmap(aPoint, xVDev->GetOutputSizePixel()));

                    xVDev.disposeAndClear();

                    // draw masked content to target and restore MapMode
                    DrawBitmapEx(aDstRect.TopLeft(), BitmapEx(aPaint, aAlpha));
                    EnableMapMode(bOrigMapModeEnabled);
                }
                else
                {
                    MapMode aMap(GetMapMode());
                    Point aOutPos(PixelToLogic(aDstRect.TopLeft()));
                    const bool bOldMap = mbMap;

                    aMap.SetOrigin(Point(-aOutPos.X(), -aOutPos.Y()));
                    xVDev->SetMapMode(aMap);
                    const bool bVDevOldMap = xVDev->IsMapModeEnabled();

                    // create paint bitmap
                    const_cast<GDIMetaFile&>(rMtf).WindStart();
                    const_cast<GDIMetaFile&>(rMtf).Play(xVDev.get(), rPos, rSize);
                    const_cast<GDIMetaFile&>(rMtf).WindStart();
                    xVDev->EnableMapMode(false);
                    BitmapEx aPaint = xVDev->GetBitmapEx(Point(), xVDev->GetOutputSizePixel());
                    xVDev->EnableMapMode(
                        bVDevOldMap); // #i35331#: MUST NOT use EnableMapMode( sal_True ) here!

                    // create alpha mask from gradient
                    xVDev->SetDrawMode(DrawModeFlags::GrayGradient);
                    xVDev->DrawGradient(tools::Rectangle(rPos, rSize), rTransparenceGradient);
                    xVDev->SetDrawMode(DrawModeFlags::Default);
                    xVDev->EnableMapMode(false);

                    AlphaMask aAlpha(xVDev->GetBitmap(Point(), xVDev->GetOutputSizePixel()));
                    aAlpha.BlendWith(aPaint.GetAlpha());

                    xVDev.disposeAndClear();

                    EnableMapMode(false);
                    DrawBitmapEx(aDstRect.TopLeft(), BitmapEx(aPaint.GetBitmap(), aAlpha));
                    EnableMapMode(bOldMap);
                }
            }
        }

        mpMetaFile = pOldMetaFile;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
