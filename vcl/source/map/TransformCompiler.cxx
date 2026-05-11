/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <TransformCompiler.hxx>
#include <TransformTypes.hxx>

#include "CoordinateMath.hxx"

#include <cmath>

namespace vcl
{
CompiledTransform TransformCompiler::Compile(const basegfx::B2DHomMatrix& rMat)
{
    CompiledTransform aTransform;
    aTransform.maMatrix = rMat;

    // Structural Invariants: Inherent to the affine model.
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Parallelism));
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Connectivity));

    // Orientation Invariant: Handedness check.
    // det = ad - bc. Epsilon-guarded for numerical stability.
    const double fDet = rMat.get(0, 0) * rMat.get(1, 1) - rMat.get(0, 1) * rMat.get(1, 0);
    if (fDet > 1e-12)
        aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Orientation));

    // Performance Taxonomy Classification
    if (rMat.isIdentity())
    {
        aTransform.meMode = TransformMode::Identity;
        aTransform.maContract.maPreserved.set(); // All invariants preserved
    }
    else if (IsAxisAligned(rMat))
    {
        // Rectilinear transforms preserve Axis Alignment and Orthogonality
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::AxisAlignment));
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::Orthogonality));

        if (IsPureTranslation(rMat))
        {
            const double fTx = rMat.get(0, 2);
            const double fTy = rMat.get(1, 2);
            constexpr double fEpsilon = 1e-9;

            // Fast path for integer-only translations (Legacy VCL optimization)
            if (std::abs(fTx - std::round(fTx)) < fEpsilon
                && std::abs(fTy - std::round(fTy)) < fEpsilon)
            {
                aTransform.meMode = TransformMode::Translation;
                aTransform.mnDeviceTx = static_cast<tools::Long>(std::round(fTx));
                aTransform.mnDeviceTy = static_cast<tools::Long>(std::round(fTy));
            }
            else
            {
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
        // SEMANTIC COLLAPSE: Transform is rotated or sheared.
        aTransform.meMode = TransformMode::AffineFallback;
    }

    return aTransform;
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
