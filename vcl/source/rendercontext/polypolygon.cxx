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

#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>

#include <vcl/virdev.hxx>

#include <salgdi.hxx>

#define OUTDEV_POLYPOLY_STACKBUF 32

void RenderContext2::ImplDrawPolyPolygonWithB2DPolyPolygon(
    const basegfx::B2DPolyPolygon& rB2DPolyPoly)
{
    // Do not paint empty PolyPolygons
    if (!rB2DPolyPoly.count() || !IsDeviceOutputNecessary())
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

    if (mbInitFillColor)
        InitFillColor();

    bool bSuccess(false);

    if (mpGraphics->supportsOperation(OutDevSupportType::B2DDraw)
        && RasterOp::OverPaint == GetRasterOp() && (IsLineColor() || IsFillColor()))
    {
        const basegfx::B2DHomMatrix aTransform(ImplGetDeviceTransformation());
        basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);
        bSuccess = true;

        // ensure closed - maybe assert, hinders buffering
        if (!aB2DPolyPolygon.isClosed())
            aB2DPolyPolygon.setClosed(true);

        if (IsFillColor())
            bSuccess = mpGraphics->DrawPolyPolygon(aTransform, aB2DPolyPolygon, 0.0, *this);

        if (bSuccess && IsLineColor())
        {
            const bool bPixelSnapHairline(mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

            for (auto const& rPolygon : aB2DPolyPolygon)
            {
                bSuccess = mpGraphics->DrawPolyLine(
                    aTransform, rPolygon, 0.0,
                    0.0, // tdf#124848 hairline
                    nullptr, // MM01
                    basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                    basegfx::deg2rad(
                        15.0), // not used with B2DLineJoin::NONE, but the correct default
                    bPixelSnapHairline, *this);

                if (!bSuccess)
                    break;
            }
        }
    }

    if (!bSuccess)
    {
        // fallback to old polygon drawing if needed
        const tools::PolyPolygon aToolsPolyPolygon(rB2DPolyPoly);
        const tools::PolyPolygon aPixelPolyPolygon = ImplLogicToDevicePixel(aToolsPolyPolygon);
        ImplDrawPolyPolygon(aPixelPolyPolygon.Count(), aPixelPolyPolygon);
    }

    if (mpAlphaVDev)
        mpAlphaVDev->ImplDrawPolyPolygonWithB2DPolyPolygon(rB2DPolyPoly);
}

// #100127# Extracted from RenderContext2::DrawPolyPolygon()
void RenderContext2::ImplDrawPolyPolygon(sal_uInt16 nPoly, const tools::PolyPolygon& rPolyPoly)
{
    // AW: This crashes on empty PolyPolygons, avoid that
    if (!nPoly)
        return;

    sal_uInt32 aStackAry1[OUTDEV_POLYPOLY_STACKBUF];
    const Point* aStackAry2[OUTDEV_POLYPOLY_STACKBUF];
    PolyFlags* aStackAry3[OUTDEV_POLYPOLY_STACKBUF];
    sal_uInt32* pPointAry;
    const Point** pPointAryAry;
    const PolyFlags** pFlagAryAry;
    sal_uInt16 i = 0;
    sal_uInt16 j = 0;
    sal_uInt16 last = 0;
    bool bHaveBezier = false;

    if (nPoly > OUTDEV_POLYPOLY_STACKBUF)
    {
        pPointAry = new sal_uInt32[nPoly];
        pPointAryAry = new const Point*[nPoly];
        pFlagAryAry = new const PolyFlags*[nPoly];
    }
    else
    {
        pPointAry = aStackAry1;
        pPointAryAry = aStackAry2;
        pFlagAryAry = const_cast<const PolyFlags**>(aStackAry3);
    }

    do
    {
        const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
        sal_uInt16 nSize = rPoly.GetSize();
        if (nSize)
        {
            pPointAry[j] = nSize;
            pPointAryAry[j] = rPoly.GetConstPointAry();
            pFlagAryAry[j] = rPoly.GetConstFlagAry();
            last = i;

            if (pFlagAryAry[j])
                bHaveBezier = true;

            ++j;
        }
        ++i;
    } while (i < nPoly);

    if (j == 1)
    {
        // #100127# Forward beziers to sal, if any
        if (bHaveBezier)
        {
            if (!mpGraphics->DrawPolygonBezier(*pPointAry, *pPointAryAry, *pFlagAryAry, *this))
            {
                tools::Polygon aPoly = tools::Polygon::SubdivideBezier(rPolyPoly.GetObject(last));
                mpGraphics->DrawPolygon(aPoly.GetSize(), aPoly.GetConstPointAry(), *this);
            }
        }
        else
        {
            mpGraphics->DrawPolygon(*pPointAry, *pPointAryAry, *this);
        }
    }
    else
    {
        // #100127# Forward beziers to sal, if any
        if (bHaveBezier)
        {
            if (!mpGraphics->DrawPolyPolygonBezier(j, pPointAry, pPointAryAry, pFlagAryAry, *this))
            {
                tools::PolyPolygon aPolyPoly = tools::PolyPolygon::SubdivideBezier(rPolyPoly);
                ImplDrawPolyPolygon(aPolyPoly.Count(), aPolyPoly);
            }
        }
        else
        {
            mpGraphics->DrawPolyPolygon(j, pPointAry, pPointAryAry, *this);
        }
    }

    if (pPointAry != aStackAry1)
    {
        delete[] pPointAry;
        delete[] pPointAryAry;
        delete[] pFlagAryAry;
    }
}

void RenderContext2::ImplDrawPolyPolygon(tools::PolyPolygon const& rPolyPoly,
                                         tools::PolyPolygon const* pClipPolyPoly)
{
    tools::PolyPolygon* pPolyPoly;

    if (pClipPolyPoly)
    {
        pPolyPoly = new tools::PolyPolygon;
        rPolyPoly.GetIntersection(*pClipPolyPoly, *pPolyPoly);
    }
    else
    {
        pPolyPoly = const_cast<tools::PolyPolygon*>(&rPolyPoly);
    }

    if (pPolyPoly->Count() == 1)
    {
        const tools::Polygon& rPoly = pPolyPoly->GetObject(0);
        sal_uInt16 nSize = rPoly.GetSize();

        if (nSize >= 2)
        {
            const Point* pPtAry = rPoly.GetConstPointAry();
            mpGraphics->DrawPolygon(nSize, pPtAry, *this);
        }
    }
    else if (pPolyPoly->Count())
    {
        sal_uInt16 nCount = pPolyPoly->Count();
        std::unique_ptr<sal_uInt32[]> pPointAry(new sal_uInt32[nCount]);
        std::unique_ptr<const Point* []> pPointAryAry(new const Point*[nCount]);
        sal_uInt16 i = 0;

        do
        {
            const tools::Polygon& rPoly = pPolyPoly->GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();
            if (nSize)
            {
                pPointAry[i] = nSize;
                pPointAryAry[i] = rPoly.GetConstPointAry();
                i++;
            }
            else
                nCount--;
        } while (i < nCount);

        if (nCount == 1)
            mpGraphics->DrawPolygon(pPointAry[0], pPointAryAry[0], *this);
        else
            mpGraphics->DrawPolyPolygon(nCount, pPointAry.get(), pPointAryAry.get(), *this);
    }

    if (pClipPolyPoly)
        delete pPolyPoly;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
