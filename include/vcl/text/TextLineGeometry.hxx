/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
// Structure representing a segment of a line (decoration)
struct TextLineSegment
{
    tools::Long nX;
    tools::Long nWidth;
    tools::Long nYOffset = 0;
    tools::Long nHeight = 0;
};

struct WaveLineGeometry
{
    std::vector<TextLineSegment> aSegments;
    tools::Long nLineWidth;
};

struct StrikeoutGeometry
{
    std::vector<TextLineSegment> aSegments;
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

// Request structure specifically for line decorations (underline, etc.)
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
