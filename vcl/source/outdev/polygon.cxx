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

#include <cassert>

#include <sal/types.h>

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <memory>
#include <tools/poly.hxx>

#include <vcl/drawables/PolygonDrawable.hxx>
#include <vcl/drawables/PolyPolygonDrawable.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>

#include <salgdi.hxx>

// Caution: This method is nearly the same as
// OutputDevice::DrawTransparent( const basegfx::B2DPolyPolygon& rB2DPolyPoly, double fTransparency),
// so when changes are made here do not forget to make changes there, too

void OutputDevice::DrawPolyPolygon( const basegfx::B2DPolyPolygon& rB2DPolyPoly )
{
    assert(!is_double_buffered_window());

    if( mpMetaFile )
        mpMetaFile->AddAction( new MetaPolyPolygonAction( tools::PolyPolygon( rB2DPolyPoly ) ) );

    // call helper
    ImplDrawPolyPolygonWithB2DPolyPolygon(rB2DPolyPoly);
}

void OutputDevice::ImplDrawPolyPolygonWithB2DPolyPolygon(const basegfx::B2DPolyPolygon& rB2DPolyPoly)
{
    // Do not paint empty PolyPolygons
    if(!rB2DPolyPoly.count() || !IsDeviceOutputNecessary())
        return;

    // we need a graphics
    if( !mpGraphics && !AcquireGraphics() )
        return;

    if( mbInitClipRegion )
        InitClipRegion();

    if( mbOutputClipped )
        return;

    if( mbInitLineColor )
        InitLineColor();

    if( mbInitFillColor )
        InitFillColor();

    if((mnAntialiasing & AntialiasingFlags::EnableB2dDraw) &&
       mpGraphics->supportsOperation(OutDevSupportType::B2DDraw) &&
       RasterOp::OverPaint == GetRasterOp() &&
       (IsLineColor() || IsFillColor()))
    {
        const basegfx::B2DHomMatrix aTransform(ImplGetDeviceTransformation());
        basegfx::B2DPolyPolygon aB2DPolyPolygon(rB2DPolyPoly);
        bool bSuccess(true);

        // ensure closed - maybe assert, hinders buffering
        if(!aB2DPolyPolygon.isClosed())
        {
            aB2DPolyPolygon.setClosed(true);
        }

        if(IsFillColor())
        {
            bSuccess = mpGraphics->DrawPolyPolygon(
                aTransform,
                aB2DPolyPolygon,
                0.0,
                this);
        }

        if(bSuccess && IsLineColor())
        {
            const basegfx::B2DVector aB2DLineWidth( 1.0, 1.0 );
            const bool bPixelSnapHairline(mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

            for(auto const& rPolygon : aB2DPolyPolygon)
            {
                bSuccess = mpGraphics->DrawPolyLine(
                    aTransform,
                    rPolygon,
                    0.0,
                    aB2DLineWidth,
                    basegfx::B2DLineJoin::NONE,
                    css::drawing::LineCap_BUTT,
                    basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                    bPixelSnapHairline,
                    this);
                if (!bSuccess)
                    break;
            }
        }

        if(bSuccess)
        {
            return;
        }
    }

    // fallback to old polygon drawing if needed
    const tools::PolyPolygon aToolsPolyPolygon( rB2DPolyPoly );
    const tools::PolyPolygon aPixelPolyPolygon = ImplLogicToDevicePixel( aToolsPolyPolygon );
    Drawable::Draw(this, PolyPolygonDrawable(aPixelPolyPolygon.Count(), aPixelPolyPolygon));
}

void OutputDevice::ImplDrawPolygon( const tools::Polygon& rPoly, const tools::PolyPolygon* pClipPolyPoly )
{
    if( pClipPolyPoly )
    {
        ImplDrawPolyPolygon( rPoly, pClipPolyPoly );
    }
    else
    {
        sal_uInt16 nPoints = rPoly.GetSize();

        if ( nPoints < 2 )
            return;

        const SalPoint* pPtAry = reinterpret_cast<const SalPoint*>(rPoly.GetConstPointAry());
        mpGraphics->DrawPolygon( nPoints, pPtAry, this );
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
            const SalPoint* pPtAry = reinterpret_cast<const SalPoint*>(rPoly.GetConstPointAry());
            mpGraphics->DrawPolygon( nSize, pPtAry, this );
        }
    }
    else if( pPolyPoly->Count() )
    {
        sal_uInt16 nCount = pPolyPoly->Count();
        std::unique_ptr<sal_uInt32[]> pPointAry(new sal_uInt32[nCount]);
        std::unique_ptr<PCONSTSALPOINT[]> pPointAryAry(new PCONSTSALPOINT[nCount]);
        sal_uInt16 i = 0;
        do
        {
            const tools::Polygon& rPoly = pPolyPoly->GetObject( i );
            sal_uInt16 nSize = rPoly.GetSize();
            if ( nSize )
            {
                pPointAry[i] = nSize;
                pPointAryAry[i] = reinterpret_cast<PCONSTSALPOINT>(rPoly.GetConstPointAry());
                i++;
            }
            else
                nCount--;
        }
        while( i < nCount );

        if( nCount == 1 )
            mpGraphics->DrawPolygon( pPointAry[0], pPointAryAry[0], this );
        else
            mpGraphics->DrawPolyPolygon( nCount, pPointAry.get(), pPointAryAry.get(), this );
    }

    if( pClipPolyPoly )
        delete pPolyPoly;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
