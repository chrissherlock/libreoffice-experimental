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

#include <window.h>
#include <helpwin.hxx>

#include "HandleGestureEventBase.hxx"

bool HandleGestureEventBase::Setup()
{

    if (m_pSVData->mpWinData->mpAutoScrollWin)
        m_pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
    if (ImplGetSVHelpData().mpHelpWin)
        ImplDestroyHelpWindow( true );
    return !m_pWindow->isDisposed();
}

bool HandleGestureEventBase::IsAcceptableWheelScrollTarget(const vcl::Window *pMouseWindow)
{
    return (pMouseWindow && !pMouseWindow->isDisposed() && pMouseWindow->IsInputEnabled() && !pMouseWindow->IsInModalMode());
}

vcl::Window* HandleGestureEventBase::FindTarget()
{
    // first check any floating window ( eg. drop down listboxes)
    vcl::Window *pMouseWindow = nullptr;

    if (m_pSVData->mpWinData->mpFirstFloat && !m_pSVData->mpWinData->mpCaptureWin &&
         !m_pSVData->mpWinData->mpFirstFloat->ImplIsFloatPopupModeWindow( m_pWindow ) )
    {
        bool bHitTestInsideRect = false;
        pMouseWindow = m_pSVData->mpWinData->mpFirstFloat->ImplFloatHitTest( m_pWindow, m_aMousePos, bHitTestInsideRect );
        if (!pMouseWindow)
            pMouseWindow = m_pSVData->mpWinData->mpFirstFloat;
    }
    // then try the window directly beneath the mouse
    if( !pMouseWindow )
    {
        pMouseWindow = m_pWindow->ImplFindWindow( m_aMousePos );
    }
    else
    {
        // transform coordinates to float window frame coordinates
        pMouseWindow = pMouseWindow->ImplFindWindow(
                 pMouseWindow->OutputToScreenPixel(
                  pMouseWindow->AbsoluteScreenToOutputPixel(
                   m_pWindow->OutputToAbsoluteScreenPixel(
                    m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
    }

    while (IsAcceptableWheelScrollTarget(pMouseWindow))
    {
        if (pMouseWindow->IsEnabled())
            break;
        //try the parent if this one is disabled
        pMouseWindow = pMouseWindow->GetParent();
    }

    return pMouseWindow;
}

vcl::Window *HandleGestureEventBase::Dispatch(vcl::Window* pMouseWindow)
{
    vcl::Window *pDispatchedTo = nullptr;

    if (IsAcceptableWheelScrollTarget(pMouseWindow) && pMouseWindow->IsEnabled())
    {
        // transform coordinates to float window frame coordinates
        Point aRelMousePos( pMouseWindow->OutputToScreenPixel(
                             pMouseWindow->AbsoluteScreenToOutputPixel(
                              m_pWindow->OutputToAbsoluteScreenPixel(
                               m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
        bool bPropagate = CallCommand(pMouseWindow, aRelMousePos);
        if (!bPropagate)
            pDispatchedTo = pMouseWindow;
    }

    // if the command was not handled try the focus window
    if (pDispatchedTo)
        return pDispatchedTo;

    vcl::Window* pFocusWindow = m_pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
    if ( !pFocusWindow || (pFocusWindow == pMouseWindow) ||
         (pFocusWindow != m_pSVData->mpWinData->mpFocusWin) )
    {
        return nullptr;
    }

    // no wheel-messages to disabled windows
    if ( !pFocusWindow->IsEnabled() || !pFocusWindow->IsInputEnabled() || pFocusWindow->IsInModalMode() )
        return nullptr;

    // transform coordinates to focus window frame coordinates
    Point aRelMousePos( pFocusWindow->OutputToScreenPixel(
                         pFocusWindow->AbsoluteScreenToOutputPixel(
                          m_pWindow->OutputToAbsoluteScreenPixel(
                           m_pWindow->ScreenToOutputPixel( m_aMousePos ) ) ) ) );
    bool bPropagate = CallCommand(pFocusWindow, aRelMousePos);
    if (!bPropagate)
        return pMouseWindow;

    return nullptr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
