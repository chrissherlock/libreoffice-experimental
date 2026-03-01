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

#include <sal/types.h>
#include <tools/poly.hxx>
#include <tools/helpers.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/rendercontext/DrawGridFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawCheckered(const Point& rPos, const Size& rSize, sal_uInt32 nLen,
                                 Color aStart, Color aEnd)
{
    assert(!is_double_buffered_window());

    const sal_uInt32 nMaxX(rPos.X() + rSize.Width());
    const sal_uInt32 nMaxY(rPos.Y() + rSize.Height());

    auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR);
    SetLineColor();

    for (sal_uInt32 x(0), nX(rPos.X()); nX < nMaxX; x++, nX += nLen)
    {
        const sal_uInt32 nRight(std::min(nMaxX, nX + nLen));

        for (sal_uInt32 y(0), nY(rPos.Y()); nY < nMaxY; y++, nY += nLen)
        {
            const sal_uInt32 nBottom(std::min(nMaxY, nY + nLen));

            SetFillColor(((x & 0x0001) ^ (y & 0x0001)) ? aStart : aEnd);
            DrawRect(tools::Rectangle(nX, nY, nRight, nBottom));
        }
    }
}

void OutputDevice::DrawGrid(const tools::Rectangle& rRect, const Size& rDist, DrawGridFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    tools::Rectangle aDstRect(PixelToLogic(Point()), GetOutputSize());
    aDstRect.Intersection(rRect);

    if (aDstRect.IsEmpty())
        return;

    const bool bOldMap = mpMapper->IsMapModeEnabled();
    comphelper::ScopeGuard aMapGuard([this, bOldMap]() { this->EnableMapMode(bOldMap); });

    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line)
        && mpGraphics)
    {
        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);
        tools::Rectangle aDeviceDstRect = mpMapper->LogicToDevicePixel(aDstRect);
        Size aDeviceDist = mpMapper->LogicToDevicePixel(rDist);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nFrameWidth
                = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, ImplIsAntiparallel());
            mpMapper->MirrorDevicePixelRect(aDeviceDstRect, nFrameWidth, bRTL,
                                            ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawGrid(*mpGraphics, aDeviceRect, aDeviceDstRect,
                                                        aDeviceDist, nFlags);
    }
}

void OutputDevice::DrawGridOfCrosses(const tools::Rectangle& rGridArea, const Size& rGridDistance,
                                     const tools::Rectangle& rDrawingArea)
{
    assert(!is_double_buffered_window());

    if (rDrawingArea.IsEmpty() || rGridArea.IsEmpty())
        return;

    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line)
        && mpGraphics)
    {
        tools::Rectangle aDeviceGridArea = mpMapper->LogicToDevicePixel(rGridArea);
        tools::Rectangle aDeviceDrawingArea = mpMapper->LogicToDevicePixel(rDrawingArea);
        Size aDeviceGridDistance = mpMapper->LogicToDevicePixel(rGridDistance);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nFrameWidth
                = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();

            // Mirror both the boundary and the active drawing area
            mpMapper->MirrorDevicePixelRect(aDeviceGridArea, nFrameWidth, bRTL,
                                            ImplIsAntiparallel());
            mpMapper->MirrorDevicePixelRect(aDeviceDrawingArea, nFrameWidth, bRTL,
                                            ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawGridOfCrosses(
            *mpGraphics, aDeviceGridArea, aDeviceGridDistance, aDeviceDrawingArea);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
