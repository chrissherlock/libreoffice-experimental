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
#include <vcl/gdimtf.hxx>
#include <vcl/outdev.hxx>

#include <salgdi.hxx>

void OutputDevice::SaveBackground(VirtualDevice& rSaveDevice,
                                  const Point& rPos, const Size& rSize, const Size& rBackgroundSize) const
{
   rSaveDevice.DrawOutDev(Point(), rBackgroundSize, rPos, rSize, *this);
}

void OutputDevice::SetClipRegion(vcl::Region const& rRegion)
{

    if (mpMetaFile)
    {
        if (!rRegion.IsNull())
            mpMetaFile->AddAction(new MetaClipRegionAction(rRegion, true));
        else
            mpMetaFile->AddAction(new MetaClipRegionAction(vcl::Region(), false));
    }

    RenderContext2::SetClipRegion(rRegion);

    if (mpAlphaVDev)
        mpAlphaVDev->SetClipRegion(rRegion);
}

bool OutputDevice::SelectClipRegion( const vcl::Region& rRegion, SalGraphics* pGraphics )
{
    DBG_TESTSOLARMUTEX();

    if( !pGraphics )
    {
        if( !mpGraphics && !AcquireGraphics() )
            return false;
        pGraphics = mpGraphics;
    }

    bool bClipRegion = pGraphics->SetClipRegion( rRegion, *this );
    OSL_ENSURE( bClipRegion, "OutputDevice::SelectClipRegion() - can't create region" );
    return bClipRegion;
}

void OutputDevice::MoveClipRegion( tools::Long nHorzMove, tools::Long nVertMove )
{

    if (IsClipRegion())
    {
        if( mpMetaFile )
            mpMetaFile->AddAction( new MetaMoveClipRegionAction( nHorzMove, nVertMove ) );

        maRegion.Move( maGeometry.LogicWidthToDevicePixel( nHorzMove ),
                       maGeometry.LogicHeightToDevicePixel( nVertMove ) );
        SetInitClipFlag(true);
    }

    if( mpAlphaVDev )
        mpAlphaVDev->MoveClipRegion( nHorzMove, nVertMove );
}

void OutputDevice::IntersectClipRegion( const tools::Rectangle& rRect )
{

    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaISectRectClipRegionAction( rRect ) );

    tools::Rectangle aRect = maGeometry.LogicToPixel( rRect );
    maRegion.Intersect( aRect );
    SetClipRegionFlag(true);
    SetInitClipFlag(true);

    if( mpAlphaVDev )
        mpAlphaVDev->IntersectClipRegion( rRect );
}

void OutputDevice::IntersectClipRegion( const vcl::Region& rRegion )
{

    if(!rRegion.IsNull())
    {
        if ( mpMetaFile )
            mpMetaFile->AddAction( new MetaISectRegionClipRegionAction( rRegion ) );

        vcl::Region aRegion = maGeometry.LogicToPixel( rRegion );
        maRegion.Intersect( aRegion );
        SetClipRegionFlag(true);
        SetInitClipFlag(true);
    }

    if( mpAlphaVDev )
        mpAlphaVDev->IntersectClipRegion( rRegion );
}

void OutputDevice::InitClipRegion()
{
    DBG_TESTSOLARMUTEX();

    if (IsClipRegion())
    {
        if ( maRegion.IsEmpty() )
        {
            mbOutputClipped = true;
        }
        else
        {
            mbOutputClipped = false;

            // #102532# Respect output offset also for clip region
            vcl::Region aRegion = ClipToDeviceBounds(maGeometry.PixelToDevicePixel(maRegion));

            if ( aRegion.IsEmpty() )
            {
                mbOutputClipped = true;
            }
            else
            {
                mbOutputClipped = false;
                SelectClipRegion( aRegion );
            }
        }

        mbClipRegionSet = true;
    }
    else
    {
        if ( mbClipRegionSet )
        {
            if (mpGraphics)
                mpGraphics->ResetClipRegion();
            mbClipRegionSet = false;
        }

        mbOutputClipped = false;
    }

    SetInitClipFlag(false);
}

vcl::Region OutputDevice::ClipToDeviceBounds(vcl::Region aRegion) const
{
    aRegion.Intersect(tools::Rectangle{GetXOffsetInPixels(),
                                       GetYOffsetInPixels(),
                                       GetXOffsetInPixels() + GetWidthInPixels() - 1,
                                       GetYOffsetInPixels() + GetHeightInPixels() - 1
                                      });
    return aRegion;
}

void OutputDevice::ClipToPaintRegion(tools::Rectangle& /*rDstRect*/)
{
    // this is only used in Window, but we still need it as it's called
    // on in other clipping functions
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
