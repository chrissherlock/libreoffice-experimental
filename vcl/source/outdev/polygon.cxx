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
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
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

    if (rPoly.GetSize() < 2 || !IsDeviceOutputNecessary())
        return;

    bool bFill = IsFillColor();
    vcl::rendercontext::StrokeAttributes aStroke;
    vcl::rendercontext::StrokeAttributes* pStroke = nullptr;
    double fLineTransparency = 0.0;

    if (IsLineColor())
    {
        aStroke.fWidth = 0.0; // Hairline fallback
        aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        aStroke.eCap = css::drawing::LineCap_BUTT;
        aStroke.fMiterMinimumAngle = basegfx::deg2rad(15.0);
        fLineTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
        pStroke = &aStroke;
    }

    // Delegate to PrimitiveRenderer. If hardware rendering fails, utilize the legacy fallback.
    if (!vcl::rendercontext::PrimitiveRenderer::DrawPolygon(*this, rPoly, bFill, pStroke, fLineTransparency))
        ImplDrawPolygon(rPoly, nullptr);
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

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
