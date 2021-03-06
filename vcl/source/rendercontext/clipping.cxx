/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/log.hxx>
#include <tools/debug.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>

bool RenderContext2::IsClipRegion() const { return mbClipRegion; }

vcl::Region RenderContext2::GetClipRegion() const { return PixelToLogic(maRegion); }

void RenderContext2::SetClipRegion()
{
    SetDeviceClipRegion(nullptr);

    if (mpAlphaVDev)
        mpAlphaVDev->SetClipRegion();
}

void RenderContext2::SetClipRegion(vcl::Region const& rRegion)
{
    if (rRegion.IsNull())
    {
        SetDeviceClipRegion(nullptr);
    }
    else
    {
        vcl::Region aRegion = LogicToPixel(rRegion);
        SetDeviceClipRegion(&aRegion);
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetClipRegion(rRegion);
}

bool RenderContext2::SelectClipRegion(vcl::Region const& rRegion, SalGraphics* pGraphics)
{
    DBG_TESTSOLARMUTEX();

    if (!pGraphics)
    {
        if (!mpGraphics && !AcquireGraphics())
        {
            assert(mpGraphics);
            return false;
        }

        pGraphics = mpGraphics;
    }

    bool bClipRegion = pGraphics->SetClipRegion(rRegion, *this);
    SAL_WARN_IF(bClipRegion, "vcl.gdi", "RenderContext2::SelectClipRegion() - can't create region");
    return bClipRegion;
}

void RenderContext2::SetDeviceClipRegion(vcl::Region const* pRegion)
{
    DBG_TESTSOLARMUTEX();

    if (!pRegion)
    {
        if (mbClipRegion)
        {
            maRegion = vcl::Region(true);
            mbClipRegion = false;
            mbInitClipRegion = true;
        }
    }
    else
    {
        maRegion = *pRegion;
        mbClipRegion = true;
        mbInitClipRegion = true;
    }
}

void RenderContext2::MoveClipRegion(tools::Long nHorzMove, tools::Long nVertMove)
{
    if (mbClipRegion)
    {
        maRegion.Move(ImplLogicWidthToDevicePixel(nHorzMove),
                      ImplLogicHeightToDevicePixel(nVertMove));
        mbInitClipRegion = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->MoveClipRegion(nHorzMove, nVertMove);
}

void RenderContext2::IntersectClipRegion(tools::Rectangle const& rRect)
{
    tools::Rectangle aRect = LogicToPixel(rRect);
    maRegion.Intersect(aRect);
    mbClipRegion = true;
    mbInitClipRegion = true;

    if (mpAlphaVDev)
        mpAlphaVDev->IntersectClipRegion(rRect);
}

void RenderContext2::IntersectClipRegion(vcl::Region const& rRegion)
{
    if (!rRegion.IsNull())
    {
        vcl::Region aRegion = LogicToPixel(rRegion);
        maRegion.Intersect(aRegion);
        mbClipRegion = true;
        mbInitClipRegion = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->IntersectClipRegion(rRegion);
}

vcl::Region RenderContext2::ClipToDeviceBounds(vcl::Region aRegion) const
{
    aRegion.Intersect(tools::Rectangle{ mnOutOffX, mnOutOffY, mnOutOffX + GetOutputWidthPixel() - 1,
                                        mnOutOffY + GetOutputHeightPixel() - 1 });
    return aRegion;
}

void RenderContext2::InitClipRegion()
{
    DBG_TESTSOLARMUTEX();

    if (mbClipRegion)
    {
        if (maRegion.IsEmpty())
        {
            mbOutputClipped = true;
        }
        else
        {
            mbOutputClipped = false;

            // #102532# Respect output offset also for clip region
            vcl::Region aRegion = ClipToDeviceBounds(ImplPixelToDevicePixel(maRegion));

            if (aRegion.IsEmpty())
            {
                mbOutputClipped = true;
            }
            else
            {
                mbOutputClipped = false;
                SelectClipRegion(aRegion);
            }
        }

        mbClipRegionSet = true;
    }
    else
    {
        if (mbClipRegionSet)
        {
            if (mpGraphics)
                mpGraphics->ResetClipRegion();

            mbClipRegionSet = false;
        }

        mbOutputClipped = false;
    }

    mbInitClipRegion = false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
