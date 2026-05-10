/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cmath>
#include <tools/long.hxx>
#include <tools/gen.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/vector/b2dvector.hxx>

namespace vcl::detail
{
inline tools::Long RoundToLong(double fVal) { return static_cast<tools::Long>(std::llround(fVal)); }

inline double GetBasisVectorMagnitudeX(const basegfx::B2DHomMatrix& m)
{
    basegfx::B2DVector vx(1.0, 0.0);
    vx *= m;
    return vx.getLength();
}

inline double GetBasisVectorMagnitudeY(const basegfx::B2DHomMatrix& m)
{
    basegfx::B2DVector vy(0.0, 1.0);
    vy *= m;
    return vy.getLength();
}

inline tools::Rectangle RangeToVCLRect(const basegfx::B2DRange& rRange)
{
    return tools::Rectangle(RoundToLong(rRange.getMinX()), RoundToLong(rRange.getMinY()),
                            RoundToLong(rRange.getMaxX()) - 1, RoundToLong(rRange.getMaxY()) - 1);
}

} // namespace vcl::detail

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
