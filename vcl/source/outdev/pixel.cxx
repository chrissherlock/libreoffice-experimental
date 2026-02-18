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
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright
 * ownership. The ASF licenses this file to you under the Apache
 * License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/virdev.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <ClippingController.hxx>
#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include <cassert>

Color OutputDevice::GetPixel(const Point& rPoint) const
{
    if (!mpGraphics && !AcquireGraphics())
        return Color();

    assert(mpGraphics);

    if (mpClippingController->IsDirty())
        const_cast<OutputDevice*>(this)->InitClipRegion();

    if (IsOutputCulled())
        return Color();

    const tools::Long nX = LogicXToDevicePixel(rPoint.X());
    const tools::Long nY = LogicYToDevicePixel(rPoint.Y());

    return mpGraphics->GetPixel(nX, nY, *this);
}

void OutputDevice::DrawPixel(const Point& rPt)
{
    maRecorder.RecordPixel(rPt);

    if (PrepareGraphicsOutput(false) && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::DrawPixel(*mpGraphics, *mpMapper, this, rPt);
}

void OutputDevice::DrawPixel(const Point& rPt, const Color& rColor)
{
    maRecorder.RecordPixel( rPt, rColor );

    if (PrepareGraphicsOutput(false) && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::DrawPixel(*mpGraphics, *mpMapper, this, rPt, rColor);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
