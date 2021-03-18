/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>

#include <vcl/virdev.hxx>

#include <salgdi.hxx>

void RenderContext2::DrawPolyLine(const tools::Polygon& rPoly)
{
    assert(!is_double_buffered_window());

    sal_uInt16 nPoints = rPoly.GetSize();

    if (!IsDeviceOutputNecessary() || !mbLineColor || (nPoints < 2))
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (mbInitLineColor)
        InitLineColor();

    // use b2dpolygon drawing if possible
    if (DrawPolyLineDirect(basegfx::B2DHomMatrix(), rPoly.getB2DPolygon()))
        return;

    const basegfx::B2DPolygon aB2DPolyLine(rPoly.getB2DPolygon());
    const basegfx::B2DHomMatrix aTransform(ImplGetDeviceTransformation());
    const bool bPixelSnapHairline(mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    bool bDrawn = mpGraphics->DrawPolyLine(
        aTransform, aB2DPolyLine, 0.0,
        0.0, // tdf#124848 hairline
        nullptr, // MM01
        basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
        basegfx::deg2rad(15.0) /*default fMiterMinimumAngle, not used*/, bPixelSnapHairline, *this);

    if (!bDrawn)
    {
        tools::Polygon aPoly = ImplLogicToDevicePixel(rPoly);
        Point* pPtAry = aPoly.GetPointAry();

        // #100127# Forward beziers to sal, if any
        if (aPoly.HasFlags())
        {
            const PolyFlags* pFlgAry = aPoly.GetConstFlagAry();
            if (!mpGraphics->DrawPolyLineBezier(nPoints, pPtAry, pFlgAry, *this))
            {
                aPoly = tools::Polygon::SubdivideBezier(aPoly);
                pPtAry = aPoly.GetPointAry();
                mpGraphics->DrawPolyLine(aPoly.GetSize(), pPtAry, *this);
            }
        }
        else
        {
            mpGraphics->DrawPolyLine(nPoints, pPtAry, *this);
        }
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawPolyLine(rPoly);
}

bool RenderContext2::DrawPolyLineDirect(const basegfx::B2DHomMatrix& rObjectTransform,
                                        const basegfx::B2DPolygon& rB2DPolygon, double fLineWidth,
                                        double fTransparency,
                                        const std::vector<double>* pStroke, // MM01
                                        basegfx::B2DLineJoin eLineJoin,
                                        css::drawing::LineCap eLineCap, double fMiterMinimumAngle)
{
    assert(!is_double_buffered_window());

    // AW: Do NOT paint empty PolyPolygons
    if (!rB2DPolygon.count())
        return true;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return true;

    if (mbInitLineColor)
        InitLineColor();

    const bool bTryB2d(mpGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                       && RasterOp::OverPaint == GetRasterOp() && IsLineColor());

    if (bTryB2d)
    {
        // combine rObjectTransform with WorldToDevice
        const basegfx::B2DHomMatrix aTransform(ImplGetDeviceTransformation() * rObjectTransform);
        const bool bPixelSnapHairline((mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
                                      && rB2DPolygon.count() < 1000);

        const double fAdjustedTransparency = mpAlphaVDev ? 0 : fTransparency;
        // draw the polyline
        bool bDrawSuccess = mpGraphics->DrawPolyLine(
            aTransform, rB2DPolygon, fAdjustedTransparency,
            fLineWidth, // tdf#124848 use LineWidth direct, do not try to solve for zero-case (aka hairline)
            pStroke, // MM01
            eLineJoin, eLineCap, fMiterMinimumAngle, bPixelSnapHairline, *this);

        if (bDrawSuccess)
        {
            if (mpAlphaVDev)
                mpAlphaVDev->DrawPolyLineDirect(rObjectTransform, rB2DPolygon, fLineWidth,
                                                fTransparency, pStroke, eLineJoin, eLineCap,
                                                fMiterMinimumAngle);

            return true;
        }
    }
    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
