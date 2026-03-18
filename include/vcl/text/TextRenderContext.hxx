
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <tools/degree.hxx>
#include <tools/long.hxx>

class SalGraphics;
class CoordinateMapper;
class LogicalFontInstance;

namespace vcl::text
{
struct TextRenderContext
{
    SalGraphics& rGraphics;
    const CoordinateMapper& rMapper;
    const LogicalFontInstance& rFontInstance;

    // Geometric State
    Degree10 nOrientation;
    bool bRTL;
    tools::Long nFrameWidth;
    bool bAntiparallel;

    // Drawing Attributes
    Color aTextColor;
    Color aLineColor;
    Color aFillColor;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
