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
#include <devicedispatcher.hxx>
#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawCheckered(const Point& rPos, const Size& rSize, sal_uInt32 nLen,
                                 Color aStart, Color aEnd)
{
    assert(!is_double_buffered_window());

    if (rSize.IsEmpty() || nLen == 0)
        return;

    maRecorder.RecordCheckered(rPos, rSize, nLen, aStart, aEnd);

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        if (!FlushGraphicsState())
            return;

        // Map the bounding box to device pixels once
        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(tools::Rectangle(rPos, rSize));

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nFrameWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, ImplIsAntiparallel());
        }

        auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR);
        SetLineColor();
        if (mbLineColorDirty)
            InitLineColor();

        vcl::rendercontext::PrimitiveRenderer::DrawCheckered(*mpGraphics, aDeviceRect, nLen, aStart,
                                                             aEnd);
    });
}

void OutputDevice::DrawGrid(const tools::Rectangle& rRect, const Size& rStep, DrawGridFlags nFlags)
{
    assert(!is_double_buffered_window());

    // avoid infinite loops or empty renders
    if (rRect.IsEmpty() || rStep.Width() <= 0 || rStep.Height() <= 0)
        return;

    maRecorder.RecordGrid(rRect, rStep, nFlags, GetLineColor(), GetFillColor());

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line
                                   | vcl::PrepareOutputFlags::Fill)
            || !mpGraphics)
        {
            return;
        }

        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);
        Size aDeviceStep = mpMapper->LogicToDevicePixel(rStep);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nWidth, bRTL, ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawGrid(*mpGraphics, aDeviceRect, aDeviceStep,
                                                        nFlags);
    });
}

void OutputDevice::DrawGridOfCrosses(const tools::Rectangle& rGridArea, const Size& rGridDistance,
                                     const tools::Rectangle& rDrawingArea)
{
    assert(!is_double_buffered_window());

    if (rDrawingArea.IsEmpty() || rGridArea.IsEmpty())
        return;

    maRecorder.RecordGridOfCrosses(rGridArea, rGridDistance, rDrawingArea, GetLineColor());

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line)
            || !mpGraphics)
        {
            return;
        }

        tools::Rectangle aDeviceGridArea = mpMapper->LogicToDevicePixel(rGridArea);
        tools::Rectangle aDeviceDrawingArea = mpMapper->LogicToDevicePixel(rDrawingArea);
        Size aDeviceGridDistance = mpMapper->LogicToDevicePixel(rGridDistance);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nFrameWidth = vcl::get_reference_width_v(rConcrete);

            mpMapper->MirrorDevicePixelRect(aDeviceGridArea, nFrameWidth, bRTL,
                                            ImplIsAntiparallel());
            mpMapper->MirrorDevicePixelRect(aDeviceDrawingArea, nFrameWidth, bRTL,
                                            ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawGridOfCrosses(
            *mpGraphics, aDeviceGridArea, aDeviceGridDistance, aDeviceDrawingArea);
    });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
