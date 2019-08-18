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
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>
#include <vcl/drawables/LineDrawable.hxx>

#include <salgdi.hxx>
#include <outdata.hxx>

#include <cassert>
#include <numeric>

bool LineDrawable::DrawCommand(OutputDevice* pRenderContext) const
{
    if (mbUsesPolyPolygon)
        return Draw(pRenderContext, mpLinePolyPolygon, maLineInfo);
    else if (mbUsesLineInfo)
        return Draw(pRenderContext, maLineInfo);
    else
        return Draw(pRenderContext);
}

bool LineDrawable::CanDraw(OutputDevice* pRenderContext) const
{
    if (UsesScaffolding()
        && (!pRenderContext->IsDeviceOutputNecessary()
            || (!pRenderContext->IsLineColor() && !pRenderContext->IsFillColor())
            || pRenderContext->ImplIsRecordLayout()))
        return false;

    return true;
}

bool LineDrawable::Draw(OutputDevice* pRenderContext) const
{
    // #i101598# support AA and snap for lines, too
    if ((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
        && mpGraphics->supportsOperation(OutDevSupportType::B2DDraw)
        && pRenderContext->GetRasterOp() == RasterOp::OverPaint && pRenderContext->IsLineColor())
    {
        // at least transform with double precision to device coordinates; pRenderContext will
        // avoid pixel snap of single, appended lines
        const basegfx::B2DHomMatrix aTransform(pRenderContext->ImplGetDeviceTransformation());
        const basegfx::B2DVector aB2DLineWidth(1.0, 1.0);
        basegfx::B2DPolygon aB2DPolyLine;

        aB2DPolyLine.append(basegfx::B2DPoint(maStartPt.X(), maStartPt.Y()));
        aB2DPolyLine.append(basegfx::B2DPoint(maEndPt.X(), maEndPt.Y()));
        aB2DPolyLine.transform(aTransform);

        const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                      & AntialiasingFlags::PixelSnapHairline);

        return mpGraphics->DrawPolyLine(
            basegfx::B2DHomMatrix(), aB2DPolyLine, 0.0, aB2DLineWidth, basegfx::B2DLineJoin::NONE,
            css::drawing::LineCap_BUTT,
            basegfx::deg2rad(15.0), // not used with B2DLineJoin::NONE, but the correct default
            bPixelSnapHairline, pRenderContext);
    }

    const Point aStartPt(pRenderContext->ImplLogicToDevicePixel(maStartPt));
    const Point aEndPt(pRenderContext->ImplLogicToDevicePixel(maEndPt));

    mpGraphics->DrawLine(aStartPt.X(), aStartPt.Y(), aEndPt.X(), aEndPt.Y(), pRenderContext);

    return true;
}

bool LineDrawable::Draw(OutputDevice* pRenderContext, LineInfo const aLineInfo) const
{
    if (aLineInfo.IsDefault())
        return Draw(pRenderContext);

    const Point aStartPt(pRenderContext->ImplLogicToDevicePixel(maStartPt));
    const Point aEndPt(pRenderContext->ImplLogicToDevicePixel(maEndPt));
    const LineInfo aInfo(pRenderContext->ImplLogicToDevicePixel(aLineInfo));
    const bool bDashUsed(aInfo.GetStyle() == LineStyle::Dash);
    const bool bLineWidthUsed(aInfo.GetWidth() > 1);

    if (bDashUsed || bLineWidthUsed)
    {
        basegfx::B2DPolygon aLinePolygon;
        aLinePolygon.append(basegfx::B2DPoint(aStartPt.X(), aStartPt.Y()));
        aLinePolygon.append(basegfx::B2DPoint(aEndPt.X(), aEndPt.Y()));

        basegfx::B2DPolyPolygon* pLinePolyPolygon = new basegfx::B2DPolyPolygon(aLinePolygon);
        Draw(pRenderContext, pLinePolyPolygon, aInfo);
    }
    else
    {
        mpGraphics->DrawLine(aStartPt.X(), aStartPt.Y(), aEndPt.X(), aEndPt.Y(), pRenderContext);
    }

    return true;
}

bool LineDrawable::Draw(OutputDevice* pRenderContext, basegfx::B2DPolyPolygon* pLinePolyPolygon,
                        const LineInfo& rInfo) const
{
    basegfx::B2DPolyPolygon aFillPolyPolygon;
    const bool bDashUsed(LineStyle::Dash == rInfo.GetStyle());
    const bool bLineWidthUsed(rInfo.GetWidth() > 1);

    basegfx::B2DPolyPolygon aLinePolyPolygon(*pLinePolyPolygon);

    if (bDashUsed && aLinePolyPolygon.count())
    {
        ::std::vector<double> fDotDashArray;
        const double fDashLen(rInfo.GetDashLen());
        const double fDotLen(rInfo.GetDotLen());
        const double fDistance(rInfo.GetDistance());

        for (sal_uInt16 a(0); a < rInfo.GetDashCount(); a++)
        {
            fDotDashArray.push_back(fDashLen);
            fDotDashArray.push_back(fDistance);
        }

        for (sal_uInt16 b(0); b < rInfo.GetDotCount(); b++)
        {
            fDotDashArray.push_back(fDotLen);
            fDotDashArray.push_back(fDistance);
        }

        const double fAccumulated(
            ::std::accumulate(fDotDashArray.begin(), fDotDashArray.end(), 0.0));

        if (fAccumulated > 0.0)
        {
            basegfx::B2DPolyPolygon aResult;

            for (auto const& rPolygon : aLinePolyPolygon)
            {
                basegfx::B2DPolyPolygon aLineTarget;
                basegfx::utils::applyLineDashing(rPolygon, fDotDashArray, &aLineTarget);
                aResult.append(aLineTarget);
            }

            aLinePolyPolygon = aResult;
        }
    }

    if (bLineWidthUsed && aLinePolyPolygon.count())
    {
        const double fHalfLineWidth((rInfo.GetWidth() * 0.5) + 0.5);

        if (aLinePolyPolygon.areControlPointsUsed())
        {
            // #i110768# When area geometry has to be created, do not
            // use the fallback bezier decomposition inside createAreaGeometry,
            // but one that is at least as good as ImplSubdivideBezier was.
            // There, Polygon::AdaptiveSubdivide was used with default parameter
            // 1.0 as quality index.
            aLinePolyPolygon = basegfx::utils::adaptiveSubdivideByDistance(aLinePolyPolygon, 1.0);
        }

        for (auto const& rPolygon : aLinePolyPolygon)
        {
            aFillPolyPolygon.append(basegfx::utils::createAreaGeometry(
                rPolygon, fHalfLineWidth, rInfo.GetLineJoin(), rInfo.GetLineCap()));
        }

        aLinePolyPolygon.clear();
    }

    GDIMetaFile* pOldMetaFile = pRenderContext->GetConnectMetaFile();
    pRenderContext->SetConnectMetaFile(nullptr);

    const bool bTryAA((pRenderContext->GetAntialiasing() & AntialiasingFlags::EnableB2dDraw)
                      && mpGraphics->supportsOperation(OutDevSupportType::B2DDraw)
                      && pRenderContext->GetRasterOp() == RasterOp::OverPaint
                      && pRenderContext->IsLineColor());

    if (aLinePolyPolygon.count())
    {
        for (auto const& rB2DPolygon : aLinePolyPolygon)
        {
            const bool bPixelSnapHairline(pRenderContext->GetAntialiasing()
                                          & AntialiasingFlags::PixelSnapHairline);
            bool bDone = false;

            if (bTryAA)
            {
                bDone = mpGraphics->DrawPolyLine(
                    basegfx::B2DHomMatrix(), rB2DPolygon, 0.0, basegfx::B2DVector(1.0, 1.0),
                    basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                    basegfx::deg2rad(
                        15.0), // not used with B2DLineJoin::NONE, but the correct default
                    bPixelSnapHairline, pRenderContext);
            }

            if (!bDone)
            {
                tools::Polygon aPolygon(rB2DPolygon);
                mpGraphics->DrawPolyLine(aPolygon.GetSize(),
                                         reinterpret_cast<SalPoint*>(aPolygon.GetPointAry()),
                                         pRenderContext);
            }
        }
    }

    if (aFillPolyPolygon.count())
    {
        const Color aOldLineColor(pRenderContext->GetLineColor());
        const Color aOldFillColor(pRenderContext->GetFillColor());

        pRenderContext->SetLineColor();
        pRenderContext->InitLineColor();
        pRenderContext->SetFillColor(aOldLineColor);
        pRenderContext->InitFillColor();

        bool bDone = false;

        if (bTryAA)
        {
            bDone = mpGraphics->DrawPolyPolygon(basegfx::B2DHomMatrix(), aFillPolyPolygon, 0.0,
                                                pRenderContext);
        }

        if (!bDone)
        {
            for (auto const& rB2DPolygon : aFillPolyPolygon)
            {
                tools::Polygon aPolygon(rB2DPolygon);

                // need to subdivide, mpGraphics->DrawPolygon ignores curves
                aPolygon.AdaptiveSubdivide(aPolygon);
                mpGraphics->DrawPolygon(
                    aPolygon.GetSize(),
                    reinterpret_cast<const SalPoint*>(aPolygon.GetConstPointAry()), pRenderContext);
            }
        }

        pRenderContext->SetFillColor(aOldFillColor);
        pRenderContext->SetLineColor(aOldLineColor);
    }

    pRenderContext->SetConnectMetaFile(pOldMetaFile);

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
