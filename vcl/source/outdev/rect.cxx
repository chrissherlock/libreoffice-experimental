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

#include <devicedispatcher.hxx>
#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawBorder(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordBorder(rRect, GetLineColor());

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](auto& rConcrete) {
        using DeviceType = std::remove_cvref_t<decltype(rConcrete)>;

        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line)
            || !mpGraphics)
        {
            return;
        }

        if constexpr (vcl::HighContrastOutput<DeviceType>)
        {
            // High-contrast requirement: Force black and use a closed Rect path
            const Color aOldColor = rConcrete.GetLineColor();
            rConcrete.SetLineColor(COL_BLACK);

            // Printers benefit from DrawRect as it's a single atomic vector action
            rConcrete.DrawRect(rRect);

            rConcrete.SetLineColor(aOldColor);
        }
        else
        {
            // Standard Visual path: 4-line primitive for screens/UI
            tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);

            const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
            if (bRTL)
            {
                tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
                mpMapper->MirrorDevicePixelRect(aDeviceRect, nWidth, bRTL, ImplIsAntiparallel());
            }

            vcl::rendercontext::PrimitiveRenderer::DrawBorder(*mpGraphics, aDeviceRect);
        }
    });
}

void OutputDevice::DrawRect(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRect(rRect, GetLineColor(), GetFillColor());

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip |
                                   vcl::PrepareOutputFlags::Line |
                                   vcl::PrepareOutputFlags::Fill) || !mpGraphics)
        {
            return;
        }

        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nWidth, bRTL, ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawRect(*mpGraphics, aDeviceRect);
    });
}

void OutputDevice::DrawRoundedRect(const tools::Rectangle& rRect,
                                   sal_uLong nHorzRound, sal_uLong nVertRound)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRoundedRect(rRect, nHorzRound, nVertRound, GetLineColor(), GetFillColor());

    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {

        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip |
                                   vcl::PrepareOutputFlags::Line |
                                   vcl::PrepareOutputFlags::Fill) || !mpGraphics)
        {
            return;
        }

        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);
        Size aPixelRound = mpMapper->LogicToDevicePixel(Size(nHorzRound, nVertRound));

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nWidth, bRTL, ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawRoundedRect(
            *mpGraphics,
            aDeviceRect,
            aPixelRound.Width(),
            aPixelRound.Height(),
            IsFillColor());
    });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
