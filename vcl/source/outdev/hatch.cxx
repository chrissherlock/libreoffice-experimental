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

#include <osl/diagnose.h>
#include <tools/helpers.hxx>
#include <tools/line.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/hatch.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/settings.hxx>

#include <drawmode.hxx>

#include <cassert>
#include <cstdlib>
#include <memory>

void OutputDevice::DrawHatch(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch)
{
    assert(!is_double_buffered_window());

    Hatch aHatch(rHatch);
    aHatch.SetColor(GetDrawModeHatchColor(rHatch.GetColor(), GetDrawMode(), GetSettings().GetStyleSettings()));

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaHatchAction(rPolyPoly, aHatch));

    RenderContext2::DrawHatch(rPolyPoly, rHatch);
}

void OutputDevice::DrawHatchLines(tools::Line const& rLine, tools::PolyPolygon const& rPolyPoly,
                                 Point* pPtBuffer)
{
    assert(!is_double_buffered_window());

    tools::Long nPCounter = 0;
    std::tie(nPCounter, pPtBuffer) = GenerateHatchPointBuffer(rLine, rPolyPoly, pPtBuffer);

    for (tools::Long i = 0; i < nPCounter; i += 2)
    {
        DrawHatchLine(pPtBuffer[i], pPtBuffer[i + 1]);
    }
}

void OutputDevice::DrawHatchLine(Point const& rStartPoint, Point const& rEndPoint)
{
    mpMetaFile->AddAction(new MetaLineAction(rStartPoint, rEndPoint));
}

void OutputDevice::AddHatchActions(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch,
                                   GDIMetaFile& rMtf)
{
    assert(!is_double_buffered_window());

    tools::PolyPolygon aPolyPoly(rPolyPoly);
    aPolyPoly.Optimize(PolyOptimizeFlags::NO_SAME | PolyOptimizeFlags::CLOSE);

    if (aPolyPoly.Count())
    {
        GDIMetaFile* pOldMtf = mpMetaFile;

        mpMetaFile = &rMtf;
        mpMetaFile->AddAction(new MetaPushAction(PushFlags::ALL));
        mpMetaFile->AddAction(new MetaLineColorAction(rHatch.GetColor(), true));

        if (!aPolyPoly.Count())
            return;

        // #i115630# DrawHatch does not work with beziers included in the polypolygon, take care of that
        bool bIsCurve = false;

        for (sal_uInt16 a(0); !bIsCurve && a < rPolyPoly.Count(); a++)
        {
            if (aPolyPoly[a].HasFlags())
                bIsCurve = true;
        }

        if (bIsCurve)
        {
            OSL_ENSURE(false,
                       "DrawHatch does *not* support curves, falling back to AdaptiveSubdivide()...");
            tools::PolyPolygon aPolyPoly2;

            aPolyPoly.AdaptiveSubdivide(aPolyPoly2);
            DrawHatch(aPolyPoly2, rHatch);
        }
        else
        {
            tools::Rectangle aRect(aPolyPoly.GetBoundRect());
            const tools::Long nLogPixelWidth = ImplDevicePixelToLogicWidth(1);
            const tools::Long nWidth = ImplDevicePixelToLogicWidth(
                std::max(ImplLogicWidthToDevicePixel(rHatch.GetDistance()), tools::Long(3)));
            std::unique_ptr<Point[]> pPtBuffer(new Point[HATCH_MAXPOINTS]);
            Point aPt1, aPt2, aEndPt1;
            Size aInc;

            // Single hatch
            aRect.AdjustLeft(-nLogPixelWidth);
            aRect.AdjustTop(-nLogPixelWidth);
            aRect.AdjustRight(nLogPixelWidth);
            aRect.AdjustBottom(nLogPixelWidth);

            CalcHatchValues(aRect, nWidth, rHatch.GetAngle(), aPt1, aPt2, aInc, aEndPt1);
            do
            {
                DrawHatchLines(tools::Line(aPt1, aPt2), aPolyPoly, pPtBuffer.get());
                aPt1.AdjustX(aInc.Width());
                aPt1.AdjustY(aInc.Height());
                aPt2.AdjustX(aInc.Width());
                aPt2.AdjustY(aInc.Height());
            } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));

            if ((rHatch.GetStyle() == HatchStyle::Double) || (rHatch.GetStyle() == HatchStyle::Triple))
            {
                // Double hatch
                CalcHatchValues(aRect, nWidth, rHatch.GetAngle() + 900_deg10, aPt1, aPt2, aInc,
                                aEndPt1);
                do
                {
                    DrawHatchLines(tools::Line(aPt1, aPt2), aPolyPoly, pPtBuffer.get());
                    aPt1.AdjustX(aInc.Width());
                    aPt1.AdjustY(aInc.Height());
                    aPt2.AdjustX(aInc.Width());
                    aPt2.AdjustY(aInc.Height());
                } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));

                if (rHatch.GetStyle() == HatchStyle::Triple)
                {
                    // Triple hatch
                    CalcHatchValues(aRect, nWidth, rHatch.GetAngle() + 450_deg10, aPt1, aPt2, aInc,
                                    aEndPt1);
                    do
                    {
                        DrawHatchLines(tools::Line(aPt1, aPt2), aPolyPoly, pPtBuffer.get());
                        aPt1.AdjustX(aInc.Width());
                        aPt1.AdjustY(aInc.Height());
                        aPt2.AdjustX(aInc.Width());
                        aPt2.AdjustY(aInc.Height());
                    } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));
                }
            }
        }

        mpMetaFile->AddAction(new MetaPopAction());
        mpMetaFile = pOldMtf;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
