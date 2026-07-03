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
#include <vcl/window.hxx>
#include <vcl/virdev.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <WindowOutputDevice.hxx>
#include <window.h>
#include <clipping.hxx>
#include <clipping/ClippingManager.hxx>
#include <devicedispatcher.hxx>
#include <salgdi.hxx>

vcl::clipping::ClippingManager& OutputDevice::GetClippingManager(vcl::Window& rRoot)
{
    // Lazy instantiation: Only create the manager if/when
    // a component actually requests clipping logic.
    if (!mpClippingManager)
        mpClippingManager = std::make_unique<vcl::clipping::ClippingManager>(rRoot);

    return *mpClippingManager;
}

void OutputDevice::SaveBackground(VirtualDevice& rSaveDevice,
                                  const Point& rPos, const Size& rSize, const Size& rBackgroundSize) const
{
   rSaveDevice.DrawOutDev(Point(), rBackgroundSize, rPos, rSize, *this);
}

vcl::Region OutputDevice::GetClipRegion() const
{
    return convertTo<vcl::LogicRegion>(vcl::WindowRegion(maClipState.maRegion));
}

void OutputDevice::SetClipRegion()
{

    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaClipRegionAction( vcl::Region(), false ) );

    SetDeviceClipRegion( nullptr );
}

void OutputDevice::SetClipRegion( const vcl::Region& rRegion )
{

    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaClipRegionAction( rRegion, true ) );

    if ( rRegion.IsNull() )
    {
        SetDeviceClipRegion( nullptr );
    }
    else
    {
        vcl::Region aRegion = mpMapper->LogicToWindowUnits(rRegion, GetMappingPolicy());
        SetDeviceClipRegion( &aRegion );
    }
}

bool OutputDevice::SelectClipRegion( const vcl::Region& rRegion, SalGraphics* pGraphics )
{
    DBG_TESTSOLARMUTEX();

    if( !pGraphics )
    {
        if( !mpGraphics && !AcquireGraphics() )
            return false;
        assert(mpGraphics);
        pGraphics = mpGraphics;
    }

    pGraphics->SetClipRegion( rRegion, *this );
    return true;
}

void OutputDevice::MoveClipRegion( tools::Long nHorzMove, tools::Long nVertMove )
{
    if (!maClipState.mbHasCustomClip)
        return;

    if( mpMetaFile )
        mpMetaFile->AddAction( new MetaMoveClipRegionAction( nHorzMove, nVertMove ) );

    maClipState.maRegion.Move(LogicWidthToDevicePixel(nHorzMove),
                              LogicHeightToDevicePixel(nVertMove));

    maClipState.Invalidate();
}

void OutputDevice::IntersectClipRegion( const tools::Rectangle& rRect )
{
    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaISectRectClipRegionAction( rRect ) );

    tools::Rectangle aRect = mpMapper->LogicToWindowUnits(rRect, GetMappingPolicy());
    maClipState.maRegion.Intersect( aRect );
    maClipState.mbHasCustomClip = true;
    maClipState.Invalidate();
}

void OutputDevice::IntersectClipRegion(const vcl::Region& rRegion)
{
    if (rRegion.IsNull())
        return;

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaISectRegionClipRegionAction(rRegion));

    vcl::Region aRegion = mpMapper->LogicToWindowUnits(rRegion, GetMappingPolicy());
    maClipState.maRegion.Intersect(aRegion);
    maClipState.mbHasCustomClip = true;
    maClipState.Invalidate();
}

vcl::Region OutputDevice::ClipToDeviceBounds(vcl::Region aRegion) const
{
    aRegion.Intersect(tools::Rectangle{
        GetDeviceOriginX(),
        GetDeviceOriginY(),
        GetDeviceOriginX() + GetOutputWidthPixel() - 1,
        GetDeviceOriginY() + GetOutputHeightPixel() - 1
    });

    return aRegion;
}

void OutputDevice::SetDeviceClipRegion(const vcl::Region* pRegion)
{
    DBG_TESTSOLARMUTEX();

    if (!pRegion)
    {
        if (maClipState.mbHasCustomClip)
        {
            maClipState.maRegion = vcl::Region(true);
            maClipState.mbHasCustomClip = false;
            maClipState.Invalidate();
        }
    }
    else
    {
        maClipState.maRegion = *pRegion;
        maClipState.mbHasCustomClip = true;
        maClipState.Invalidate();
    }
}

void OutputDevice::ResetGraphicsClipRegion()
{
    if (mpGraphics)
        mpGraphics->ResetClipRegion();
}

vcl::Region OutputDevice::GetActiveClipRegion() const
{
    return vcl::clipping::getActiveClipRegion(*this);
}

namespace vcl::clipping {

void clipToPaintRegion(OutputDevice& rDevice, tools::Rectangle& rDstRect)
{
    vcl::DispatchDevice(rDevice, [&rDstRect](auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;

        if constexpr (std::is_same_v<T, WindowOutputDevice>)
        {
            // Only WindowOutputDevices have owner windows and paint regions
            const vcl::Region aPaintRgn(rTypedDev.GetOwnerWindow()->GetPaintRegion());
            if (aPaintRgn.IsNull())
                return;

            auto aBoundRect  = vcl::LogicRect(aPaintRgn.GetBoundRect());
            auto aWindowRect = rTypedDev.template convertTo<vcl::WindowRect>(aBoundRect, rTypedDev.GetMapMode()).get();

            rDstRect.Intersection(aWindowRect);
        }
        // For Printer, VirtualDevice, or base OutputDevice, this does nothing (which is correct)
    });
}

static vcl::Region lcl_getWindowActiveClip(const WindowOutputDevice& rWinDev)
{
    vcl::Region aRegion(true);
    WindowImpl* pImpl = rWinDev.GetOwnerWindow()->ImplGetWindowImpl();

    if (pImpl->mbInPaint && pImpl->mpPaintRegion)
    {
        aRegion = *(pImpl->mpPaintRegion);
        aRegion.Move(-rWinDev.GetDeviceOriginX(), -rWinDev.GetDeviceOriginY());
    }

    const auto& rState = rWinDev.GetClipState();
    if (rState.mbHasCustomClip)
        aRegion.Intersect(rState.maRegion);

    return rWinDev.template convertTo<vcl::LogicRegion>(vcl::WindowRegion(aRegion)).get();
}

static vcl::Region lcl_getDeviceActiveClip(const OutputDevice& rDev)
{
    const auto& rState = rDev.GetClipState();
    if (rState.mbHasCustomClip)
        return rState.maRegion;

    // Return the device bounds instead of an "infinite" region
    return vcl::Region(tools::Rectangle(Point(0, 0), rDev.GetOutputSizePixel()));
}

vcl::Region getActiveClipRegion(const OutputDevice& rDevice)
{
    return vcl::DispatchDevice(rDevice, [](const auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;
        if constexpr (std::is_same_v<T, WindowOutputDevice>)
            return lcl_getWindowActiveClip(rTypedDev);
        else
            return lcl_getDeviceActiveClip(rTypedDev);
    });
}

void initDeviceClipRegion(OutputDevice& rDevice)
{
    vcl::DispatchDevice(rDevice, [](auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;
        DBG_TESTSOLARMUTEX();

        // Grab the unified state block
        auto& rState = rTypedDev.GetClipState();

        if constexpr (std::is_same_v<T, WindowOutputDevice>)
        {
            vcl::Region aRegion;
            WindowImpl* pImpl = rTypedDev.GetOwnerWindow()->ImplGetWindowImpl();

            if (pImpl->mbInPaint)
            {
                if (pImpl->mpPaintRegion)
                    aRegion = *(pImpl->mpPaintRegion);
            }
            else
            {
                aRegion = rTypedDev.GetClippingManager(*rTypedDev.GetOwnerWindow()).GetClipPlan(*rTypedDev.GetOwnerWindow()).maFinalRegion;

                if (rTypedDev.ImplIsAntiparallel())
                    rTypedDev.ReMirror(aRegion);
            }

            if (rState.mbHasCustomClip)
                aRegion.Intersect(rTypedDev.GetMapper().ViewToDevice(rState.maRegion));

            if (aRegion.IsEmpty())
            {
                rState.mbOutputClipped = true;
            }
            else
            {
                rState.mbOutputClipped = false;
                rTypedDev.SelectClipRegion(aRegion);
            }

            rState.mbBackendClipInstalled = true;
            rState.mbNeedsRecalc = false;
        }
        else
        {
            // Standard OutputDevice layout path
            if (rState.mbHasCustomClip)
            {
                if (rState.maRegion.IsEmpty())
                {
                    rState.mbOutputClipped = true;
                }
                else
                {
                    rState.mbOutputClipped = false;
                    vcl::Region aRegion = static_cast<OutputDevice&>(rTypedDev).ClipToDeviceBounds(
                        rTypedDev.GetMapper().ViewToDevice(rState.maRegion));

                    if (aRegion.IsEmpty())
                    {
                        rState.mbOutputClipped = true;
                    }
                    else
                    {
                        rTypedDev.SelectClipRegion(aRegion);
                    }
                }

                rState.mbBackendClipInstalled = true;
            }
            else
            {
                // If we previously pushed a valid clip to hardware, clear it.
                if (rState.mbBackendClipInstalled)
                {
                    rTypedDev.ResetGraphicsClipRegion();
                    rState.mbBackendClipInstalled = false;
                }

                rState.mbOutputClipped = false;
            }

            rState.mbNeedsRecalc = false;
        }
    });
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
