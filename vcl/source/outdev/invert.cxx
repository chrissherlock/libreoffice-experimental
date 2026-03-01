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

void OutputDevice::Invert(const tools::Polygon& rPoly, InvertFlags nFlags)
{
    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip) && mpGraphics)
    {
        tools::Polygon aDevicePoly = mpMapper->LogicToDevicePixel(rPoly);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);

        if (bRTL)
        {
            tools::Long nFrameWidth
                = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
            mpMapper->MirrorDevicePixelPolygon(aDevicePoly, nFrameWidth, bRTL,
                                               ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::Invert(*mpGraphics, aDevicePoly, nFlags);
    }
}

void OutputDevice::Invert(const tools::Rectangle& rRect, InvertFlags nFlags)
{
    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip) && mpGraphics)
    {
        tools::Rectangle aDeviceRect = mpMapper->LogicToDevicePixel(rRect);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);

        if (bRTL)
        {
            tools::Long nFrameWidth
                = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();
            mpMapper->MirrorDevicePixelRect(aDeviceRect, nFrameWidth, bRTL, ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::Invert(*mpGraphics, aDeviceRect, nFlags);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
