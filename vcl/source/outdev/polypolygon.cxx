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
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/poly.hxx>

#include <vcl/deviceconcepts.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <devicedispatcher.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <memory>

void OutputDevice::DrawPolyPolygon(const tools::PolyPolygon& rPolyPoly)
{
    assert(!is_double_buffered_window());

    const sal_uInt16 nPoly = rPolyPoly.Count();
    if (!nPoly)
        return;

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(rPolyPoly);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill))
        return;

    bool bFill = IsFillColor();
    std::optional<vcl::rendercontext::StrokeAttributes> oStroke;

    if (IsLineColor())
    {
        oStroke.emplace();
        oStroke->fTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
    }

    tools::PolyPolygon aDevicePolyPoly = mpMapper->LogicToDevicePixel(rPolyPoly);
    const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
    const bool bAntiparallel = ImplIsAntiparallel();

    if (bRTL)
    {
        tools::Long nFrameWidth = vcl::DispatchDevice(
            *this, [](const auto& rDev) { return vcl::get_reference_width_v(rDev); });

        mpMapper->MirrorDevicePixelPolyPolygon(aDevicePolyPoly, nFrameWidth, bRTL, bAntiparallel);
    }

    vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(
        *mpGraphics, aDevicePolyPoly, bFill, oStroke ? &*oStroke : nullptr,
        basegfx::B2DHomMatrix(), // Identity matrix
        GetAntialiasing(), GetRasterOp());
}

void OutputDevice::DrawPolyPolygon(const basegfx::B2DPolyPolygon& rB2DPolyPoly)
{
    assert(!is_double_buffered_window());

    if (!rB2DPolyPoly.count())
        return;

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(tools::PolyPolygon(rB2DPolyPoly));

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill))
        return;

    bool bFill = IsFillColor();
    std::optional<vcl::rendercontext::StrokeAttributes> oStroke;

    if (IsLineColor())
    {
        oStroke.emplace();
        oStroke->fTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
    }

    basegfx::B2DHomMatrix aTransform = GetViewTransformation();
    vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(*mpGraphics, aTransform, rB2DPolyPoly,
                                                           bFill);

    if (oStroke)
    {
        for (sal_uInt32 i = 0; i < rB2DPolyPoly.count(); ++i)
            DrawPolyLine(rB2DPolyPoly.getB2DPolygon(i), *oStroke);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
