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

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <memory>

void OutputDevice::DrawPolygon( const basegfx::B2DPolygon& rB2DPolygon)
{
    assert(!is_double_buffered_window());

    // AW: Do NOT paint empty polygons
    if(rB2DPolygon.count())
    {
        basegfx::B2DPolyPolygon aPP( rB2DPolygon );
        DrawPolyPolygon( aPP );
    }
}

/** * Creates a StrokeAttributes object for a default hairline.
 * Returns a std::optional to handle the "pStroke = nullptr" logic
 * without manual pointer management in the caller.
 */
static std::optional<vcl::rendercontext::StrokeAttributes> lcl_CreateDefaultHairline(bool bIsLineColor)
{
    if (!bIsLineColor)
        return std::nullopt;

    vcl::rendercontext::StrokeAttributes aStroke;
    aStroke.fWidth = 0.0; // Hairline fallback
    aStroke.eJoin = basegfx::B2DLineJoin::NONE;
    aStroke.eCap = css::drawing::LineCap_BUTT;
    aStroke.fMiterMinimumAngle = basegfx::deg2rad(15.0);
    return aStroke;
}

static double lcl_GetLineTransparency(bool bIsLineColor, const Color& rColor)
{
    if (!bIsLineColor)
        return 0.0;

    return (255.0 - rColor.GetAlpha()) / 255.0;
}

void OutputDevice::DrawPolygon( const tools::Polygon& rPoly )
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolygon(rPoly);

    if (rPoly.GetSize() < 2 || !IsDeviceOutputNecessary())
        return;

    auto oStroke = lcl_CreateDefaultHairline(IsLineColor());
    vcl::rendercontext::StrokeAttributes* pStroke = oStroke ? &*oStroke : nullptr;

    const double fLineTransparency = lcl_GetLineTransparency(IsLineColor(), GetLineColor());
    vcl::rendercontext::PrimitiveRenderer::DrawPolygon(*this, rPoly, IsFillColor(), pStroke, fLineTransparency);
}

void OutputDevice::ImplDrawClippedPolygon(const tools::Polygon& rPoly, const tools::PolyPolygon& rClipPolyPoly)
{
    // Handle clipping via intersection then dispatch
    tools::PolyPolygon aClipped;
    tools::PolyPolygon(rPoly).GetIntersection(rClipPolyPoly, aClipped);

    vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygonGeometry(*this, aClipped);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
