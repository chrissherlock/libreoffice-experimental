/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextRenderer.hxx>
#include <vcl/text/TextRenderContext.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <salgdi.hxx>
#include <sallayout.hxx>

#include <basegfx/point/b2dpoint.hxx>

namespace vcl::text
{
void TextRenderer::DrawStrikeoutCharLayout(const TextRenderContext& rCtx, SalLayout& rLayout,
                                           const Point& rOrigin, Color aColor)
{
    //. Stateless application of color to the graphics context.
    // We do not set "dirty" flags here; we just tell the hardware what to use.
    rCtx.rGraphics.SetTextColor(aColor);

    // Apply the pre-calculated origin.
    // OutputDevice has already handled Font Kerning, RTL Mirroring,
    // and Baseline Rotation calculations for this specific point.
    rLayout.DrawBase() = basegfx::B2DPoint(rOrigin.X(), rOrigin.Y());

    rLayout.DrawText(rCtx.rGraphics);
}

void TextRenderer::DrawTextDecoration(const TextRenderContext& rCtx,
                                      const vcl::text::RotatedGeometry& rDeviceGeo)
{
    if (rDeviceGeo.mbIsPolygon)
        vcl::rendercontext::PrimitiveRenderer::DrawPolygonGeometry(rCtx.rGraphics,
                                                                   rDeviceGeo.maPoly);
    else
        vcl::rendercontext::PrimitiveRenderer::DrawRect(rCtx.rGraphics, rDeviceGeo.maRect);
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
