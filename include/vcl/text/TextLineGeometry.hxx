/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
#pragma once

#include <sal/types.h>
#include <tools/fontenum.hxx>
#include <tools/long.hxx>
#include <vcl/dllapi.h>
#include <vcl/vclenum.hxx>
#include <vcl/fntstyle.hxx>
#include <vector>

namespace vcl::text
{
// For Dashed/Dotted lines (X-axis segments)
struct TextDashSegment
{
    tools::Long nX;
    tools::Long nWidth;
};

// For Strikeout/Wave lines (Y-axis stacking)
struct TextDecorationSegment
{
    tools::Long nYOffset;
    tools::Long nHeight;
};

struct WaveLineGeometry
{
    std::vector<TextDecorationSegment> aSegments;
    tools::Long nLineWidth;
};

struct StrikeoutGeometry
{
    std::vector<TextDecorationSegment> aSegments;
};

struct TextLineGeometry
{
    tools::Long nLineWidth = 0;
    tools::Long nUnderlinePos1 = 0;
    tools::Long nUnderlinePos2 = 0;
    tools::Long nUnderlineWaveHeight = 0;
    tools::Long nOverlinePos1 = 0;
    tools::Long nOverlineWaveHeight = 0;
    tools::Long nStrikeoutPos1 = 0;
    tools::Long nStrikeoutPos2 = 0;
    bool bUnderlineIsWave = false;
    bool bOverlineIsWave = false;
    bool bStrikeoutIsChar = false;
};

class VCL_DLLPUBLIC TextLineRequest
{
public:
    tools::Long nDPIX = 0;
    tools::Long nDPIY = 0;
    FontLineStyle eUnderline = LINESTYLE_NONE;
    FontLineStyle eOverline = LINESTYLE_NONE;
    FontStrikeout eStrikeout = STRIKEOUT_NONE;
    bool bUnderlineAbove = false;
};

} // namespace vcl::text
