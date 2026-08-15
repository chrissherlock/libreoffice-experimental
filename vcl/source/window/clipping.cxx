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

#include <tools/debug.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/window.hxx>
#include <vcl/virdev.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <clipping_window.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl {

void Window::SetParentClipMode(ParentClipMode eMode)
{
    vcl::clipping::setParentClipMode(this, eMode);
}

ParentClipMode Window::GetParentClipMode() const
{
    return vcl::clipping::getParentClipMode(*this);
}

void Window::ExpandPaintClipRegion(const vcl::Region& rRegion)
{
    if (!mpWindowImpl->mpPaintRegion)
        return;

    WindowRegion aPixRegion(rRegion);
    vcl::Region aDevPixRegion = GetOutDev()->GetMapper().ViewToDevice(aPixRegion.get());

    vcl::Region aWinChildRegion = vcl::clipping::getWinChildClipRegion(*this);

    // only this region is in frame coordinates, so re-mirror it
    if (GetOutDev()->ImplIsAntiparallel())
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror(aWinChildRegion);
    }

    aDevPixRegion.Intersect(aWinChildRegion);

    if (aDevPixRegion.IsEmpty())
        return;

    mpWindowImpl->mpPaintRegion->Union(aDevPixRegion);
    GetOutDev()->GetClipState().Invalidate();
}

vcl::Region Window::GetWindowClipRegionPixel() const
{
    vcl::Region aWinClipRegion;

    if (mpWindowImpl->mpClippingState->mbInitWinClipRegion)
        clipping::initWinClipRegion(*this);

    aWinClipRegion = mpWindowImpl->mpClippingState->maWinClipRegion;

    vcl::Region aWinRegion(GetOutputRectPixel());

    if (aWinRegion == aWinClipRegion)
        aWinClipRegion.SetNull();

    aWinClipRegion.Move(-GetOutDev()->GetDeviceOriginX(), -GetOutDev()->GetDeviceOriginY());

    return aWinClipRegion;
}

void Window::EnableClipSiblings(bool bClipSiblings)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->EnableClipSiblings(bClipSiblings);

    mpWindowImpl->mpClippingState->mbClipSiblings = bClipSiblings;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
