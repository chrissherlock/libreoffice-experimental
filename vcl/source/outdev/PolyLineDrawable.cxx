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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <tools/gen.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/drawables/PolyLineDrawable.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PolyLineDrawable::execute(OutputDevice* pRenderContext) const
{
    if (mbUsesToolsPolygon && !mbUsesLineInfo)
        return Draw(pRenderContext, maPolygon);

    return false;
}

bool PolyLineDrawable::Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon) const
{
    assert(!pRenderContext->is_double_buffered_window());

    GDIMetaFile* pMetaFile = pRenderContext->GetConnectMetaFile();
    if (pMetaFile)
        pMetaFile->AddAction(new MetaPolyLineAction(rPolygon));

    if (!pRenderContext->IsDeviceOutputNecessary()
        || (!pRenderContext->IsLineColor() || pRenderContext->ImplIsRecordLayout()))
        return false;

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    if (pRenderContext->IsLineColorInitialized())
        pRenderContext->InitLineColor();

    sal_uInt16 nPoints = rPolygon.GetSize();

    if (nPoints < 2)
        return false;

    if (pRenderContext->DrawPolyLineDirect(basegfx::B2DHomMatrix(), rPolygon.getB2DPolygon()))
        return true;

    const basegfx::B2DPolygon aB2DPolyLine(rPolygon.getB2DPolygon());
    const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
    const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
    const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                  & AntialiasingFlags::PixelSnapHairline);

    if (pGraphics->DrawPolyLine(aTransform, aB2DPolyLine, 0.0, aB2DLineWidth,
                                basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                                basegfx::deg2rad(15.0), bPixelSnapHairline, pRenderContext))
    {
        return true;
    }

    tools::Polygon aPoly = pRenderContext->ImplLogicToDevicePixel(rPolygon);
    SalPoint* pPtAry = reinterpret_cast<SalPoint*>(aPoly.GetPointAry());

    // #100127# Forward beziers to sal, if any
    if (rPolygon.HasFlags())
    {
        const PolyFlags* pFlgAry = aPoly.GetConstFlagAry();
        if (!pGraphics->DrawPolyLineBezier(nPoints, pPtAry, pFlgAry, pRenderContext))
        {
            aPoly = tools::Polygon::SubdivideBezier(rPolygon);
            tools::Polygon aPolygon(rPolygon);
            pPtAry = reinterpret_cast<SalPoint*>(aPolygon.GetPointAry());
            pGraphics->DrawPolyLine(rPolygon.GetSize(), pPtAry, pRenderContext);
        }
    }
    else
    {
        pGraphics->DrawPolyLine(nPoints, pPtAry, pRenderContext);
    }

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
        Drawable::Draw(pAlphaVDev, PolyLineDrawable(rPolygon));

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
