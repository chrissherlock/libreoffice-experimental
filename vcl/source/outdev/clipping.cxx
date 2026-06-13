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

#include <window.h>
#include <clipping.hxx>
#include <clipping_window.hxx>
#include <windowdev.hxx>
#include <devicedispatcher.hxx>
#include <salgdi.hxx>

void OutputDevice::SaveBackground(VirtualDevice& rSaveDevice,
                                  const Point& rPos, const Size& rSize, const Size& rBackgroundSize) const
{
   rSaveDevice.DrawOutDev(Point(), rBackgroundSize, rPos, rSize, *this);
}

vcl::Region OutputDevice::GetClipRegion() const
{
    return convertTo<vcl::LogicRegion>(vcl::WindowRegion(maRegion));
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
    if ( mbClipRegion )
    {
        if( mpMetaFile )
            mpMetaFile->AddAction( new MetaMoveClipRegionAction( nHorzMove, nVertMove ) );

        maRegion.Move(LogicWidthToDevicePixel(nHorzMove),
                      LogicHeightToDevicePixel(nVertMove));
        mbInitClipRegion = true;
    }
}

void OutputDevice::IntersectClipRegion( const tools::Rectangle& rRect )
{
    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaISectRectClipRegionAction( rRect ) );

    tools::Rectangle aRect = mpMapper->LogicToWindowUnits(rRect, GetMappingPolicy());
    maRegion.Intersect( aRect );
    mbClipRegion        = true;
    mbInitClipRegion    = true;
}


void OutputDevice::IntersectClipRegion( const vcl::Region& rRegion )
{
    if(!rRegion.IsNull())
    {
        if ( mpMetaFile )
            mpMetaFile->AddAction( new MetaISectRegionClipRegionAction( rRegion ) );

        vcl::Region aRegion = mpMapper->LogicToWindowUnits(rRegion, GetMappingPolicy());
        maRegion.Intersect( aRegion );
        mbClipRegion        = true;
        mbInitClipRegion    = true;
    }
}

vcl::Region OutputDevice::ClipToDeviceBounds(vcl::Region aRegion) const
{
    aRegion.Intersect(tools::Rectangle{GetDeviceOriginX(),
                                       GetDeviceOriginY(),
                                       GetDeviceOriginX() + GetOutputWidthPixel() - 1,
                                       GetDeviceOriginY() + GetOutputHeightPixel() - 1
                                      });
    return aRegion;
}

void OutputDevice::SetDeviceClipRegion( const vcl::Region* pRegion )
{
    DBG_TESTSOLARMUTEX();

    if ( !pRegion )
    {
        if ( mbClipRegion )
        {
            maRegion            = vcl::Region(true);
            mbClipRegion        = false;
            mbInitClipRegion    = true;
        }
    }
    else
    {
        maRegion            = *pRegion;
        mbClipRegion        = true;
        mbInitClipRegion    = true;
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

vcl::Region getActiveClipRegion(const OutputDevice& rDevice)
{
    return vcl::DispatchDevice(rDevice, [](const auto& rTypedDev) -> vcl::Region {
        using T = std::decay_t<decltype(rTypedDev)>;

        if constexpr (std::is_same_v<T, WindowOutputDevice>)
        {
            vcl::Region aRegion(true);
            WindowImpl* pImpl = rTypedDev.GetOwnerWindow()->ImplGetWindowImpl();

            if (pImpl->mbInPaint)
            {
                if (pImpl->mpPaintRegion)
                    aRegion = *(pImpl->mpPaintRegion);

                aRegion.Move(-rTypedDev.GetDeviceOriginX(), -rTypedDev.GetDeviceOriginY());
            }

            if (rTypedDev.IsClipRegion())
                aRegion.Intersect(rTypedDev.GetRegion());

            return rTypedDev.template convertTo<vcl::LogicRegion>(vcl::WindowRegion(aRegion)).get();
        }
        else
        {
            if (rTypedDev.IsClipRegion())
                return rTypedDev.GetClipRegion();

            return vcl::Region(tools::Rectangle(Point(0, 0), rTypedDev.GetOutputSizePixel()));
        }
    });
}

void initDeviceClipRegion(OutputDevice& rDevice)
{
    vcl::DispatchDevice(rDevice, [](auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;
        DBG_TESTSOLARMUTEX();

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
                aRegion = getWinChildClipRegion(*rTypedDev.GetOwnerWindow());

                if (rTypedDev.ImplIsAntiparallel())
                    rTypedDev.ReMirror(aRegion);
            }

            if (rTypedDev.IsClipRegion())
                aRegion.Intersect(rTypedDev.GetMapper().ViewToDevice(rTypedDev.GetRegion()));

            if (aRegion.IsEmpty())
            {
                rTypedDev.SetOutputClipped(true); // Assuming setter exists or maps to mbOutputClipped
            }
            else
            {
                rTypedDev.SetOutputClipped(false);
                rTypedDev.SelectClipRegion(aRegion);
            }

            rTypedDev.SetClipRegionSet(true);
            rTypedDev.SetInitClipRegion(false);
        }
        else
        {
            // Standard OutputDevice layout path
            if (rTypedDev.IsClipRegion())
            {
                if (rTypedDev.GetRegion().IsEmpty())
                {
                    rTypedDev.SetOutputClipped(true);
                }
                else
                {
                    rTypedDev.SetOutputClipped(false);
                    vcl::Region aRegion = rTypedDev.ClipToDeviceBounds(
                        rTypedDev.GetMapper().ViewToDevice(rTypedDev.GetRegion()));

                    if (aRegion.IsEmpty())
                        rTypedDev.SetOutputClipped(true);
                    else
                        rTypedDev.SelectClipRegion(aRegion);
                }

                rTypedDev.SetClipRegionSet(true);
            }
            else
            {
                if (rTypedDev.IsClipRegionSet())
                {
                    rTypedDev.ResetGraphicsClipRegion();
                    rTypedDev.SetClipRegionSet(false);
                }

                rTypedDev.SetOutputClipped(false);
            }

            rTypedDev.SetInitClipRegion(false);
        }
    });
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
