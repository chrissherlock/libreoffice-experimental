/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/numeric/ftools.hxx>

#include <vcl/TransformTypes.hxx>

#include <TransformCompiler.hxx>

#include <cmath>

namespace vcl
{
TransformPlan TransformCompiler::Compile(const basegfx::B2DHomMatrix& rMat)
{
    TransformPlan aTransform;
    aTransform.maMatrix = rMat;

    // Structural Invariants (Guaranteed across all affine transformations)
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Parallelism));
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Connectivity));

    // Orientation Invariant (Determinant check for scaling/reflection)
    const double fDet = rMat.get(0, 0) * rMat.get(1, 1) - rMat.get(0, 1) * rMat.get(1, 0);
    if (fDet > 1e-12)
        aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Orientation));

    // Extract translation components early for uniform downstream access
    const double fTx = rMat.get(0, 2);
    const double fTy = rMat.get(1, 2);
    aTransform.mnDeviceTx = basegfx::fround<tools::Long>(fTx);
    aTransform.mnDeviceTy = basegfx::fround<tools::Long>(fTy);

    // Performance Taxonomy Classification & Geometric Contracts
    if (rMat.isIdentity())
    {
        aTransform.meMode = TransformMode::Identity;
        aTransform.maContract.maPreserved.set(); // Identity preserves all geometric properties
        aTransform.mnDeviceTx = 0;
        aTransform.mnDeviceTy = 0;
        return aTransform;
    }

    // Full orthogonal rectilinear check:
    // Case 1: 0 or 180-degree variations (off-diagonal shear/rotation elements are zero)
    // Case 2: 90 or 270-degree variations (diagonal scale/reflection elements are zero)
    const bool bIsStandardAxisAligned
        = basegfx::fTools::equalZero(rMat.get(0, 1)) && basegfx::fTools::equalZero(rMat.get(1, 0));
    const bool bIsRotatedAxisAligned
        = basegfx::fTools::equalZero(rMat.get(0, 0)) && basegfx::fTools::equalZero(rMat.get(1, 1));

    const bool bIsAxisAligned = bIsStandardAxisAligned || bIsRotatedAxisAligned;

    if (bIsAxisAligned)
    {
        // Rectilinear configurations preserve axis orientation vectors and right angles
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::AxisAlignment));
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::Orthogonality));

        // Pure translation must be unrotated and unscaled (Case 1 with scale factors matching 1.0)
        const bool bIsPureTranslation = bIsStandardAxisAligned
                                        && basegfx::fTools::equal(rMat.get(0, 0), 1.0)
                                        && basegfx::fTools::equal(rMat.get(1, 1), 1.0);

        if (bIsPureTranslation)
        {
            constexpr double fPixelEpsilon = 1e-9;
            const bool bIsIntegerTranslation = std::abs(fTx - std::round(fTx)) < fPixelEpsilon
                                               && std::abs(fTy - std::round(fTy)) < fPixelEpsilon;

            // Mark as fast-path Translation mode ONLY if it aligns precisely to the pixel grid
            if (bIsIntegerTranslation)
            {
                aTransform.meMode = TransformMode::Translation;
            }
            else
            {
                // Fractional translations require full sub-pixel geometry adaptation passes
                aTransform.meMode = TransformMode::AxisAlignedAffine;
            }
        }
        else
        {
            aTransform.meMode = TransformMode::AxisAlignedAffine;
        }
    }
    else
    {
        // Complex transformation: Matrix contains arbitrary rotation or shearing components.
        aTransform.meMode = TransformMode::AffineFallback;
    }

    return aTransform;
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
