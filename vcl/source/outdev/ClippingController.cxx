/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>

namespace vcl
{
ClippingController::ClippingController()
    : maLogicRegion(true)
    , mbHasClipRegion(false)
    , mbDirty(true)
    , mbOutputClipped(false)
    , mbClipToDeviceBounds(true)
{
}

bool ClippingController::HasClipRegion() const { return mbHasClipRegion; }

bool ClippingController::IsDirty() const { return mbDirty; }

bool ClippingController::IsOutputClipped() const { return mbOutputClipped; }

const vcl::Region& ClippingController::GetClipRegion() const { return maLogicRegion; }

void ClippingController::SetDirty(bool bDirty) { mbDirty = bDirty; }

void ClippingController::SetOutputClipped(bool bClipped) { mbOutputClipped = bClipped; }

void ClippingController::SetClipRegion(const vcl::Region& rRegion)
{
    maLogicRegion = rRegion;
    mbHasClipRegion = true;
    mbDirty = true;
    mbOutputClipped = maLogicRegion.IsEmpty();
}

void ClippingController::SetNoClipRegion()
{
    maLogicRegion = vcl::Region(true);
    mbHasClipRegion = false;
    mbDirty = true;
    mbOutputClipped = false;
}

void ClippingController::IntersectClipRegion(const vcl::Region& rRegion)
{
    if (mbHasClipRegion)
    {
        maLogicRegion.Intersect(rRegion);
    }
    else
    {
        maLogicRegion = rRegion;
        mbHasClipRegion = true;
    }

    mbDirty = true;
    mbOutputClipped = maLogicRegion.IsEmpty();
}

void ClippingController::Synchronize(const CoordinateMapper& rMapper,
                                     const std::function<void(const vcl::Region&)>& rSyncFunc)
{
    if (!mbDirty)
        return;

    if (!mbHasClipRegion)
        return;

    if (maLogicRegion.IsEmpty())
    {
        mbOutputClipped = true;
    }
    else
    {
        vcl::Region aEffectiveRegion = maLogicRegion;

        if (mbClipToDeviceBounds)
        {
            aEffectiveRegion.Intersect(
                tools::Rectangle(rMapper.GetOutOffXPixel(), rMapper.GetOutOffYPixel(),
                                 rMapper.GetOutOffXPixel() + rMapper.GetOutputWidthPixel() - 1,
                                 rMapper.GetOutOffYPixel() + rMapper.GetOutputHeightPixel() - 1));
        }

        if (aEffectiveRegion.IsEmpty())
        {
            mbOutputClipped = true;
        }
        else
        {
            mbOutputClipped = false;
            rSyncFunc(aEffectiveRegion); // Callback to SalGraphics
        }
    }

    mbDirty = false;
}

void ClippingController::SetLogicalClip(const vcl::Region& rRegion, const CoordinateMapper& rMapper)
{
    if (rRegion.IsNull())
    {
        SetNoClipRegion();
        return;
    }

    vcl::Region aPixelRegion = rMapper.LogicToPixel(rRegion);

    maLogicRegion = aPixelRegion;
    mbHasClipRegion = true;
    mbDirty = true;
    mbOutputClipped = maLogicRegion.IsEmpty();
}

void ClippingController::IntersectLogicalClip(const vcl::Region& rRegion,
                                              const CoordinateMapper& rMapper)
{
    vcl::Region aPixelRegion = rMapper.LogicToPixel(rRegion);

    if (mbHasClipRegion)
    {
        maLogicRegion.Intersect(aPixelRegion);
    }
    else
    {
        maLogicRegion = aPixelRegion;
        mbHasClipRegion = true;
    }

    mbDirty = true;
    mbOutputClipped = maLogicRegion.IsEmpty();
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
