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

namespace vcl
{
/**
 * Ensures VCL Rectangle empty states (Width/Height <= 0) are preserved
 * after transformation math.
 */
inline void ApplyEmptyState(tools::Rectangle& rDest, const tools::Rectangle& rSrc)
{
    if (rSrc.IsWidthEmpty())
        rDest.SetWidthEmpty();

    if (rSrc.IsHeightEmpty())
        rDest.SetHeightEmpty();
}

/**
 * THE CANONICAL AFFINE BUILDER
 *
 * ARCHITECTURAL INVARIANT: Matrix Composition Order
 * basegfx::B2DHomMatrix applies operations via post-multiplication.
 * Therefore, the sequence: translate(A) -> scale(S) -> translate(B)
 * mathematically equates to the transformation:
 *
 *          P' = ((P + A) * S) + B
 *
 * Variables:
 *
 * P  (Point)      = The input coordinate
 * A  (LogicTx)    = Logical offset (applied before scaling)
 * S  (Scale)      = DPI, MapMode, and UI scaling factors
 * B  (PhysicalTx) = Absolute physical offset (applied after scaling)
 *
 * This proof ensures that:
 *
 * 1. Logical offsets (A) grow/shrink with the MapMode zoom level.
 * 2. Physical/Viewport offsets (B) remain constant screen pixels.
 * 3. The algebraic expansion [P*S + A*S + B] is consistent with legacy
 *    VCL manual matrix slot injections.
 */
inline basegfx::B2DHomMatrix BuildAffineMatrix(double fScaleX, double fScaleY, double fLogicTx,
                                               double fLogicTy, double fPhysicalTx,
                                               double fPhysicalTy)
{
    basegfx::B2DHomMatrix aMat;
    aMat.translate(fLogicTx, fLogicTy);
    aMat.scale(fScaleX, fScaleY);
    aMat.translate(fPhysicalTx, fPhysicalTy);
    return aMat;
}

inline bool IsPureTranslation(const basegfx::B2DHomMatrix& rMat)
{
    constexpr double fEpsilon = 1e-9;
    return std::abs(rMat.get(0, 0) - 1.0) < fEpsilon && std::abs(rMat.get(1, 1) - 1.0) < fEpsilon
           && std::abs(rMat.get(0, 1)) < fEpsilon && std::abs(rMat.get(1, 0)) < fEpsilon;
}

inline bool IsAxisAligned(const basegfx::B2DHomMatrix& rMat)
{
    constexpr double fEpsilon = 1e-9;
    return std::abs(rMat.get(0, 1)) < fEpsilon && std::abs(rMat.get(1, 0)) < fEpsilon;
}
} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
