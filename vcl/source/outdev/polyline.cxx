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
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>
#include <vcl/drawables/LineDrawable.hxx>
#include <vcl/drawables/PolyLineDrawable.hxx>

#include <salgdi.hxx>

bool OutputDevice::DrawPolyLineDirect(
    const basegfx::B2DHomMatrix& rObjectTransform,
    const basegfx::B2DPolygon& rB2DPolygon,
    double fLineWidth,
    double fTransparency,
    basegfx::B2DLineJoin eLineJoin,
    css::drawing::LineCap eLineCap,
    double fMiterMinimumAngle,
    bool bBypassAACheck)
{
    assert(!is_double_buffered_window());

    // AW: Do NOT paint empty PolyPolygons
    if(!rB2DPolygon.count())
        return true;

    // we need a graphics
    if( !mpGraphics && !AcquireGraphics() )
        return false;

    if( mbInitClipRegion )
        InitClipRegion();

    if( mbOutputClipped )
        return true;

    if( mbInitLineColor )
        InitLineColor();

    const bool bTryAA( bBypassAACheck ||
                      ((mnAntialiasing & AntialiasingFlags::EnableB2dDraw) &&
                      mpGraphics->supportsOperation(OutDevSupportType::B2DDraw) &&
                      RasterOp::OverPaint == GetRasterOp() &&
                      IsLineColor()));

    if(bTryAA)
    {
        // combine rObjectTransform with WorldToDevice
        const basegfx::B2DHomMatrix aTransform(ImplGetDeviceTransformation() * rObjectTransform);
        const bool bLineWidthZero(basegfx::fTools::equalZero(fLineWidth));
        const basegfx::B2DVector aB2DLineWidth(bLineWidthZero ? 1.0 : fLineWidth, bLineWidthZero ? 1.0 : fLineWidth);
        const bool bPixelSnapHairline((mnAntialiasing & AntialiasingFlags::PixelSnapHairline) && rB2DPolygon.count() < 1000);

        // draw the polyline
        bool bDrawSuccess = mpGraphics->DrawPolyLine(
            aTransform,
            rB2DPolygon,
            fTransparency,
            aB2DLineWidth,
            eLineJoin,
            eLineCap,
            fMiterMinimumAngle,
            bPixelSnapHairline,
            this);

        if( bDrawSuccess )
        {
            // worked, add metafile action (if recorded) and return true
            if( mpMetaFile )
            {
                LineInfo aLineInfo;
                if( fLineWidth != 0.0 )
                    aLineInfo.SetWidth( static_cast<long>(fLineWidth+0.5) );
                // Transport known information, might be needed
                aLineInfo.SetLineJoin(eLineJoin);
                aLineInfo.SetLineCap(eLineCap);
                // MiterMinimumAngle does not exist yet in LineInfo
                const tools::Polygon aToolsPolygon( rB2DPolygon );
                mpMetaFile->AddAction( new MetaPolyLineAction( aToolsPolygon, aLineInfo ) );
            }
            return true;
        }
    }
    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
