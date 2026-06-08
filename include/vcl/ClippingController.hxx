
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/range/b2drange.hxx>
#include <vcl/dllapi.h>
#include <functional>
#include <cstdint>

class CoordinateMapper;

namespace vcl
{
enum class ClipOp
{
    Set,
    Intersect
};

class VCL_DLLPUBLIC ClippingController
{
private:
    basegfx::B2DPolyPolygon maLogicPolyPoly;
    basegfx::B2DRange maLogicRange;
    bool mbIsRectangular;
    bool mbHasClipRegion;
    uint64_t mnLastSyncEpoch;

public:
    ClippingController();

    // Visibility Query
    bool IsCulled(const basegfx::B2DRange& rDeviceRange) const;

    // Region Management
    void ModifyClip(const basegfx::B2DPolyPolygon& rPolyPoly, ClipOp eOp);
    void SetNoClipRegion();

    // Synchronization
    using HardwareSyncFunc = std::function<void(const basegfx::B2DPolyPolygon&)>;
    void Synchronize(const CoordinateMapper& rMapper, const HardwareSyncFunc& rSyncFunc);

    // Getters for testing/translation
    bool HasClipRegion() const { return mbHasClipRegion; }
    const basegfx::B2DPolyPolygon& GetLogicClip() const { return maLogicPolyPoly; }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
