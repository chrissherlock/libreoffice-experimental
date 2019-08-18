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
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <tools/gen.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/drawables/PolyLineDrawable.hxx>
#include <vcl/drawables/PolyHairlineDrawable.hxx>
#include <vcl/drawables/LineDrawable.hxx>
#include <vcl/drawables/PolyPolygonDrawable.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PolyLineDrawable::DrawCommand(OutputDevice* pRenderContext) const
{
    if (mbUsesToolsPolygon && !mbUsesLineInfo)
        return Draw(pRenderContext, maPolygon);

    if (mbUsesToolsPolygon && mbUsesLineInfo)
        return Draw(pRenderContext, maPolygon, maLineInfo);

    if (mbUsesB2DPolygon)
        return Draw(pRenderContext, maB2DPolygon, maLineInfo, mfMiterMinimumAngle);

    return false;
}

bool PolyLineDrawable::CanDraw(OutputDevice* pRenderContext) const
{
    if (!pRenderContext->IsDeviceOutputNecessary()
        || (!pRenderContext->IsLineColor() || pRenderContext->ImplIsRecordLayout()))
        return false;

    return true;
}

bool PolyLineDrawable::Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon) const
{
    sal_uInt16 nPoints = rPolygon.GetSize();

    if (nPoints < 2)
        return false;

    if (Drawable::Draw(pRenderContext, PolyHairlineDrawable(basegfx::B2DHomMatrix(),
                                                            rPolygon.getB2DPolygon(), LineInfo())))
        return true;

    const basegfx::B2DPolygon aB2DPolyLine(rPolygon.getB2DPolygon());
    const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
    const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
    const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                  & AntialiasingFlags::PixelSnapHairline);

    if (mpGraphics->DrawPolyLine(aTransform, aB2DPolyLine, 0.0, aB2DLineWidth,
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
        if (!mpGraphics->DrawPolyLineBezier(nPoints, pPtAry, pFlgAry, pRenderContext))
        {
            aPoly = tools::Polygon::SubdivideBezier(rPolygon);
            tools::Polygon aPolygon(rPolygon);
            pPtAry = reinterpret_cast<SalPoint*>(aPolygon.GetPointAry());
            mpGraphics->DrawPolyLine(rPolygon.GetSize(), pPtAry, pRenderContext);
        }
    }
    else
    {
        mpGraphics->DrawPolyLine(nPoints, pPtAry, pRenderContext);
    }

    return true;
}

bool PolyLineDrawable::Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon,
                            LineInfo const aLineInfo) const
{
    if (aLineInfo.IsDefault())
        return Draw(pRenderContext, rPolygon);

    // #i101491#
    // Try direct Fallback to B2D-Version of DrawPolyLine
    if ((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
        && aLineInfo.GetStyle() == LineStyle::Solid)
    {
        return Draw(pRenderContext, rPolygon.getB2DPolygon(), aLineInfo, basegfx::deg2rad(15.0));
    }

    const LineInfo aInfo(pRenderContext->ImplLogicToDevicePixel(aLineInfo));
    const bool bDashUsed(aInfo.GetStyle() == LineStyle::Dash);
    const bool bLineWidthUsed(aInfo.GetWidth() > 1);

    // #i101491# - holds the old line geometry creation and is extended to use AA when
    // switched on. Advantage is that line geometry is only temporarily used for paint
    if (bDashUsed || bLineWidthUsed)
    {
        basegfx::B2DPolyPolygon* pPolyPolygon
            = new basegfx::B2DPolyPolygon(rPolygon.getB2DPolygon());
        Drawable::Draw(pRenderContext, LineDrawable(pPolyPolygon, aInfo));
    }
    else
    {
        tools::Polygon aPoly = pRenderContext->ImplLogicToDevicePixel(rPolygon);

        sal_uInt16 nPoints = 0;

        // #100127# the subdivision HAS to be done here since only a pointer
        // to an array of points is given to the DrawPolyLine method, there is
        // NO way to find out there that it's a curve.
        if (aPoly.HasFlags())
        {
            aPoly = tools::Polygon::SubdivideBezier(aPoly);
            nPoints = aPoly.GetSize();
        }

        tools::Polygon aTmpPoly(aPoly);
        mpGraphics->DrawPolyLine(nPoints, reinterpret_cast<SalPoint*>(aTmpPoly.GetPointAry()),
                                 pRenderContext);
    }

    return true;
}

bool PolyLineDrawable::Draw(OutputDevice* pRenderContext, basegfx::B2DPolygon const& rB2DPolygon,
                            LineInfo aLineInfo, double fMiterMinimumAngle) const
{
    // Do not paint empty PolyPolygons
    if (!rB2DPolygon.count())
        return false;

    // use b2dpolygon drawing if possible
    if (Drawable::Draw(pRenderContext, PolyHairlineDrawable(basegfx::B2DHomMatrix(), rB2DPolygon,
                                                            aLineInfo, 0.0, fMiterMinimumAngle)))
    {
        return true;
    }

    double fLineWidth = aLineInfo.GetWidth();

    mbEnableAlphaVDev = false;

    // #i101491#
    // no output yet; fallback to geometry decomposition and use filled polygon paint
    // when line is fat and not too complex. ImplDrawPolyPolygonWithB2DPolyPolygon
    // will do internal needed AA checks etc.
    if (fLineWidth >= 2.5 && rB2DPolygon.count() && rB2DPolygon.count() <= 1000)
    {
        const double fHalfLineWidth((fLineWidth * 0.5) + 0.5);
        const basegfx::B2DPolyPolygon aAreaPolyPolygon(
            basegfx::utils::createAreaGeometry(rB2DPolygon, fHalfLineWidth, aLineInfo.GetLineJoin(),
                                               aLineInfo.GetLineCap(), fMiterMinimumAngle));
        const Color aOldLineColor(pRenderContext->GetLineColor());
        const Color aOldFillColor(pRenderContext->GetFillColor());

        pRenderContext->SetLineColor();
        pRenderContext->InitLineColor();
        pRenderContext->SetFillColor(aOldLineColor);
        pRenderContext->InitFillColor();

        // draw using a loop; else the topology will paint a PolyPolygon
        for (auto const& rPolygon : aAreaPolyPolygon)
        {
            Drawable::Draw(pRenderContext,
                           PolyPolygonDrawable(basegfx::B2DPolyPolygon(rPolygon), false));
        }

        pRenderContext->SetLineColor(aOldLineColor);
        pRenderContext->InitLineColor();
        pRenderContext->SetFillColor(aOldFillColor);
        pRenderContext->InitFillColor();

        // when AA it is necessary to also paint the filled polygon's outline
        // to avoid optical gaps
        for (auto const& rPolygon : aAreaPolyPolygon)
        {
            Drawable::Draw(pRenderContext,
                           PolyHairlineDrawable(basegfx::B2DHomMatrix(), rPolygon, aLineInfo, 0.0,
                                                basegfx::deg2rad(15.0)));
        }
    }
    else
    {
        // fallback to old polygon drawing if needed
        LineInfo aTmpLineInfo;
        if (fLineWidth != 0.0)
            aTmpLineInfo.SetWidth(static_cast<long>(fLineWidth + 0.5));

        const tools::Polygon aToolsPolygon(rB2DPolygon);
        tools::Polygon aPoly = pRenderContext->ImplLogicToDevicePixel(aToolsPolygon);

        const LineInfo aInfo(pRenderContext->ImplLogicToDevicePixel(aTmpLineInfo));
        const bool bDashUsed(aInfo.GetStyle() == LineStyle::Dash);
        const bool bLineWidthUsed(aInfo.GetWidth() > 1);

        // #i101491# - holds the old line geometry creation and is extended to use AA when
        // switched on. Advantage is that line geometry is only temporarily used for paint
        if (bDashUsed || bLineWidthUsed)
        {
            basegfx::B2DPolyPolygon aPolyPolygon(basegfx::B2DPolyPolygon(aPoly.getB2DPolygon()));
            Drawable::Draw(pRenderContext, LineDrawable(&aPolyPolygon, aInfo));
        }
        else
        {
            sal_uInt16 nPoints = 0;

            // #100127# the subdivision HAS to be done here since only a pointer
            // to an array of points is given to the DrawPolyLine method, there is
            // NO way to find out there that it's a curve.
            if (aPoly.HasFlags())
            {
                aPoly = tools::Polygon::SubdivideBezier(aPoly);
                nPoints = aPoly.GetSize();
            }

            tools::Polygon aTmpPoly(aPoly);
            mpGraphics->DrawPolyLine(nPoints, reinterpret_cast<SalPoint*>(aTmpPoly.GetPointAry()),
                                     pRenderContext);
        }

        VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
        if (pAlphaVDev)
            Drawable::Draw(pAlphaVDev, PolyLineDrawable(aPoly, aLineInfo));
    }

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
