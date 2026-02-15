/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>

class SalGraphics;
class CoordinateMapper;
class Point;
class Color;
class OutputDevice;

namespace vcl::rendercontext
{
class VCL_DLLPUBLIC PrimitiveRenderer
{
public:
    /** Renders a single pixel at the specified logical coordinates using the current line color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt);

    /** Renders a single pixel at the specified logical coordinates with a specific color. */
    static void DrawPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt,
                          const Color& rColor);

    /** Retrieves a single pixel color at the specified logical coordinates. */
    static Color GetPixel(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                          const OutputDevice* pOutDev, const Point& rLogicalPt);

    /** Renders a line between two logical points, with optional Anti-Aliasing. */
    static void DrawLine(SalGraphics& rGraphics, const CoordinateMapper& rMapper,
                         const OutputDevice* pOutDev, const Point& rLogicalStart,
                         const Point& rLogicalEnd, bool bTryAA, bool bPixelSnapHairline);
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
