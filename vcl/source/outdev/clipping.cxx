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

#include <vcl/canvastools.hxx>
#include <vcl/deviceconcepts.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <devicedispatcher.hxx>
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

    return ((maRecorder.IsRecording() && IsOutputClipped())
        || mpClippingController->IsOutputClipped(*mpMapper, aBounds));
}

bool OutputDevice::GetVisibleDeviceRange(
        const basegfx::B2DHomMatrix& aFullTransform,
        basegfx::B2DRange &aVisibleRange,
        double &fMaximumArea)
{
    return vcl::DispatchDevice(*this, [&](auto& rConcreteDevice) -> bool {

        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        // Logical recorders (Metafiles/PDFs) have infinite canvases.
        // We MUST NOT cull them against physical pixel bounds, or we risk
        // cropping vector data permanently.
        const bool bIsLogical = vcl::LogicalRecorder<DeviceType> || GetConnectMetaFile();
        if (bIsLogical)
            return true;

        // For non-strictly-culled physical devices (like VirtualDevice memory buffers),
        // we might eventually want to bypass this too, but historically VCL culls
        // all non-metafile outputs to save RAM during the software transform.
        // We leave the culling intact for all physical paths for now.

        // limit TargetRange to existing pixels (if pixel device)
        // first get discrete range of object
        basegfx::B2DRange aFullPixelRange(aVisibleRange);

        aFullPixelRange.transform(aFullTransform);

        if(basegfx::fTools::equalZero(aFullPixelRange.getWidth()) || basegfx::fTools::equalZero(aFullPixelRange.getHeight()))
        {
            // object is outside of visible area
            return false;
        }

        // now get discrete target pixels; start with OutDev pixel size and evtl.
        // intersect with active clipping area
        basegfx::B2DRange aOutPixel(
            0.0,
            0.0,
            GetOutputSizePixel().Width(),
            GetOutputSizePixel().Height());

        if(HasClipRegion())
        {
            tools::Rectangle aRegionRectangle(GetActiveClipRegion().GetBoundRect());

            // caution! Range from rectangle, one too much (!)
            aRegionRectangle.AdjustRight(-1);
            aRegionRectangle.AdjustBottom(-1);
            aOutPixel.intersect( vcl::unotools::b2DRectangleFromRectangle(aRegionRectangle) );
        }

        if(aOutPixel.isEmpty())
        {
            // no active output area
            return false;
        }

        // if aFullPixelRange is not completely inside of aOutPixel,
        // reduction of target pixels is possible
        basegfx::B2DRange aVisiblePixelRange(aFullPixelRange);

        if(!aOutPixel.isInside(aFullPixelRange))
        {
            aVisiblePixelRange.intersect(aOutPixel);

            if(aVisiblePixelRange.isEmpty())
            {
                // nothing in visible part, reduces to nothing
                return false;
            }

            // aVisiblePixelRange contains the reduced output area in
            // discrete coordinates. To make it useful everywhere, make it relative to
            // the object range
            basegfx::B2DHomMatrix aMakeVisibleRangeRelative;

            aVisibleRange = aVisiblePixelRange;
            aMakeVisibleRangeRelative.translate(
                -aFullPixelRange.getMinX(),
                -aFullPixelRange.getMinY());
            aMakeVisibleRangeRelative.scale(
                1.0 / aFullPixelRange.getWidth(),
                1.0 / aFullPixelRange.getHeight());
            aVisibleRange.transform(aMakeVisibleRangeRelative);
        }

        const double fNewMaxArea(aVisiblePixelRange.getWidth() * aVisiblePixelRange.getHeight());

        fMaximumArea = std::min(4096000.0, fNewMaxArea + 1.0);

        return true;
    });
}

vcl::Region OutputDevice::GetClipRegion() const
{
    return PixelToLogic(mpClippingController->GetClipRegion());
}

void OutputDevice::SetClipRegion()
{
    maRecorder.RecordClipRegion(vcl::Region(), false);

    // Centralize state in the controller
    mpClippingController->SetNoClipRegion();
}

void OutputDevice::SetClipRegion(const vcl::Region& rRegion)
{
    maRecorder.RecordClipRegion(rRegion, !rRegion.IsNull());

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
    maRecorder.RecordMoveClipRegion(nHorzMove, nVertMove);

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
    maRecorder.RecordIntersectClipRegion(rRect);

    tools::Rectangle aRect = LogicToPixel(rRect);

    mpClippingController->IntersectClipRegion(vcl::Region(aRect));
}

void OutputDevice::IntersectClipRegion(const vcl::Region& rRegion)
{
    maRecorder.RecordIntersectClipRegion(rRegion);

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
