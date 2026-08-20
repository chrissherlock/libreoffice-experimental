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

#include <vcl/event.hxx>

#include <window.h>
#include <dndeventdispatcher.hxx>
#include <svdata.hxx>

#include <cassert>

ImplFrameData::ImplFrameData(vcl::Window* pWindow)
    : maPaintIdle("vcl::Window maPaintIdle")
    , maResizeIdle("vcl::Window maResizeIdle")
{
    ImplSVData* pSVData = ImplGetSVData();
    assert(pSVData->maFrameData.mpFirstFrame.get() != pWindow);
    mpNextFrame = pSVData->maFrameData.mpFirstFrame;
    pSVData->maFrameData.mpFirstFrame = pWindow;
    mpFirstOverlap = nullptr;
    mpFocusWin = nullptr;
    mpMouseMoveWin = nullptr;
    mpMouseDownWin = nullptr;
    mpTrackWin = nullptr;
    mxFontCollection = pSVData->maGDIData.mxScreenFontList;
    mxFontCache = pSVData->maGDIData.mxScreenFontCache;
    mnFocusId = nullptr;
    mnMouseMoveId = nullptr;
    mnLastMouseX = -32767;
    mnLastMouseY = -32767;
    mnBeforeLastMouseX = -32767;
    mnBeforeLastMouseY = -32767;
    mnFirstMouseX = -32767;
    mnFirstMouseY = -32767;
    mnLastMouseWinX = -32767;
    mnLastMouseWinY = -32767;
    mnModalMode = 0;
    mnMouseDownTime = 0;
    mnClickCount = 0;
    mnFirstMouseCode = 0;
    mnMouseCode = 0;
    mnMouseMode = MouseEventModifiers::NONE;
    mbHasFocus = false;
    mbInMouseMove = false;
    mbMouseIn = false;
    mbStartDragCalled = false;
    mbNeedSysWindow = false;
    mbMinimized = false;
    mbStartFocusState = false;
    mbInSysObjFocusHdl = false;
    mbInSysObjToTopHdl = false;
    mbSysObjFocus = false;
    maPaintIdle.SetPriority(TaskPriority::REPAINT);
    maPaintIdle.SetInvokeHandler(LINK(pWindow, vcl::Window, ImplHandlePaintHdl));
    maResizeIdle.SetPriority(TaskPriority::RESIZE);
    maResizeIdle.SetInvokeHandler(LINK(pWindow, vcl::Window, ImplHandleResizeTimerHdl));
    mbInternalDragGestureRecognizer = false;
    mbDragging = false;
    mbInBufferedPaint = false;
    mnDPIX = 96;
    mnDPIY = 96;
    mnTouchPanPositionX = -1;
    mnTouchPanPositionY = -1;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
