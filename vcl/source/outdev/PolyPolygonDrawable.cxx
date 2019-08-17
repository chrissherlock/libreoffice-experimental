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
#include <vcl/drawables/PolyPolygonDrawable.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PolyPolygonDrawable::execute(OutputDevice* pRenderContext) const
{
    return Draw(pRenderContext, maPolyPolygon);
}

bool PolyPolygonDrawable::Draw(OutputDevice* pRenderContext, tools::PolyPolygon aPolyPolygon) const
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

    sal_uInt16 nPoly = aPolyPolygon.Count();
    if (!nPoly)
        return false;

    const bool bTryAA((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
                      && pGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                      && pRenderContext->GetRasterOp() == RasterOp::OverPaint
                      && (pRenderContext->IsLineColor() || pRenderContext->IsFillColor()));

    // use b2dpolygon drawing if possible
    if (bTryAA)
    {
        const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
        basegfx::B2DPolyPolygon aB2DPolyPolygon(aPolyPolygon.getB2DPolyPolygon());
        bool bSuccess = true;

        // ensure closed - may be asserted, will prevent buffering
        if (!aB2DPolyPolygon.isClosed())
            aB2DPolyPolygon.setClosed(true);

        if (pRenderContext->IsFillColor())
        {
            bSuccess = pGraphics->DrawPolyPolygon(aTransform, aB2DPolyPolygon, 0.0, pRenderContext);
        }

        if (bSuccess && pRenderContext->IsLineColor())
        {
            const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
            const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                          & AntialiasingFlags::PixelSnapHairline);

            for (auto const& rPolygon : aB2DPolyPolygon)
            {
                bSuccess = pGraphics->DrawPolyLine(
                    aTransform, rPolygon, 0.0, aB2DLineWidth, basegfx::B2DLineJoin::NONE,
                    css::drawing::LineCap_BUTT,
                    basegfx::deg2rad(
                        15.0), // not used with B2DLineJoin::NONE, but the correct default
                    bPixelSnapHairline, pRenderContext);

                if (!bSuccess)
                    break;
            }
        }

        if (bSuccess)
        {
            VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
            if (pAlphaVDev)
                Drawable::Draw(pAlphaVDev, PolyPolygonDrawable(aPolyPolygon));

            return true;
        }
    }

    if (nPoly == 1)
    {
        // #100127# Map to DrawPolygon
        const tools::Polygon& aPoly = aPolyPolygon.GetObject(0);
        if (aPoly.GetSize() >= 2)
        {
            GDIMetaFile* pOldMF = pRenderContext->GetConnectMetaFile();
            pRenderContext->SetConnectMetaFile(nullptr);

            Drawable::Draw(pRenderContext, PolygonDrawable(aPoly));

            pRenderContext->SetConnectMetaFile(pOldMF);
        }
    }
    else
    {
        // #100127# moved real tools::PolyPolygon draw to separate method,
        // have to call recursively, avoiding duplicate
        // ImplLogicToDevicePixel calls
        pRenderContext->ImplDrawPolyPolygon(nPoly,
                                            pRenderContext->ImplLogicToDevicePixel(aPolyPolygon));
    }

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
        Drawable::Draw(pAlphaVDev, PolyPolygonDrawable(aPolyPolygon));

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
