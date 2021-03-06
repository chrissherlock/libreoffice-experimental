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

void OutputDevice::SetClipRegion()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaClipRegionAction(vcl::Region(), false));

    RenderContext2::SetClipRegion();
}

void OutputDevice::SetClipRegion(vcl::Region const& rRegion )
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaClipRegionAction(rRegion, true));

    RenderContext2::SetClipRegion(rRegion);
}

void OutputDevice::MoveClipRegion(tools::Long nHorzMove, tools::Long nVertMove)
{
    if (mbClipRegion && mpMetaFile)
        mpMetaFile->AddAction(new MetaMoveClipRegionAction(nHorzMove, nVertMove));

    RenderContext2::MoveClipRegion(nHorzMove, nVertMove);
}

void OutputDevice::IntersectClipRegion(tools::Rectangle const& rRect)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaISectRectClipRegionAction(rRect));

    RenderContext2::IntersectClipRegion(rRect);
}

void OutputDevice::IntersectClipRegion(vcl::Region const& rRegion)
{
    if (!rRegion.IsNull() && mpMetaFile)
        mpMetaFile->AddAction(new MetaISectRegionClipRegionAction(rRegion));

    RenderContext2::IntersectClipRegion(rRegion);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
