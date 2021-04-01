/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>

Color RenderContext2::GetPixel(Point const& rPoint) const
{
    Color aColor;

    if (mpGraphics || AcquireGraphics())
    {
        if (mbInitClipRegion)
            const_cast<RenderContext2*>(this)->InitClipRegion();

        if (!mbOutputClipped)
        {
            const tools::Long nX = ImplLogicXToDevicePixel(rPoint.X());
            const tools::Long nY = ImplLogicYToDevicePixel(rPoint.Y());
            aColor = mpGraphics->GetPixel(nX, nY, *this);

            if (mpAlphaVDev)
            {
                Color aAlphaColor = mpAlphaVDev->GetPixel(rPoint);
                aColor.SetAlpha(255 - aAlphaColor.GetBlue());
            }
        }
    }

    return aColor;
}

void RenderContext2::DrawPixel(Point const& rPt)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (!IsDeviceOutputNecessary() || !mbLineColor)
        return;

    Point aPt = ImplLogicToDevicePixel(rPt);

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (mbInitLineColor)
        InitLineColor();

    mpGraphics->DrawPixel(aPt.X(), aPt.Y(), *this);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawPixel(rPt);
}

void RenderContext2::DrawPixel(Point const& rPt, Color const& rColor)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary())
        return;

    Point aPt = ImplLogicToDevicePixel(rPt);

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    Color aColor;

    if (rColor.IsTransparent())
        aColor = rColor;
    else
        aColor = GetDrawModeLineColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    mpGraphics->DrawPixel(aPt.X(), aPt.Y(), aColor, *this);

    if (mpAlphaVDev)
    {
        Color aAlphaColor(255 - rColor.GetAlpha(), 255 - rColor.GetAlpha(),
                          255 - rColor.GetAlpha());
        mpAlphaVDev->DrawPixel(rPt, aAlphaColor);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
