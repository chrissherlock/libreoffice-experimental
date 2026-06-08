/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/polygon/b2dpolypolygoncutter.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/utils/canvastools.hxx>

#include <vcl/ClippingController.hxx>
#include <vcl/CoordinateMapper.hxx>

namespace vcl
{
ClippingController::ClippingController()
    : mbIsRectangular(true)
    , mbHasClipRegion(false)
    , mnLastSyncEpoch(0)
{
}

bool ClippingController::IsCulled(const basegfx::B2DRange& rDeviceRange) const
{
    if (!mbHasClipRegion)
        return false;

    // Fast path: O(1) Bounding box overlap
    if (mbIsRectangular)
        return !maLogicRange.overlaps(rDeviceRange);

    // Robust path: Conservative culling against the complex polygon's bounds
    // FIXED: Use .getB2DRange()
    return !maLogicPolyPoly.getB2DRange().overlaps(rDeviceRange);
}

void ClippingController::SetNoClipRegion()
{
    maLogicPolyPoly.clear();
    mbHasClipRegion = false;
    mbIsRectangular = true;
    mnLastSyncEpoch = 0; // Force sync
}

void ClippingController::ModifyClip(const basegfx::B2DPolyPolygon& rPolyPoly, ClipOp eOp)
{
    if (eOp == ClipOp::Set || !mbHasClipRegion)
    {
        maLogicPolyPoly = rPolyPoly;
        mbHasClipRegion = true;
    }
    else
    {
        maLogicPolyPoly = basegfx::utils::solvePolygonOperationAnd(maLogicPolyPoly, rPolyPoly);
    }

    mbIsRectangular = basegfx::utils::isRectangle(maLogicPolyPoly);

    if (mbIsRectangular)
        maLogicRange = maLogicPolyPoly.getB2DRange();

    mnLastSyncEpoch = 0;
}

void ClippingController::Synchronize(const CoordinateMapper& rMapper,
                                     const HardwareSyncFunc& rSyncFunc)
{
    if (!rSyncFunc)
        return;

    vcl::MappingPolicy ePolicy = rMapper.GetMappingPolicy();
    uint64_t nCurrentKey = rMapper.GetSemanticKey(ePolicy);

    // The "Staircase" breaker: only sync if the semantic key has changed
    if (mnLastSyncEpoch == nCurrentKey)
        return;

    if (!mbHasClipRegion)
    {
        rSyncFunc(basegfx::B2DPolyPolygon());
    }
    else
    {
        // Matrix projection: Logic Space -> Device Space
        basegfx::B2DHomMatrix aMatrix = rMapper.GetLogicToDeviceMatrix(ePolicy);

        basegfx::B2DPolyPolygon aDevicePoly = maLogicPolyPoly;
        aDevicePoly.transform(aMatrix);

        rSyncFunc(aDevicePoly);
    }

    mnLastSyncEpoch = nCurrentKey;
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
