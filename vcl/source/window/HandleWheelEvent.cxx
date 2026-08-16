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

#include <vcl/event.hxx>
#include <vcl/window.hxx>

#include <window.h>

#include "HandleWheelEvent.hxx"

HandleWheelEvent::HandleWheelEvent(vcl::Window* pWindow, const SalWheelMouseEvent& rEvt)
    : HandleGestureEventBase(pWindow, Point(rEvt.mnX, rEvt.mnY))
{
    CommandWheelMode nMode;
    sal_uInt16 nCode = rEvt.mnCode;
    bool bHorz = rEvt.mbHorz;
    bool bPixel = rEvt.mbDeltaIsPixel;

    if (nCode & KEY_MOD1)
    {
        nMode = CommandWheelMode::ZOOM;
    }
    else if (nCode & KEY_MOD2)
    {
        nMode = CommandWheelMode::DATAZOOM;
    }
    else
    {
        nMode = CommandWheelMode::SCROLL;

        // #i85450# interpret shift-wheel as horizontal wheel action
        if ((nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)) == KEY_SHIFT)
            bHorz = true;
    }

    m_aWheelData = CommandWheelData(rEvt.mnDelta, rEvt.mnNotchDelta, rEvt.mnScrollLines, nMode,
                                    nCode, bHorz, bPixel);
}

bool HandleWheelEvent::CallCommand(vcl::Window* pWindow, const Point& rMousePos)
{
    Point aCmdMousePos = pWindow->ScreenToOutputPixel(rMousePos);
    CommandEvent aCEvt(aCmdMousePos, CommandEventId::Wheel, true, &m_aWheelData);
    NotifyEvent aNCmdEvt(NotifyEventType::COMMAND, pWindow, &aCEvt);
    bool bPreNotify = ImplCallPreNotify(aNCmdEvt);

    if (pWindow->isDisposed())
        return false;

    if (bPreNotify)
        return false;

    pWindow->ImplGetWindowImpl()->mbCommand = false;
    pWindow->Command(aCEvt);

    if (pWindow->isDisposed())
        return false;

    if (pWindow->ImplGetWindowImpl()->mbCommand)
        return true;

    return false;
}

// If the last event at the same absolute screen position was handled by a
// different window then reuse that window if the event occurs within 1/2 a
// second, i.e. so scrolling down something like the calc sidebar that contains
// widgets that respond to wheel events will continue to send the event to the
// scrolling widget in favour of the widget that happens to end up under the
// mouse.
static bool lcl_ShouldReusePreviousMouseWindow(const SalWheelMouseEvent& rPrevEvt,
                                               const SalWheelMouseEvent& rEvt)
{
    return (rEvt.mnX == rPrevEvt.mnX && rEvt.mnY == rPrevEvt.mnY
            && rEvt.mnTime - rPrevEvt.mnTime < 500 /*ms*/);
}

bool HandleWheelEvent::HandleEvent(const SalWheelMouseEvent& rEvt)
{
    if (!Setup())
        return false;

    VclPtr<vcl::Window> xMouseWindow = FindTarget();

    ImplSVData* pSVData = ImplGetSVData();

    if (lcl_ShouldReusePreviousMouseWindow(pSVData->mpWinData->maLastWheelEvent, rEvt)
        && IsAcceptableWheelScrollTarget(pSVData->mpWinData->mpLastWheelWindow))
    {
        xMouseWindow = pSVData->mpWinData->mpLastWheelWindow;
    }

    pSVData->mpWinData->maLastWheelEvent = rEvt;
    pSVData->mpWinData->mpLastWheelWindow = Dispatch(xMouseWindow);

    return pSVData->mpWinData->mpLastWheelWindow;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
