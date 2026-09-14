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

#include <vcl/toolkit/unowrap.hxx>
#include <vcl/transfer.hxx>
#include <vcl/window.hxx>

#include <window/ImplFrameData.hxx>
#include <window/WindowClassification.hxx>
#include <window/WindowPlatformState.hxx>
#include <window/WindowA11y.hxx>

#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/datatransfer/clipboard/XClipboard.hpp>
#include <com/sun/star/uno/Reference.hxx>

namespace vcl
{
void Window::SetWindowPeer(css::uno::Reference<css::awt::XVclWindowPeer> const& xPeer,
                           VCLXWindow* pVCLXWindow)
{
    if (!mpClassification || mpClassification->mbInDispose)
        return;

    if (mpAccessibleData)
        mpAccessibleData->setWindowPeer(xPeer, pVCLXWindow);
}

css::uno::Reference<css::awt::XVclWindowPeer> Window::GetComponentInterface(bool bCreate)
{
    if (mpAccessibleData)
        return mpAccessibleData->getWindowPeer(bCreate, this);

    return nullptr;
}

void Window::SetComponentInterface(css::uno::Reference<css::awt::XVclWindowPeer> const& xIFace)
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    SAL_WARN_IF(!pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!");
    if (pWrapper)
        pWrapper->SetWindowInterface(this, xIFace);
}

void Window::SetClipboard(
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> const& xClipboard)
{
    if (mpPlatformState->mpFrameData)
        mpPlatformState->mpFrameData->mxClipboard = xClipboard;
}

css::uno::Reference<css::datatransfer::clipboard::XClipboard> Window::GetClipboard()
{
    if (!mpPlatformState->mpFrameData)
        return static_cast<css::datatransfer::clipboard::XClipboard*>(nullptr);
    if (!mpPlatformState->mpFrameData->mxClipboard.is())
        mpPlatformState->mpFrameData->mxClipboard = GetSystemClipboard();
    return mpPlatformState->mpFrameData->mxClipboard;
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
