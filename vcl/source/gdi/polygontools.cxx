/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <rtl/math.hxx>
#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/range/b2drange.hxx>

/**
     * Perform a safe approximation of a polygon from double-precision
     * coordinates to integer coordinates, to ensure that it has at least 2
     * pixels in both X and Y directions.
     */
tools::Polygon toPolygon(basegfx::B2DPolygon const& rPoly)
{
    basegfx::B2DRange aRange = rPoly.getB2DRange();
    double fW = aRange.getWidth(), fH = aRange.getHeight();
    if (0.0 < fW && 0.0 < fH && (fW <= 1.0 || fH <= 1.0))
    {
        // This polygon not empty but is too small to display.  Approximate it
        // with a rectangle large enough to be displayed.
        double nX = aRange.getMinX(), nY = aRange.getMinY();
        double nW = std::max<double>(1.0, rtl::math::round(fW));
        double nH = std::max<double>(1.0, rtl::math::round(fH));

        tools::Polygon aTarget;
        aTarget.Insert(0, Point(nX, nY));
        aTarget.Insert(1, Point(nX + nW, nY));
        aTarget.Insert(2, Point(nX + nW, nY + nH));
        aTarget.Insert(3, Point(nX, nY + nH));
        aTarget.Insert(4, Point(nX, nY));
        return aTarget;
    }
    return tools::Polygon(rPoly);
}

tools::PolyPolygon toPolyPolygon(basegfx::B2DPolyPolygon const& rPolyPoly)
{
    tools::PolyPolygon aTarget;
    for (auto const& rB2DPolygon : rPolyPoly)
        aTarget.Insert(toPolygon(rB2DPolygon));

    return aTarget;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
