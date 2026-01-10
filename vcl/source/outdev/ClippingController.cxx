/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ClippingController.hxx>

namespace vcl
{
ClippingController::ClippingController()
    : maClipRegion(true)
    , mbClipRegion(false)
    , mbInitClipRegion(true)
    , mbOutputClipped(false)
{
}

bool ClippingController::HasClipRegion() const { return mbClipRegion; }

bool ClippingController::NeedsInit() const { return mbInitClipRegion; }

bool ClippingController::IsOutputClipped() const { return mbOutputClipped; }

const vcl::Region& ClippingController::GetClipRegion() const { return maClipRegion; }

void ClippingController::SetInitClipRegion(bool bInit) { mbInitClipRegion = bInit; }

void ClippingController::SetOutputClipped(bool bClipped) { mbOutputClipped = bClipped; }

void ClippingController::SetClipRegion(const vcl::Region& rRegion)
{
    maClipRegion = rRegion;
    mbClipRegion = true;
    mbInitClipRegion = true;
    mbOutputClipped = maClipRegion.IsEmpty();
}

void ClippingController::SetNoClipRegion()
{
    maClipRegion = vcl::Region(true);
    mbClipRegion = false;
    mbInitClipRegion = true;
    mbOutputClipped = false;
}

void ClippingController::IntersectClipRegion(const vcl::Region& rRegion)
{
    if (mbClipRegion)
    {
        maClipRegion.Intersect(rRegion);
    }
    else
    {
        maClipRegion = rRegion;
        mbClipRegion = true;
    }

    mbInitClipRegion = true;
    mbOutputClipped = maClipRegion.IsEmpty();
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
