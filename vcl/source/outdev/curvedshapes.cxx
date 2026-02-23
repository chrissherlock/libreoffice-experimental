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

#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawEllipse(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordEllipse(rRect);

    if (!PrepareGraphicsOutput())
        return;

    tools::Rectangle aRect(LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    vcl::rendercontext::PrimitiveRenderer::DrawEllipse(*this, aRect, mpGraphicsState->mbFillColor);
}

void OutputDevice::DrawArc(const tools::Rectangle& rRect, const Point& rStartPt,
                           const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordArc(rRect, rStartPt, rEndPt);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line))
        return;

    tools::Rectangle aRect(LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    const Point aStart(LogicToDevicePixel(rStartPt));
    const Point aEnd(LogicToDevicePixel(rEndPt));

    vcl::rendercontext::PrimitiveRenderer::DrawArc(*this, aRect, aStart, aEnd);
}

void OutputDevice::DrawPie(const tools::Rectangle& rRect, const Point& rStartPt,
                           const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordPie(rRect, rStartPt, rEndPt);

    if (!PrepareGraphicsOutput())
        return;

    tools::Rectangle aRect(LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    const Point aStart(LogicToDevicePixel(rStartPt));
    const Point aEnd(LogicToDevicePixel(rEndPt));

    vcl::rendercontext::PrimitiveRenderer::DrawPie(*this, aRect, aStart, aEnd,
                                                   mpGraphicsState->mbFillColor);
}

void OutputDevice::DrawChord(const tools::Rectangle& rRect, const Point& rStartPt,
                             const Point& rEndPt)
{
    assert(!is_double_buffered_window());
    maRecorder.RecordChord(rRect, rStartPt, rEndPt);

    if (!PrepareGraphicsOutput())
        return;

    tools::Rectangle aRect(LogicToDevicePixel(rRect));
    if (aRect.IsEmpty())
        return;

    const Point aStart(LogicToDevicePixel(rStartPt));
    const Point aEnd(LogicToDevicePixel(rEndPt));

    vcl::rendercontext::PrimitiveRenderer::DrawChord(*this, aRect, aStart, aEnd,
                                                     mpGraphicsState->mbFillColor);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
