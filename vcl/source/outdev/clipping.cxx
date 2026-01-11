/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <sal/config.h>
#include <osl/diagnose.h>
#include <tools/debug.hxx>

#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

bool OutputDevice::HasClipRegion() const
{
    return mpClippingController->HasClipRegion();
}

bool OutputDevice::IsOutputClipped() const
{
    return mpClippingController->IsOutputClipped();
}

bool OutputDevice::IsOutputCulled() const
{
    // If Recording (PDF), use Infinite Bounds so we don't cull off-screen objects.
    // If Rendering (Screen), use Device Bounds so we don't blackout the screen.
    tools::Rectangle aBounds = tools::Rectangle(Point(0, 0), GetOutputSizePixel());

    return ((mpMetaFile && IsOutputClipped())
        || mpClippingController->IsOutputClipped(*mpMapper, aBounds));
}

void OutputDevice::SaveBackground(VirtualDevice& rSaveDevice,
                                  const Point& rPos, const Size& rSize, const Size& rBackgroundSize) const
{
   rSaveDevice.DrawOutDev(Point(), rBackgroundSize, rPos, rSize, *this);
}

vcl::Region OutputDevice::GetClipRegion() const
{
    return PixelToLogic(mpClippingController->GetClipRegion());
}

void OutputDevice::SetClipRegion()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaClipRegionAction(vcl::Region(), false));

    // Centralize state in the controller
    mpClippingController->SetNoClipRegion();
}

void OutputDevice::SetClipRegion(const vcl::Region& rRegion)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaClipRegionAction(rRegion, !rRegion.IsNull()));

    mpClippingController->SetLogicalClip(rRegion, *mpMapper);
}

bool OutputDevice::SetGraphicsClip(const vcl::Region& rRegion, SalGraphics* pGraphics)
{
    DBG_TESTSOLARMUTEX();

    if (!pGraphics)
    {
        if (!mpGraphics && !AcquireGraphics())
            return false;

        assert(mpGraphics);
        pGraphics = mpGraphics;
    }

    // Apply the region to the hardware
    pGraphics->SetClipRegion(rRegion, *this);

    // Mark that the hardware is now in sync with this region
    mpClippingController->SetDirty(false);

    return true;
}

void OutputDevice::MoveClipRegion(long nHorzMove, long nVertMove)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMoveClipRegionAction(nHorzMove, nVertMove));

    if (mpClippingController->HasClipRegion())
    {
        vcl::Region aRegion = mpClippingController->GetClipRegion();

        long nHorzPixel = LogicWidthToDevicePixel(nHorzMove);
        long nVertPixel = LogicHeightToDevicePixel(nVertMove);

        aRegion.Move(nHorzPixel, nVertPixel);
        mpClippingController->SetClipRegion(aRegion);
    }
}

void OutputDevice::IntersectClipRegion(const tools::Rectangle& rRect)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaISectRectClipRegionAction(rRect));

    tools::Rectangle aRect = LogicToPixel(rRect);

    mpClippingController->IntersectClipRegion(vcl::Region(aRect));
}

void OutputDevice::IntersectClipRegion(const vcl::Region& rRegion)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaISectRegionClipRegionAction(rRegion));

    mpClippingController->IntersectLogicalClip(rRegion, *mpMapper);
}

void OutputDevice::InitClipRegion()
{
    DBG_TESTSOLARMUTEX();

    mpClippingController->Synchronize(*mpMapper, [this](const vcl::Region& rPixelRegion) {
        this->SetGraphicsClip(rPixelRegion);
    });
}

vcl::Region OutputDevice::ClipToDeviceBounds(vcl::Region aRegion) const
{
    aRegion.Intersect(tools::Rectangle{GetOutOffXPixel(),
                                       GetOutOffYPixel(),
                                       GetOutOffXPixel() + GetOutputWidthPixel() - 1,
                                       GetOutOffYPixel() + GetOutputHeightPixel() - 1
                                      });
    return aRegion;
}

vcl::Region OutputDevice::GetActiveClipRegion() const
{
    return GetClipRegion();
}

void OutputDevice::ClipToPaintRegion(tools::Rectangle& /*rDstRect*/)
{
    // this is only used in Window, but we still need it as it's called
    // on in other clipping functions
}

void OutputDevice::SetDeviceClipRegion(const vcl::Region* pRegion)
{
    DBG_TESTSOLARMUTEX();

    if (!pRegion)
    {
        if (mpClippingController->HasClipRegion())
            mpClippingController->SetNoClipRegion();
    }
    else
    {
        mpClippingController->SetClipRegion(*pRegion);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
