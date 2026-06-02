/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/long.hxx>

#include <vcl/TransformTypes.hxx>

#include <bitset>

namespace vcl
{
/**
 * IMMUTABLE execution artifact representing a resolved coordinate transformation.
 * This satisfies the strict separation between the compilation phase and execution phase.
 */
struct TransformPlan
{
    basegfx::B2DHomMatrix maMatrix;
    TransformMode meMode = TransformMode::Identity;
    tools::Long mnDeviceTx = 0;
    tools::Long mnDeviceTy = 0;

    struct
    {
        std::bitset<static_cast<size_t>(GeometryInvariant::COUNT)> maPreserved;
    } maContract;

    bool PreservesAxisAlignment() const
    {
        return maContract.maPreserved.test(static_cast<size_t>(GeometryInvariant::AxisAlignment));
    }

    TransformPlan compose(const TransformPlan& rNextPlan) const
    {
        // Fast paths for Identity
        if (this->meMode == TransformMode::Identity)
            return rNextPlan;
        if (rNextPlan.meMode == TransformMode::Identity)
            return *this;

        TransformPlan aComposite;

        // Matrix composition: basegfx uses column vectors (M * p).
        // To apply `this` THEN `rNextPlan`, we must multiply `rNextPlan * this`
        aComposite.maMatrix = rNextPlan.maMatrix * this->maMatrix;

        // Combine Device Translation offsets
        aComposite.mnDeviceTx = this->mnDeviceTx + rNextPlan.mnDeviceTx;
        aComposite.mnDeviceTy = this->mnDeviceTy + rNextPlan.mnDeviceTy;

        // Determine new mode
        if (this->meMode == TransformMode::AffineFallback
            || rNextPlan.meMode == TransformMode::AffineFallback)
            aComposite.meMode = TransformMode::AffineFallback;
        else if (this->meMode == TransformMode::Translation
                 && rNextPlan.meMode == TransformMode::Translation)
            aComposite.meMode = TransformMode::Translation;
        else
            aComposite.meMode = TransformMode::AxisAlignedAffine;

        // Intersect the geometric contracts
        aComposite.maContract.maPreserved
            = this->maContract.maPreserved & rNextPlan.maContract.maPreserved;

        return aComposite;
    }

    template <typename T> auto apply(const T& rGeom) const;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
