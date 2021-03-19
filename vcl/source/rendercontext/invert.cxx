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

void RenderContext2::Invert(tools::Rectangle const& rRect, InvertFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary())
        return;

    tools::Rectangle aRect(ImplLogicToDevicePixel(rRect));

    if (aRect.IsEmpty())
        return;
    aRect.Justify();

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    mpGraphics->Invert(aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(), nSalFlags,
                       *this);
}

void RenderContext2::Invert(tools::Polygon const& rPoly, InvertFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary())
        return;

    sal_uInt16 nPoints = rPoly.GetSize();

    if (nPoints < 2)
        return;

    tools::Polygon aPoly(ImplLogicToDevicePixel(rPoly));

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    SalInvert nSalFlags = SalInvert::NONE;

    if (nFlags & InvertFlags::N50)
        nSalFlags |= SalInvert::N50;

    if (nFlags & InvertFlags::TrackFrame)
        nSalFlags |= SalInvert::TrackFrame;

    const Point* pPtAry = aPoly.GetConstPointAry();
    mpGraphics->Invert(nPoints, pPtAry, nSalFlags, *this);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
