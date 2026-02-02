/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>

#include <tools/degree.hxx>
#include <tools/long.hxx>

#include <functional>
#include <vector>

namespace tools
{
class PolyPolygon;
class Rect;
class Line;
}

class Point;
class Size;
class Hatch;

namespace vcl
{
class VCL_DLLPUBLIC HatchProcessor
{
public:
    using Callback = std::function<void(const Point&, const Point&)>;

    /**
     * Decomposes a hatch pattern into a series of line segments.
     * Handles curves by adaptive subdivision.
     */
    static void Process(const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch,
                        const tools::Rectangle& rRect, const Point& rRefPoint,
                        tools::Long nLogPixelWidth, tools::Long nWidth, Callback callback);

private:
    static void calcHatchValues(const tools::Rectangle& rRect, tools::Long nDist, Degree10 nAngle10,
                                const Point& rRefPoint, Point& rPt1, Point& rPt2, Size& rInc,
                                Point& rEndPt1);

    static void collectHatchIntersections(const tools::Line& rLine,
                                          const tools::PolyPolygon& rPolyPoly,
                                          std::vector<Point>& rPtBuffer);

    static bool hasSaneNSteps(const Point& rPt1, const Point& rEndPt1, const Size& rInc);
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
