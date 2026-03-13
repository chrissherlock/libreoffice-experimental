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

#include <vcl/deviceconcepts.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <devicedispatcher.hxx>
#include <salgdi.hxx>

#include <cassert>

template <typename PolyGenerator>
void OutputDevice::ImplDrawCurve(const tools::Rectangle& rRect, vcl::PrepareOutputFlags nFlags,
                                 bool bFill, PolyGenerator&& rPolyGen)
{
    if (!IsDeviceOutputNecessary())
        return;

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        if (!PrepareGraphicsOutput(nFlags) || !mpGraphics)
            return;

        tools::Rectangle aRect(LogicToDevicePixel(rRect));
        if (aRect.IsEmpty())
            return;

        // Lazily invoke the specific geometry math from the caller's lambda
        tools::Polygon aPoly = rPolyGen(aRect);

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            const tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelPolygon(aPoly, nWidth, bRTL, ImplIsAntiparallel());
        }

        vcl::rendercontext::PrimitiveRenderer::DrawPolygon(*mpGraphics, aPoly, bFill);
    });
}

void OutputDevice::DrawEllipse(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordEllipse(rRect);

    ImplDrawCurve(rRect,
                  (vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line
                   | vcl::PrepareOutputFlags::Fill),
                  mpGraphicsState->mbFillColor, [](const tools::Rectangle& rDeviceRect) {
                      return tools::Polygon(rDeviceRect.Center(), rDeviceRect.GetWidth() >> 1,
                                            rDeviceRect.GetHeight() >> 1);
                  });
}

void OutputDevice::DrawArc(const tools::Rectangle& rRect, const Point& rStartPt,
                           const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordArc(rRect, rStartPt, rEndPt);

    // Arc is just a line, so it doesn't need the Fill flag
    ImplDrawCurve(rRect, (vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line), false,
                  [&](const tools::Rectangle& rDeviceRect) {
                      return tools::Polygon(rDeviceRect, LogicToDevicePixel(rStartPt),
                                            LogicToDevicePixel(rEndPt), PolyStyle::Arc);
                  });
}

void OutputDevice::DrawPie(const tools::Rectangle& rRect, const Point& rStartPt,
                           const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordPie(rRect, rStartPt, rEndPt);

    ImplDrawCurve(rRect,
                  (vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line
                   | vcl::PrepareOutputFlags::Fill),
                  mpGraphicsState->mbFillColor, [&](const tools::Rectangle& rDeviceRect) {
                      return tools::Polygon(rDeviceRect, LogicToDevicePixel(rStartPt),
                                            LogicToDevicePixel(rEndPt), PolyStyle::Pie);
                  });
}

void OutputDevice::DrawChord(const tools::Rectangle& rRect, const Point& rStartPt,
                             const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordChord(rRect, rStartPt, rEndPt);

    ImplDrawCurve(rRect,
                  (vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line
                   | vcl::PrepareOutputFlags::Fill),
                  mpGraphicsState->mbFillColor, [&](const tools::Rectangle& rDeviceRect) {
                      return tools::Polygon(rDeviceRect, LogicToDevicePixel(rStartPt),
                                            LogicToDevicePixel(rEndPt), PolyStyle::Chord);
                  });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
