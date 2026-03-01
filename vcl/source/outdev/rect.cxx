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

void OutputDevice::DrawBorder(tools::Rectangle aBorderRect)
{
    sal_uInt16 nPixel = static_cast<sal_uInt16>(PixelToLogic(Size(1, 1)).Width());

    aBorderRect.AdjustLeft(nPixel);
    aBorderRect.AdjustTop(nPixel);

    SetLineColor(COL_LIGHTGRAY);
    DrawRect(aBorderRect);

    aBorderRect.AdjustLeft(-nPixel);
    aBorderRect.AdjustTop(-nPixel);
    aBorderRect.AdjustRight(-nPixel);
    aBorderRect.AdjustBottom(-nPixel);
    SetLineColor(COL_GRAY);

    DrawRect(aBorderRect);
}

void OutputDevice::DrawRect(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRect(rRect);

    if (PrepareGraphicsOutput() && mpGraphics)
    {
        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);

        bool bRTL = IsRTLEnabled() || (mpGraphics && (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
        bool bAntiparallel = ImplIsAntiparallel();
        tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();

        mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, bAntiparallel);

        vcl::rendercontext::PrimitiveRenderer::DrawRect(*mpGraphics, aDeviceRect);
    }
}

void OutputDevice::DrawRoundedRect(const tools::Rectangle& rRect,
                                   sal_uLong nHorzRound, sal_uLong nVertRound)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRoundRect(rRect, nHorzRound, nVertRound);

    if (PrepareGraphicsOutput() && mpGraphics)
    {
        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);
        sal_uLong nHorzRoundPixel = mpMapper->LogicWidthToDevicePixel(nHorzRound);
        sal_uLong nVertRoundPixel = mpMapper->LogicHeightToDevicePixel(nVertRound);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        const bool bAntiparallel = ImplIsAntiparallel();
        const tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();

        mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, bAntiparallel);

        vcl::rendercontext::PrimitiveRenderer::DrawRoundedRect(*mpGraphics, aDeviceRect,
                                                               nHorzRoundPixel, nVertRoundPixel, mpGraphicsState->mbFillColor);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
