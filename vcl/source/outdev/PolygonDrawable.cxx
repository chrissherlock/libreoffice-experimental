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
#include <vcl/drawables/PolygonDrawable.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PolygonDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    GDIMetaFile* pMetaFile = pRenderContext->GetConnectMetaFile();
    if (pMetaFile)
        pMetaFile->AddAction(mpMetaAction);

    if (!pRenderContext->IsDeviceOutputNecessary()
        || (!pRenderContext->IsLineColor() && !pRenderContext->IsFillColor())
        || pRenderContext->ImplIsRecordLayout())
        return false;

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    if (pRenderContext->IsClipRegionInitialized())
        pRenderContext->InitClipRegion();

    if (pRenderContext->IsOutputClipped())
        return false;

    if (pRenderContext->IsLineColorInitialized())
        pRenderContext->InitLineColor();

    if (pRenderContext->IsFillColorInitialized())
        pRenderContext->InitFillColor();

    sal_uInt16 nPoints = maPolygon.GetSize();
    if (nPoints < 2)
        return false;

    const bool bTryAA((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
                      && pGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                      && pRenderContext->GetRasterOp() == RasterOp::OverPaint
                      && pRenderContext->IsLineColor());

    // use b2dpolygon drawing if possible
    if (bTryAA)
    {
        const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
        basegfx::B2DPolygon aB2DPolygon(maPolygon.getB2DPolygon());
        bool bSuccess(true);

        // ensure closed - maybe assert, hinders buffering
        if (!aB2DPolygon.isClosed())
            aB2DPolygon.setClosed(true);

        if (pRenderContext->IsFillColor())
        {
            bSuccess = pGraphics->DrawPolyPolygon(aTransform, basegfx::B2DPolyPolygon(aB2DPolygon),
                                                  0.0, pRenderContext);
        }

        if (bSuccess && pRenderContext->IsLineColor())
        {
            const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
            const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                          & AntialiasingFlags::PixelSnapHairline);

            bSuccess = pGraphics->DrawPolyLine(
                aTransform, aB2DPolygon, 0.0, aB2DLineWidth, basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline, pRenderContext);
        }

        if (bSuccess)
        {
            VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
            if (pAlphaVDev)
                Drawable::Draw(pAlphaVDev, PolygonDrawable(maPolygon));

            return true;
        }
    }

    tools::Polygon aPoly = pRenderContext->ImplLogicToDevicePixel(maPolygon);
    const SalPoint* pPtAry = reinterpret_cast<const SalPoint*>(aPoly.GetConstPointAry());

    // #100127# Forward beziers to sal, if any
    if (aPoly.HasFlags())
    {
        const PolyFlags* pFlgAry = aPoly.GetConstFlagAry();
        if (!pGraphics->DrawPolygonBezier(nPoints, pPtAry, pFlgAry, pRenderContext))
        {
            aPoly = tools::Polygon::SubdivideBezier(aPoly);
            pPtAry = reinterpret_cast<const SalPoint*>(aPoly.GetConstPointAry());
            pGraphics->DrawPolygon(aPoly.GetSize(), pPtAry, pRenderContext);
        }
    }
    else
    {
        pGraphics->DrawPolygon(nPoints, pPtAry, pRenderContext);
    }

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
        Drawable::Draw(pAlphaVDev, PolygonDrawable(maPolygon));

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
