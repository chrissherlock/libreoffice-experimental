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

bool PolygonDrawable::DrawCommand(OutputDevice* pRenderContext) const
{
    if (mbUsesB2DPolygon && !mbClipped)
        return Draw(pRenderContext, maB2DPolygon);
    else if (!mbUsesB2DPolygon && mbClipped)
        return Draw(pRenderContext, maPolygon, maClipPolyPolygon);
    else
        return Draw(pRenderContext, maPolygon);
}

bool PolygonDrawable::Draw(OutputDevice* pRenderContext, tools::Polygon aPolygon) const
{
    sal_uInt16 nPoints = aPolygon.GetSize();
    if (nPoints < 2)
        return false;

    const bool bTryAA((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
                      && mpGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                      && pRenderContext->GetRasterOp() == RasterOp::OverPaint
                      && pRenderContext->IsLineColor());

    // use b2dpolygon drawing if possible
    if (bTryAA)
    {
        const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
        basegfx::B2DPolygon aB2DPolygon(aPolygon.getB2DPolygon());
        bool bSuccess(true);

        // ensure closed - maybe assert, hinders buffering
        if (!aB2DPolygon.isClosed())
            aB2DPolygon.setClosed(true);

        if (pRenderContext->IsFillColor())
        {
            bSuccess = mpGraphics->DrawPolyPolygon(aTransform, basegfx::B2DPolyPolygon(aB2DPolygon),
                                                   0.0, pRenderContext);
        }

        if (bSuccess && pRenderContext->IsLineColor())
        {
            const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
            const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                          & AntialiasingFlags::PixelSnapHairline);

            bSuccess = mpGraphics->DrawPolyLine(
                aTransform, aB2DPolygon, 0.0, aB2DLineWidth, basegfx::B2DLineJoin::NONE,
                css::drawing::LineCap_BUTT,
                basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
                bPixelSnapHairline, pRenderContext);
        }

        if (bSuccess)
        {
            VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
            if (pAlphaVDev)
                Drawable::Draw(pAlphaVDev, PolygonDrawable(aPolygon));

            return true;
        }
    }

    tools::Polygon aPoly = pRenderContext->ImplLogicToDevicePixel(aPolygon);
    const SalPoint* pPtAry = reinterpret_cast<const SalPoint*>(aPoly.GetConstPointAry());

    // #100127# Forward beziers to sal, if any
    if (aPoly.HasFlags())
    {
        const PolyFlags* pFlgAry = aPoly.GetConstFlagAry();
        if (!mpGraphics->DrawPolygonBezier(nPoints, pPtAry, pFlgAry, pRenderContext))
        {
            aPoly = tools::Polygon::SubdivideBezier(aPoly);
            pPtAry = reinterpret_cast<const SalPoint*>(aPoly.GetConstPointAry());
            mpGraphics->DrawPolygon(aPoly.GetSize(), pPtAry, pRenderContext);
        }
    }
    else
    {
        mpGraphics->DrawPolygon(nPoints, pPtAry, pRenderContext);
    }

    return true;
}

bool PolygonDrawable::Draw(OutputDevice* pRenderContext,
                           basegfx::B2DPolygon const aB2DPolygon) const
{
    assert(!pRenderContext->is_double_buffered_window());

    // do not paint empty polygons
    if (aB2DPolygon.count())
    {
        basegfx::B2DPolyPolygon aPP(aB2DPolygon);
        Drawable::Draw(pRenderContext, PolyPolygonDrawable(aPP));
    }

    return true;
}

bool PolygonDrawable::Draw(OutputDevice* pRenderContext, tools::Polygon aPolygon,
                           tools::PolyPolygon aClipPolyPolygon) const
{
    return Drawable::Draw(pRenderContext, PolyPolygonDrawable(aPolygon, aClipPolyPolygon));
}

bool PolygonDrawable::CanDraw(OutputDevice* pRenderContext) const
{
    if (!pRenderContext->IsDeviceOutputNecessary()
        || (!pRenderContext->IsLineColor() && !pRenderContext->IsFillColor())
        || pRenderContext->ImplIsRecordLayout())
        return false;

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
