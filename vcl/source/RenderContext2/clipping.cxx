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

#include <salgdi.hxx>

bool RenderContext2::IsInitClipped() const { return mbInitClipRegion; }

void RenderContext2::SetInitClipFlag(bool bFlag) { mbInitClipRegion = bFlag; }

void RenderContext2::SetClipRegionFlag(bool bFlag) { mbClipRegion = bFlag; }

bool RenderContext2::IsClipRegion() const { return mbClipRegion; }

vcl::Region RenderContext2::GetActiveClipRegion() const { return GetClipRegion(); }

vcl::Region RenderContext2::GetClipRegion() const { return maGeometry.PixelToLogic(maRegion); }

void RenderContext2::SetClipRegion(vcl::Region const& rRegion)
{
    DBG_TESTSOLARMUTEX();

    if (rRegion.IsNull())
    {
        if (IsClipRegion())
        {
            maRegion = vcl::Region(true);
            SetClipRegionFlag(false);
            SetInitClipFlag(true);
        }
    }
    else
    {
        maRegion = maGeometry.LogicToPixel(rRegion);
        SetClipRegionFlag(true);
        SetInitClipFlag(true);
    }
}

bool RenderContext2::SelectClipRegion(vcl::Region const& rRegion, SalGraphics* pGraphics)
{
    DBG_TESTSOLARMUTEX();

    if (!pGraphics)
    {
        if (!mpGraphics && !AcquireGraphics())
            return false;
        pGraphics = mpGraphics;
    }

    OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

    bool bClipRegion = pGraphics->SetClipRegion(rRegion, *pOutDev);
    SAL_WARN_IF(!bClipRegion, "vcl.gdi",
                "RenderContext2::SelectClipRegion() - can't create region");
    return bClipRegion;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
