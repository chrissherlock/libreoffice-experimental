/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <tools/fontenum.hxx>

#include <vcl/dllapi.h>
#include <vcl/fntstyle.hxx>

#include <vector>

class FontMetricData;

namespace vcl::text
{
struct SAL_DLLPUBLIC WaveLineSegment
{
    tools::Long nYOffset;
    tools::Long nHeight;
};

struct SAL_DLLPUBLIC WaveLineGeometry
{
    tools::Long nLineWidth;
    std::vector<WaveLineSegment> aSegments;
};

struct SAL_DLLPUBLIC StrikeoutSegment
{
    tools::Long nYOffset;
    tools::Long nHeight;
};

struct SAL_DLLPUBLIC StrikeoutGeometry
{
    std::vector<StrikeoutSegment> aSegments;
};

struct SAL_DLLPUBLIC TextLineSegment
{
    tools::Long nX;
    tools::Long nWidth;
};

struct TextLineRequest
{
    FontLineStyle eUnderline;
    FontLineStyle eOverline;
    FontStrikeout eStrikeout;
    bool bUnderlineAbove;
    sal_Int32 nDPIX;
    sal_Int32 nDPIY;
};

struct TextLineGeometry
{
    tools::Long nUnderlinePos1 = 0;
    tools::Long nUnderlinePos2 = 0;
    tools::Long nOverlinePos1 = 0;
    tools::Long nOverlinePos2 = 0;
    tools::Long nStrikeoutPos1 = 0;
    tools::Long nStrikeoutPos2 = 0;
    tools::Long nLineWidth = 0;
    tools::Long nUnderlineWaveHeight = 0;
    tools::Long nOverlineWaveHeight = 0;
    bool bUnderlineIsWave = false;
    bool bOverlineIsWave = false;
    bool bStrikeoutIsChar = false;
};

class VCL_DLLPUBLIC TextDecorator
{
public:
    static constexpr tools::Long nMaxSmallWavelineHeight = 3;

    static WaveLineGeometry CalculateWaveLineGeometry(const FontMetricData& rMetric,
                                                      FontLineStyle eStyle, bool bIsAbove,
                                                      tools::Long nDistY, tools::Long nDPIX,
                                                      tools::Long nDPIY);

    static StrikeoutGeometry CalculateStrikeoutGeometry(const FontMetricData& rMetric,
                                                        FontStrikeout eStrikeout,
                                                        tools::Long nDistY);

    static std::vector<TextLineSegment>
    CalculateTextLineSegments(tools::Long nWidth, FontLineStyle eStyle, tools::Long nLineHeight,
                              tools::Long nDPIX, tools::Long nDPIY);

    static TextLineGeometry GetTextLineGeometry(const TextLineRequest& rReq,
                                                const FontMetricData& rMetric);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
