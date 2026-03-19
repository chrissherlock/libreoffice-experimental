/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/TextDecorationMetrics.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextRenderer.hxx>
#include <vcl/text/TextRenderContext.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/rendercontext/TextLineGeometry.hxx>
#include <vcl/rendercontext/WaveLineGeometry.hxx>

#include <CoordinateMapper.hxx>
#include <salgdi.hxx>
#include <sallayout.hxx>

#include <basegfx/point/b2dpoint.hxx>

namespace vcl::text
{
void TextRenderer::DrawStrikeoutCharLayout(const TextRenderContext& rCtx, SalLayout& rLayout,
                                           const Point& rOrigin, Color aColor)
{
    // Stateless application of color to the graphics context.
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

void TextRenderer::DrawStrikeoutLine(const TextRenderContext& rCtx,
                                     const vcl::rendercontext::TextLineGeometry& rGeo, // <-- FIXED
                                     const vcl::text::StrikeoutGeometry& rStrikeoutGeo)
{
    for (const auto& rSeg : rStrikeoutGeo.aSegments)
    {
        auto aTextGeo = vcl::text::TextGeometry::GetRotatedGeometry(
            rGeo.maOrigin,
            tools::Rectangle(Point(rGeo.mnDistX, rSeg.nYOffset), Size(rGeo.mfWidth, rSeg.nHeight)),
            rCtx.nOrientation);

        if (rCtx.bRTL)
        {
            if (aTextGeo.mbIsPolygon)
                rCtx.rMapper.MirrorDevicePixelPolygon(aTextGeo.maPoly, rCtx.nFrameWidth, rCtx.bRTL,
                                                      rCtx.bAntiparallel);
            else
                rCtx.rMapper.MirrorDevicePixelRect(aTextGeo.maRect, rCtx.nFrameWidth, rCtx.bRTL,
                                                   rCtx.bAntiparallel);
        }

        DrawTextDecoration(rCtx, aTextGeo);
    }
}

void TextRenderer::DrawStraightTextLine(
    const TextRenderContext& rCtx, const vcl::rendercontext::TextLineGeometry& rGeo,
    const vcl::text::StraightLineMetrics& rMetrics,
    const std::vector<vcl::text::TextDashSegment>& rDashSegments)
{
    const tools::Long nLeft = rGeo.mnDistX;

    auto fnDrawDecoration
        = [&](tools::Long nPos, tools::Long nHeight, tools::Long nSegLeft, tools::Long nSegWidth) {
              auto aTextGeo = vcl::text::TextGeometry::GetRotatedGeometry(
                  rGeo.maOrigin,
                  tools::Rectangle(Point(nLeft + nSegLeft, nPos), Size(nSegWidth, nHeight)),
                  rCtx.nOrientation);

              if (rCtx.bRTL)
              {
                  if (aTextGeo.mbIsPolygon)
                      rCtx.rMapper.MirrorDevicePixelPolygon(aTextGeo.maPoly, rCtx.nFrameWidth,
                                                            rCtx.bRTL, rCtx.bAntiparallel);
                  else
                      rCtx.rMapper.MirrorDevicePixelRect(aTextGeo.maRect, rCtx.nFrameWidth,
                                                         rCtx.bRTL, rCtx.bAntiparallel);
              }

              DrawTextDecoration(rCtx, aTextGeo);
          };

    switch (rMetrics.eUnderline)
    {
        case LINESTYLE_SINGLE:
        case LINESTYLE_BOLD:
            fnDrawDecoration(rMetrics.nLinePos, rMetrics.nLineHeight, 0, rGeo.mfWidth);
            break;

        case LINESTYLE_DOUBLE:
            fnDrawDecoration(rMetrics.nLinePos, rMetrics.nLineHeight, 0, rGeo.mfWidth);
            fnDrawDecoration(rMetrics.nLinePos2, rMetrics.nLineHeight, 0, rGeo.mfWidth);
            break;

        default:
        {
            for (const auto& rSeg : rDashSegments)
            {
                fnDrawDecoration(rMetrics.nLinePos, rMetrics.nLineHeight, rSeg.nX, rSeg.nWidth);
            }
        }
        break;
    }
}

void TextRenderer::DrawWaveHairline(const TextRenderContext& rCtx,
                                    const vcl::rendercontext::WaveLineGeometry& rGeo, Color aColor)
{
    // Apply the stateless color directly to the hardware backend
    rCtx.rGraphics.SetLineColor(aColor);

    const Point aLineStart = rGeo.GetLineStart();
    const Point aLineEnd = rGeo.GetLineEnd();

    tools::Long nX1 = aLineStart.X();
    tools::Long nX2 = aLineEnd.X();

    if (rCtx.bRTL)
    {
        nX1 = rCtx.nFrameWidth - nX1;
        nX2 = rCtx.nFrameWidth - nX2;
    }

    rCtx.rGraphics.drawLine(nX1, aLineStart.Y(), nX2, aLineEnd.Y());
}

void TextRenderer::DrawWaveLine(const TextRenderContext& rCtx,
                                const vcl::rendercontext::WaveLineGeometry& rGeo)
{
    for (Point aDrawPt : rGeo.GetRegion())
    {
        if (rGeo.mnOrientation)
            rGeo.maBase.RotateAround(aDrawPt, rGeo.mnOrientation);

        if (rGeo.mbDrawAsRect)
        {
            tools::Long nX = aDrawPt.X();

            if (rCtx.bRTL)
                nX = rCtx.nFrameWidth - nX - rGeo.maWavePixelSize.Width();

            rCtx.rGraphics.drawRect(nX, aDrawPt.Y(), rGeo.maWavePixelSize.Width(),
                                    rGeo.maWavePixelSize.Height());
        }
        else
        {
            rCtx.rGraphics.drawPixel(aDrawPt.X(), aDrawPt.Y());
        }
    }
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
