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

#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>

#include <ImplOutDevData.hxx>
#include <salframe.hxx>
#include <svdata.hxx>
#include <window.h>
#include <helpwin.hxx>

#ifdef _WIN32 // see #140456#
#include <win/salframe.h>
#endif

#include <cassert>

namespace vcl {

// We will eventually being removing the inheritance of OutputDevice
// from Window. It will be replaced with a transient relationship such
// that the OutputDevice is only live for the scope of the Paint method.
// In the meantime this can help move us towards a Window use an
// OutputDevice, not being one.

::OutputDevice const* Window::GetOutDev() const
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

::OutputDevice* Window::GetOutDev()
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

void Window::RecordLayoutData( vcl::ControlLayoutData* pLayout, const tools::Rectangle& rRect )
{
    assert(GetOutDev()->mpOutDevData);
    GetOutDev()->mpOutDevData->mpRecordLayout = pLayout;
    GetOutDev()->mpOutDevData->maRecordRect = rRect;
    Paint(*GetOutDev(), rRect);
    GetOutDev()->mpOutDevData->mpRecordLayout = nullptr;
}

bool Window::IsScrollable() const
{
    // check for scrollbars
    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while( pChild )
    {
        if( pChild->GetType() == WindowType::SCROLLBAR )
            return true;
        else
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
    return false;
}

void Window::ImplIsInTaskPaneList( bool mbIsInTaskList )
{
    mpWindowImpl->mbIsInTaskPaneList = mbIsInTaskList;
}

bool Window::SupportsDoubleBuffering() const
{
    return mpWindowImpl->mpFrameData->mpBuffer;
}

void Window::RequestDoubleBuffering(bool bRequest)
{
    if (bRequest)
    {
        mpWindowImpl->mpFrameData->mpBuffer = VclPtrInstance<VirtualDevice>();
        // Make sure that the buffer size matches the frame size.
        mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(mpWindowImpl->mpFrameWindow->GetOutputSizePixel());
    }
    else
        mpWindowImpl->mpFrameData->mpBuffer.reset();
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
