/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <tools/gen.hxx>

#include <vcl/dllapi.h>
#include <vcl/text/TextRenderContext.hxx>

class SalLayout;

namespace vcl::rendercontext
{
struct TextLineGeometry;
struct WaveLineGeometry;
}

namespace vcl::text
{
struct RotatedGeometry;
struct StrikeoutGeometry;
struct StraightLineMetrics;
struct TextDashSegment;

/**
 * TextRenderer: A stateless processor for text decorations and auxiliary
 * text rendering elements.
 *
 * This class decouples complex text-related drawing logic from the
 * OutputDevice state machine.
 */
class VCL_DLLPUBLIC TextRenderer
{
public:
    /** * Renders a pre-calculated layout for character-based strikeouts.
     * The layout generation (which requires heavy FontCache/Collection state)
     * is handled by OutputDevice, while this renderer purely applies context
     * state (like color) and dispatches to SalGraphics.
     */
    static void DrawStrikeoutCharLayout(const TextRenderContext& rCtx, SalLayout& rLayout,
                                        const Point& rOrigin, Color aColor);

    /** * Renders a calculated text decoration geometry (either a rotated polygon
     * or a straight rectangle) directly to the hardware graphics backend.
     */
    static void DrawTextDecoration(const TextRenderContext& rCtx,
                                   const vcl::text::RotatedGeometry& rDeviceGeo);

    /** * Renders geometric strikeout decorations (e.g., single, double, bold lines).
     */
    static void DrawStrikeoutLine(const TextRenderContext& rCtx,
                                  const vcl::rendercontext::TextLineGeometry& rGeo,
                                  const vcl::text::StrikeoutGeometry& rStrikeoutGeo);

    static void DrawStraightTextLine(const TextRenderContext& rCtx,
                                     const vcl::rendercontext::TextLineGeometry& rGeo,
                                     const vcl::text::StraightLineMetrics& rMetrics,
                                     const std::vector<vcl::text::TextDashSegment>& rDashSegments);

    static void DrawWaveHairline(const TextRenderContext& rCtx,
                                 const vcl::rendercontext::WaveLineGeometry& rGeo, Color aColor);

    static void DrawWaveLine(const TextRenderContext& rCtx,
                             const vcl::rendercontext::WaveLineGeometry& rGeo);

    static void DrawWaveLineBezier(SalGraphics& rGraphics, const Point& rStartPos,
                                   const Point& rEndPos, tools::Long nWaveHeight,
                                   double fOrientation, tools::Long nLineWidth,
                                   const Color& rLineColor, bool bPixelSnapHairline);

    static void DrawEmphasisMark(const TextRenderContext& rCtx, tools::Long nBaseX, tools::Long nX,
                                 tools::Long nY, tools::Long nOutOffX, tools::Long nOutOffY,
                                 const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                 const tools::Rectangle& rRect1, const tools::Rectangle& rRect2);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
