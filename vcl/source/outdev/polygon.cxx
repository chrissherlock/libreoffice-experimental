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

#include <sal/types.h>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/poly.hxx>

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <memory>

constexpr sal_uInt16 OUTDEV_POLYPOLY_STACKBUF = 32;

void OutputDevice::DrawPolyPolygon( const tools::PolyPolygon& rPolyPoly )
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(rPolyPoly);

    const sal_uInt16 nPoly = rPolyPoly.Count();
    if (!nPoly || !PrepareGraphicsOutput() || !mpGraphics)
        return;

    // use b2dpolygon drawing if possible
    if (CanDrawPolygon())
    {
        const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation());
        basegfx::B2DPolyPolygon aB2DPolyPolygon(rPolyPoly.getB2DPolyPolygon());

        // ensure closed - may be asserted, will prevent buffering
        if(!aB2DPolyPolygon.isClosed())
            aB2DPolyPolygon.setClosed(true);

        if (IsFillColor())
        {
            mpGraphics->DrawPolyPolygon(
                aTransform,
                aB2DPolyPolygon,
                0.0,
                *this);
        }

        if (ImplDrawPolyPolygonOutlines(aTransform, aB2DPolyPolygon))
            return;
    }

    ImplDrawPolyPolygonFallback(nPoly, rPolyPoly);
}

bool OutputDevice::ImplDrawPolyPolygonOutlines(
    const basegfx::B2DHomMatrix& rTransform,
    const basegfx::B2DPolyPolygon& rB2DPolyPolygon,
    double fTransparency)
{
    if (!IsLineColor())
        return true;

    const bool bPixelSnapHairline(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    for (auto const& rPolygon : std::as_const(rB2DPolyPolygon))
    {
        bool bSuccess = mpGraphics->DrawPolyLine(
            rTransform,
            rPolygon,
            fTransparency,
            0.0, // tdf#124848 hairline
            nullptr, // MM01
            basegfx::B2DLineJoin::NONE,
            css::drawing::LineCap_BUTT,
            basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
            bPixelSnapHairline,
            *this);

        if (!bSuccess)
            return false;
    }

    return true;
}

void OutputDevice::ImplDrawPolyPolygonFallback(sal_uInt16 nPoly, const tools::PolyPolygon& rPolyPoly)
{
    if (nPoly == 1)
    {
        // #100127# Map to DrawPolygon
        const tools::Polygon& aPoly = rPolyPoly.GetObject(0);
        if (aPoly.GetSize() >= 2)
        {
            vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
            DrawPolygon(aPoly);
        }
    }
    else if (nPoly > 1)
    {
        // #100127# moved real tools::PolyPolygon draw to separate method,
        // have to call recursively, avoiding duplicate
        // ImplLogicToDevicePixel calls
        ImplDrawPolyPolygon(nPoly, mpMapper->LogicToDevicePixel(rPolyPoly));
    }
}

void OutputDevice::DrawPolygon( const basegfx::B2DPolygon& rB2DPolygon)
{
    assert(!is_double_buffered_window());

    // AW: Do NOT paint empty polygons
    if(rB2DPolygon.count())
    {
        basegfx::B2DPolyPolygon aPP( rB2DPolygon );
        DrawPolyPolygon( aPP );
    }
}

void OutputDevice::DrawPolygon( const tools::Polygon& rPoly )
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolygon(rPoly);

    sal_uInt16 nPoints = rPoly.GetSize();
    if (nPoints < 2 || !PrepareGraphicsOutput() || !mpGraphics)
        return;

    // use b2dpolygon drawing if possible
    if (CanDrawPolygon())
    {
        const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation());
        basegfx::B2DPolygon aB2DPolygon(rPoly.getB2DPolygon());

        // ensure closed - maybe assert, hinders buffering
        if (!aB2DPolygon.isClosed())
            aB2DPolygon.setClosed(true);

        if (IsFillColor())
        {
            mpGraphics->DrawPolyPolygon(
                aTransform,
                basegfx::B2DPolyPolygon(aB2DPolygon),
                0.0,
                *this);
        }

        bool bSuccess = true;
        if (IsLineColor())
        {
            const bool bPixelSnapHairline(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

            bSuccess = mpGraphics->DrawPolyLine(
                aTransform,
                aB2DPolygon,
                0.0,
                0.0, // tdf#124848 hairline
                nullptr, // MM01
                basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline,
                *this);
        }

        if (bSuccess)
            return;
    }

    tools::Polygon aPoly = mpMapper->LogicToDevicePixel(rPoly);
    const Point* pPtAry = aPoly.GetConstPointAry();

    // #100127# Forward beziers to sal, if any
    if( aPoly.HasFlags() )
    {
        const PolyFlags* pFlgAry = aPoly.GetConstFlagAry();
        if( !mpGraphics->DrawPolygonBezier( nPoints, pPtAry, pFlgAry, *this ) )
        {
            aPoly = tools::Polygon::SubdivideBezier(aPoly);
            pPtAry = aPoly.GetConstPointAry();
            mpGraphics->DrawPolygon( aPoly.GetSize(), pPtAry, *this );
        }
    }
    else
    {
        mpGraphics->DrawPolygon( nPoints, pPtAry, *this );
    }
}

// Caution: This method is nearly the same as
// OutputDevice::DrawTransparent( const basegfx::B2DPolyPolygon& rB2DPolyPoly, double fTransparency),
// so when changes are made here do not forget to make changes there, too

void OutputDevice::DrawPolyPolygon(const basegfx::B2DPolyPolygon& rB2DPolyPoly)
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(tools::PolyPolygon(rB2DPolyPoly));

    if (!rB2DPolyPoly.count() || !IsDeviceOutputNecessary())
        return;

    if (!PrepareGraphicsOutput() || !mpGraphics)
        return;

    auto drawB2DPolyPolygon = [&]() -> bool
    {
        if (!CanDrawPolygon())
            return false;

        const basegfx::B2DHomMatrix aTransform(mpMapper->GetDeviceTransformation());
        basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);

        // ensure closed - hinders buffering if left unclosed
        if(!aB2DPolyPolygon.isClosed())
            aB2DPolyPolygon.setClosed(true);

        if (IsFillColor())
            mpGraphics->DrawPolyPolygon(aTransform, aB2DPolyPolygon, 0.0, *this);

        if (!ImplDrawPolyPolygonOutlines(aTransform, aB2DPolyPolygon, (255 - GetLineColor().GetAlpha()) / 255.0))
            return false;

        return true;
    };

    if (drawB2DPolyPolygon())
        return;

    // Fallback to legacy tools::PolyPolygon rasterizer
    const tools::PolyPolygon aToolsPolyPolygon(rB2DPolyPoly);
    const tools::PolyPolygon aPixelPolyPolygon = mpMapper->LogicToDevicePixel(aToolsPolyPolygon);
    ImplDrawPolyPolygon(aPixelPolyPolygon.Count(), aPixelPolyPolygon);
}

namespace
{
struct PolyPolyBuffer
{
    // The fast stack buffers
    sal_uInt32 aStackAry1[OUTDEV_POLYPOLY_STACKBUF];
    const Point* aStackAry2[OUTDEV_POLYPOLY_STACKBUF];
    const PolyFlags* aStackAry3[OUTDEV_POLYPOLY_STACKBUF];

    // The active pointers (will point to stack OR heap)
    sal_uInt32* pPointAry;
    const Point** pPointAryAry;
    const PolyFlags** pFlagAryAry;

    bool bUseHeap;

    // Extracted state variables
    sal_uInt16 mnValidCount = 0;
    sal_uInt16 mnLastIndex = 0;
    bool mbHaveBezier = false;

    explicit PolyPolyBuffer(const tools::PolyPolygon& rPolyPoly)
        : bUseHeap(rPolyPoly.Count() > OUTDEV_POLYPOLY_STACKBUF)
    {
        sal_uInt16 nPoly = rPolyPoly.Count();

        if (bUseHeap)
        {
            pPointAry    = new sal_uInt32[nPoly];
            pPointAryAry = new const Point*[nPoly];
            pFlagAryAry  = new const PolyFlags*[nPoly];
        }
        else
        {
            pPointAry    = aStackAry1;
            pPointAryAry = aStackAry2;
            pFlagAryAry  = aStackAry3;
        }

        // Flattens valid sub-polygons into parallel C-arrays for the graphics
        // backend and detects Bézier curves.

        for (sal_uInt16 i = 0; i < nPoly; ++i)
        {
            const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();

            if (nSize)
            {
                pPointAry[mnValidCount] = nSize;
                pPointAryAry[mnValidCount] = rPoly.GetConstPointAry();
                pFlagAryAry[mnValidCount] = rPoly.GetConstFlagAry();
                mnLastIndex = i;

                if (pFlagAryAry[mnValidCount])
                    mbHaveBezier = true;

                ++mnValidCount;
            }
        }
    }

    ~PolyPolyBuffer()
    {
        if (bUseHeap)
        {
            delete[] pPointAry;
            delete[] pPointAryAry;
            delete[] pFlagAryAry;
        }
    }
};

} // end anonymous namespace

void OutputDevice::ImplDrawPolyPolygon( sal_uInt16 nPoly, const tools::PolyPolygon& rPolyPoly )
{
    if (!nPoly)
        return;

    PolyPolyBuffer aBuffer(rPolyPoly);

    if (aBuffer.mnValidCount == 0)
        return;

    if (aBuffer.mnValidCount == 1)
    {
        // Only one valid polygon, fallback to singular draw
        const tools::Polygon& rPoly = rPolyPoly.GetObject(aBuffer.mnLastIndex);
        ImplDrawPolygon(rPoly);

        return;
    }

    // Draw the full PolyPolygon
    if (aBuffer.mbHaveBezier)
    {
        if (!mpGraphics->DrawPolyPolygonBezier(
                aBuffer.mnValidCount,
                aBuffer.pPointAry,
                aBuffer.pPointAryAry,
                aBuffer.pFlagAryAry,
                *this))
        {
            // Backend doesn't support beziers natively, subdivide and retry
            tools::PolyPolygon aPolyPoly = tools::PolyPolygon::SubdivideBezier(rPolyPoly);
            ImplDrawPolyPolygon(aPolyPoly.Count(), aPolyPoly);
        }

        return;
    }

    mpGraphics->DrawPolyPolygon(
        aBuffer.mnValidCount,
        aBuffer.pPointAry,
        aBuffer.pPointAryAry,
        *this);
}

void OutputDevice::ImplDrawPolygon( const tools::Polygon& rPoly, const tools::PolyPolygon* pClipPolyPoly )
{
    if( pClipPolyPoly )
    {
        ImplDrawPolyPolygon( tools::PolyPolygon(rPoly), pClipPolyPoly );
    }
    else
    {
        sal_uInt16 nPoints = rPoly.GetSize();

        if ( nPoints < 2 )
            return;

        const Point* pPtAry = rPoly.GetConstPointAry();
        mpGraphics->DrawPolygon( nPoints, pPtAry, *this );
    }
}

void OutputDevice::ImplDrawPolyPolygon( const tools::PolyPolygon& rPolyPoly, const tools::PolyPolygon* pClipPolyPoly )
{
    tools::PolyPolygon* pPolyPoly;

    if( pClipPolyPoly )
    {
        pPolyPoly = new tools::PolyPolygon;
        rPolyPoly.GetIntersection( *pClipPolyPoly, *pPolyPoly );
    }
    else
    {
        pPolyPoly = const_cast<tools::PolyPolygon*>(&rPolyPoly);
    }
    if( pPolyPoly->Count() == 1 )
    {
        const tools::Polygon& rPoly = pPolyPoly->GetObject( 0 );
        sal_uInt16 nSize = rPoly.GetSize();

        if( nSize >= 2 )
        {
            const Point* pPtAry = rPoly.GetConstPointAry();
            mpGraphics->DrawPolygon( nSize, pPtAry, *this );
        }
    }
    else if( pPolyPoly->Count() )
    {
        sal_uInt16 nCount = pPolyPoly->Count();
        std::unique_ptr<sal_uInt32[]> pPointAry(new sal_uInt32[nCount]);
        std::unique_ptr<const Point*[]> pPointAryAry(new const Point*[nCount]);
        sal_uInt16 i = 0;
        do
        {
            const tools::Polygon& rPoly = pPolyPoly->GetObject( i );
            sal_uInt16 nSize = rPoly.GetSize();
            if ( nSize )
            {
                pPointAry[i] = nSize;
                pPointAryAry[i] = rPoly.GetConstPointAry();
                i++;
            }
            else
                nCount--;
        }
        while( i < nCount );

        if( nCount == 1 )
            mpGraphics->DrawPolygon( pPointAry[0], pPointAryAry[0], *this );
        else
            mpGraphics->DrawPolyPolygon( nCount, pPointAry.get(), pPointAryAry.get(), *this );
    }

    if( pClipPolyPoly )
        delete pPolyPoly;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
