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

#include <salgdi.hxx>

void RenderContext2::DrawGrid(tools::Rectangle const& rRect, Size const& rDist, DrawGridFlags nFlags)
{
    assert(!is_double_buffered_window());

    tools::Rectangle aDstRect(PixelToLogic(Point()), GetOutputSize());
    aDstRect.Intersection(rRect);

    if (aDstRect.IsEmpty())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    const tools::Long nDistX = std::max(rDist.Width(), tools::Long(1));
    const tools::Long nDistY = std::max(rDist.Height(), tools::Long(1));
    tools::Long nX = (rRect.Left() >= aDstRect.Left())
                         ? rRect.Left()
                         : (rRect.Left() + ((aDstRect.Left() - rRect.Left()) / nDistX) * nDistX);
    tools::Long nY = (rRect.Top() >= aDstRect.Top())
                         ? rRect.Top()
                         : (rRect.Top() + ((aDstRect.Top() - rRect.Top()) / nDistY) * nDistY);
    const tools::Long nRight = aDstRect.Right();
    const tools::Long nBottom = aDstRect.Bottom();
    const tools::Long nStartX = ImplLogicXToDevicePixel(nX);
    const tools::Long nEndX = ImplLogicXToDevicePixel(nRight);
    const tools::Long nStartY = ImplLogicYToDevicePixel(nY);
    const tools::Long nEndY = ImplLogicYToDevicePixel(nBottom);
    tools::Long nHorzCount = 0;
    tools::Long nVertCount = 0;

    std::vector<sal_Int32> aVertBuf;
    std::vector<sal_Int32> aHorzBuf;

    if ((nFlags & DrawGridFlags::Dots) || (nFlags & DrawGridFlags::HorzLines))
    {
        aVertBuf.resize(aDstRect.GetHeight() / nDistY + 2);
        aVertBuf[nVertCount++] = nStartY;
        while ((nY += nDistY) <= nBottom)
        {
            aVertBuf[nVertCount++] = ImplLogicYToDevicePixel(nY);
        }
    }

    if ((nFlags & DrawGridFlags::Dots) || (nFlags & DrawGridFlags::VertLines))
    {
        aHorzBuf.resize(aDstRect.GetWidth() / nDistX + 2);
        aHorzBuf[nHorzCount++] = nStartX;
        while ((nX += nDistX) <= nRight)
        {
            aHorzBuf[nHorzCount++] = ImplLogicXToDevicePixel(nX);
        }
    }

    if (mbInitLineColor)
        InitLineColor();

    if (mbInitFillColor)
        InitFillColor();

    const bool bOldMap = mbMap;
    EnableMapMode(false);

    if (nFlags & DrawGridFlags::Dots)
    {
        for (tools::Long i = 0; i < nVertCount; i++)
        {
            for (tools::Long j = 0, Y = aVertBuf[i]; j < nHorzCount; j++)
            {
                mpGraphics->DrawPixel(aHorzBuf[j], Y, *this);
            }
        }
    }
    else
    {
        if (nFlags & DrawGridFlags::HorzLines)
        {
            for (tools::Long i = 0; i < nVertCount; i++)
            {
                nY = aVertBuf[i];
                mpGraphics->DrawLine(nStartX, nY, nEndX, nY, *this);
            }
        }

        if (nFlags & DrawGridFlags::VertLines)
        {
            for (tools::Long i = 0; i < nHorzCount; i++)
            {
                nX = aHorzBuf[i];
                mpGraphics->DrawLine(nX, nStartY, nX, nEndY, *this);
            }
        }
    }

    EnableMapMode(bOldMap);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawGrid(rRect, rDist, nFlags);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
