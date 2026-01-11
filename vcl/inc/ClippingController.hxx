/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project. [cite: 6]
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this [cite: 7]
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. [cite: 8]
 */

#pragma once

#include <vcl/region.hxx>
#include <vcl/dllapi.h>

#include <functional>

class CoordinateMapper;

namespace vcl
{
class VCL_DLLPUBLIC ClippingController
{
private:
    vcl::Region maLogicRegion;
    vcl::Region maEffectiveRegion;

    bool mbHasClipRegion;

    bool mbDirty;
    bool mbOutputClipped;
    bool mbClipToDeviceBounds;

public:
    ClippingController();

    const vcl::Region& GetClipRegion() const;
    bool HasClipRegion() const;
    bool IsOutputClipped() const;
    bool IsOutputClipped(const CoordinateMapper& rMapper, const tools::Rectangle& rRect) const;

    bool IsDirty() const;
    void SetDirty(bool bDirty);
    void SetOutputClipped(bool bClipped);

    void SetClipRegion(const vcl::Region& rRegion);
    void SetNoClipRegion();

    void IntersectClipRegion(const vcl::Region& rRegion);
    void IntersectLogicalClip(const vcl::Region& rRegion, const CoordinateMapper& rMapper);

    void SetLogicalClip(const vcl::Region& rRegion, const CoordinateMapper& rMapper);

    using HardwareSyncFunc = std::function<void(const vcl::Region&)>;
    void Synchronize(const CoordinateMapper& rMapper,
                     const std::function<void(const vcl::Region&)>& rSyncFunc);

    void Synchronize(const CoordinateMapper& rMapper, const tools::Rectangle& rDeviceBounds,
                     const std::function<void(const vcl::Region&)>& rSyncFunc);

    void EnableDeviceBoundsClipping(bool bEnable) { mbClipToDeviceBounds = bEnable; }
};
} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
