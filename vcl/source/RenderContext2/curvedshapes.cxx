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

#include <vcl/RenderContext2.hxx>

#include <salgdi.hxx>

void RenderContext2::DrawEllipse(tools::Rectangle const& rRect)
{
    if (!IsDeviceOutputNecessary() || (!IsOpaqueLineColor() && !IsOpaqueFillColor()))
        return;

    tools::Rectangle aRect(maGeometry.LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsInitClipped())
        InitClipRegion();
    if (maRegion.IsEmpty())
        return;

    if (IsInitLineColor())
        InitLineColor();

    tools::Polygon aRectPoly(aRect.Center(), aRect.GetWidth() >> 1, aRect.GetHeight() >> 1);
    if (aRectPoly.GetSize() >= 2)
    {
        OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

        Point* pPtAry = aRectPoly.GetPointAry();
        if (!IsOpaqueFillColor())
        {
            mpGraphics->DrawPolyLine(aRectPoly.GetSize(), pPtAry, *pOutDev);
        }
        else
        {
            if (IsInitFillColor())
                InitFillColor();
            mpGraphics->DrawPolygon(aRectPoly.GetSize(), pPtAry, *pOutDev);
        }
    }
}

void RenderContext2::DrawArc(tools::Rectangle const& rRect, Point const& rStartPt,
                             Point const& rEndPt)
{
    if (!IsDeviceOutputNecessary() || !IsOpaqueLineColor())
        return;

    tools::Rectangle aRect(maGeometry.LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsInitClipped())
        InitClipRegion();
    if (maRegion.IsEmpty())
        return;

    if (IsInitLineColor())
        InitLineColor();

    const Point aStart(maGeometry.LogicToDevicePixel(rStartPt));
    const Point aEnd(maGeometry.LogicToDevicePixel(rEndPt));
    tools::Polygon aArcPoly(aRect, aStart, aEnd, PolyStyle::Arc);

    if (aArcPoly.GetSize() >= 2)
    {
        OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

        Point* pPtAry = aArcPoly.GetPointAry();
        mpGraphics->DrawPolyLine(aArcPoly.GetSize(), pPtAry, *pOutDev);
    }
}

void RenderContext2::DrawPie(tools::Rectangle const& rRect, Point const& rStartPt,
                             Point const& rEndPt)
{
    if (!IsDeviceOutputNecessary() || (!IsOpaqueLineColor() && !IsOpaqueFillColor()))
        return;

    tools::Rectangle aRect(maGeometry.LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsInitClipped())
        InitClipRegion();
    if (maRegion.IsEmpty())
        return;

    if (IsInitLineColor())
        InitLineColor();

    const Point aStart(maGeometry.LogicToDevicePixel(rStartPt));
    const Point aEnd(maGeometry.LogicToDevicePixel(rEndPt));
    tools::Polygon aPiePoly(aRect, aStart, aEnd, PolyStyle::Pie);

    if (aPiePoly.GetSize() >= 2)
    {
        OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

        Point* pPtAry = aPiePoly.GetPointAry();
        if (!IsOpaqueFillColor())
        {
            mpGraphics->DrawPolyLine(aPiePoly.GetSize(), pPtAry, *pOutDev);
        }
        else
        {
            if (IsInitFillColor())
                InitFillColor();

            mpGraphics->DrawPolygon(aPiePoly.GetSize(), pPtAry, *pOutDev);
        }
    }
}

void RenderContext2::DrawChord(tools::Rectangle const& rRect, Point const& rStartPt,
                               Point const& rEndPt)
{
    if (!IsDeviceOutputNecessary() || (!IsOpaqueLineColor() && !IsOpaqueFillColor()))
        return;

    tools::Rectangle aRect(maGeometry.LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsInitClipped())
        InitClipRegion();
    if (maRegion.IsEmpty())
        return;

    if (IsInitLineColor())
        InitLineColor();

    const Point aStart(maGeometry.LogicToDevicePixel(rStartPt));
    const Point aEnd(maGeometry.LogicToDevicePixel(rEndPt));
    tools::Polygon aChordPoly(aRect, aStart, aEnd, PolyStyle::Chord);

    if (aChordPoly.GetSize() >= 2)
    {
        OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

        Point* pPtAry = aChordPoly.GetPointAry();
        if (!IsOpaqueFillColor())
        {
            mpGraphics->DrawPolyLine(aChordPoly.GetSize(), pPtAry, *pOutDev);
        }
        else
        {
            if (IsInitFillColor())
                InitFillColor();

            mpGraphics->DrawPolygon(aChordPoly.GetSize(), pPtAry, *pOutDev);
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
