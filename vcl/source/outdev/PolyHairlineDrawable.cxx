/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with pRenderContext
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with pRenderContext work for additional information regarding copyright
 *   ownership. The ASF licenses pRenderContext file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use pRenderContext file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/drawables/PolyHairlineDrawable.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PolyHairlineDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    double fLineWidth = maLineInfo.GetWidth();
    basegfx::B2DLineJoin eLineJoin = maLineInfo.GetLineJoin();
    css::drawing::LineCap eLineCap = maLineInfo.GetLineCap();

    if (!pRenderContext->IsDeviceOutputNecessary() || pRenderContext->ImplIsRecordLayout())
        return false;

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    // Do not paint empty PolyPolygons
    if (!maB2DPolygon.count())
        return false;

    if (pRenderContext->IsClipRegionInitialized())
        pRenderContext->InitClipRegion();

    if (pRenderContext->IsOutputClipped())
        return false;

    if (pRenderContext->IsLineColorInitialized())
        pRenderContext->InitLineColor();

    const bool bTryAA((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
                      && pGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                      && pRenderContext->GetRasterOp() == RasterOp::OverPaint
                      && pRenderContext->IsLineColor());

    if (bTryAA)
    {
        // combine mrObjectTransform with WorldToDevice
        const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation()
                                               * maObjectTransform);
        const bool bLineWidthZero(basegfx::fTools::equalZero(fLineWidth));
        const basegfx::B2DVector aB2DLineWidth(bLineWidthZero ? 1.0 : fLineWidth,
                                               bLineWidthZero ? 1.0 : fLineWidth);
        const bool bPixelSnapHairline(
            (pRenderContext->GetAntialiasing() & AntialiasingFlags::PixelSnapHairline)
            && maB2DPolygon.count() < 1000);

        // draw the polyline
        bool bDrawSuccess = pGraphics->DrawPolyLine(
            aTransform, maB2DPolygon, mfTransparency, aB2DLineWidth, eLineJoin, eLineCap,
            mfMiterMinimumAngle, bPixelSnapHairline, pRenderContext);

        if (bDrawSuccess)
        {
            GDIMetaFile* pMetaFile = pRenderContext->GetConnectMetaFile();
            if (pMetaFile)
                pMetaFile->AddAction(mpMetaAction);

            return true;
        }
    }
    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
