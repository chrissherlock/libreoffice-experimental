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

#include <config_features.h>

#include <vcl/window.hxx>

#if HAVE_FEATURE_OPENGL
#include <vcl/opengl/OpenGLHelper.hxx>
#endif

#include <ImplFrameData.hxx>
#include <ImplWinData.hxx>
#include <PaintBufferGuard.hxx>
#include <WindowVisibilityState.hxx>
#include <WindowImpl.hxx>
#include <WindowInvalidation.hxx>
#include <WindowHierarchy.hxx>
#include <clipping_window.hxx>

#include "PaintHelper.hxx"

PaintHelper::PaintHelper(vcl::Window* pWindow, ImplPaintFlags nPaintFlags)
    : m_pWindow(pWindow)
    , m_nPaintFlags(nPaintFlags)
    , m_bPop(false)
    , m_bRestoreCursor(false)
    , m_bStartedBufferedPaint(false)
{
}

PaintHelper::~PaintHelper()
{
    WindowImpl* pWindowImpl = m_pWindow->ImplGetWindowImpl();
    ImplWinData* pWinData = m_pWindow->ImplGetWinData();

    if (m_bPop)
        m_pWindow->PopPaintHelper(this);

    ImplFrameData* pFrameData = pWindowImpl->mpFrameData;
    if (m_nPaintFlags & (ImplPaintFlags::PaintAllChildren | ImplPaintFlags::PaintChildren))
    {
        // Paint from the bottom child window and frontward.
        vcl::Window* pTempWindow = m_pWindow->ImplGetWindowHierarchy()->mpLastChild;
        while (pTempWindow)
        {
            if (pTempWindow->ImplGetVisibilityState()->mbVisible)
                pTempWindow->ImplCallPaint(m_pChildRegion.get(), m_nPaintFlags);
            pTempWindow = pTempWindow->ImplGetWindowHierarchy()->mpPrev;
        }
    }

    if (pWinData && pWindowImpl->mbTrackVisible
        && (pWinData->mnTrackFlags & ShowTrackFlags::TrackWindow))
        /* #98602# need to invert the tracking rect AFTER
         * the children have painted
         */
        m_pWindow->InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);

    // double-buffering: paint in case we created the buffer, the children are
    // already painted inside
    if (m_bStartedBufferedPaint && pFrameData->mbInBufferedPaint)
    {
        PaintBuffer();
        pFrameData->mbInBufferedPaint = false;
        pFrameData->maBufferedRect = tools::Rectangle();
    }

    // #98943# draw toolbox selection
    if (!m_aSelectionRect.IsEmpty())
    {
        m_pWindow->GetOutDev()->DrawSelectionBackground(
            m_aSelectionRect, m_pWindow->GetBackgroundColor(), 3, false, true);
    }
}

void PaintHelper::StartBufferedPaint()
{
    ImplFrameData* pFrameData = m_pWindow->mpWindowImpl->mpFrameData;
    assert(!pFrameData->mbInBufferedPaint);

    pFrameData->mbInBufferedPaint = true;
    pFrameData->maBufferedRect = tools::Rectangle();
    m_bStartedBufferedPaint = true;
}

void PaintHelper::PaintBuffer()
{
    ImplFrameData* pFrameData = m_pWindow->mpWindowImpl->mpFrameData;
    assert(pFrameData->mbInBufferedPaint);
    assert(m_bStartedBufferedPaint);

    vcl::PaintBufferGuard aGuard(pFrameData, m_pWindow);
    aGuard.SetPaintRect(pFrameData->maBufferedRect);
}

void PaintHelper::DoPaint(const vcl::Region* pRegion)
{
    WindowImpl* pWindowImpl = m_pWindow->ImplGetWindowImpl();
    WindowInvalidation* pInvalidation = m_pWindow->ImplGetWindowInvalidation();
    ImplWinData* pWinData = m_pWindow->ImplGetWinData();

    vcl::Region& rWinChildClipRegion = vcl::clipping::getWinChildClipRegion(*m_pWindow);
    ImplFrameData* pFrameData = m_pWindow->mpWindowImpl->mpFrameData;

    if (pInvalidation->mnPaintFlags & ImplPaintFlags::PaintAll || pFrameData->mbInBufferedPaint)
    {
        pInvalidation->maInvalidateRegion = rWinChildClipRegion;
    }
    else
    {
        if (pRegion)
            pInvalidation->maInvalidateRegion.Union(*pRegion);

        if (pWinData && pWindowImpl->mbTrackVisible)
        {
            /* #98602# need to repaint all children within the
            * tracking rectangle, so the following invert
            * operation takes places without traces of the previous
            * one.
            */
            pInvalidation->maInvalidateRegion.Union(*pWinData->mpTrackRect);
        }

        if (pInvalidation->mnPaintFlags & ImplPaintFlags::PaintAllChildren)
            m_pChildRegion.reset(new vcl::Region(pInvalidation->maInvalidateRegion));

        pInvalidation->maInvalidateRegion.Intersect(rWinChildClipRegion);
    }

    pInvalidation->mnPaintFlags = ImplPaintFlags::NONE;

    if (pInvalidation->maInvalidateRegion.IsEmpty())
        return;

#if HAVE_FEATURE_OPENGL
    VCL_GL_INFO("PaintHelper::DoPaint on " << typeid(*m_pWindow).name() << " '"
                                           << m_pWindow->GetText() << "' begin");
#endif
    // double-buffering: setup the buffer if it does not exist
    if (!pFrameData->mbInBufferedPaint && m_pWindow->SupportsDoubleBuffering())
        StartBufferedPaint();

    // double-buffering: if this window does not support double-buffering,
    // but we are in the middle of double-buffered paint, we might be
    // losing information
    if (pFrameData->mbInBufferedPaint && !m_pWindow->SupportsDoubleBuffering())
        SAL_WARN("vcl.window",
                 "non-double buffered window in the double-buffered hierarchy, painting directly: "
                     << typeid(*m_pWindow.get()).name());

    if (pFrameData->mbInBufferedPaint && m_pWindow->SupportsDoubleBuffering())
    {
        // double-buffering
        vcl::PaintBufferGuard g(pFrameData, m_pWindow);
        m_pWindow->ApplySettings(*pFrameData->mpBuffer);

        m_pWindow->PushPaintHelper(this, *pFrameData->mpBuffer);
        m_pWindow->Paint(*pFrameData->mpBuffer, m_aPaintRect);
        pFrameData->maBufferedRect.Union(m_aPaintRect);
    }
    else
    {
        // direct painting
        Wallpaper aBackground = m_pWindow->GetBackground();
        m_pWindow->ApplySettings(*m_pWindow->GetOutDev());
        // Restore bitmap background if it was lost.
        if (aBackground.IsBitmap() && !m_pWindow->GetBackground().IsBitmap())
        {
            m_pWindow->SetBackground(aBackground);
        }
        m_pWindow->PushPaintHelper(this, *m_pWindow->GetOutDev());
        m_pWindow->Paint(*m_pWindow->GetOutDev(), m_aPaintRect);
    }
#if HAVE_FEATURE_OPENGL
    VCL_GL_INFO("PaintHelper::DoPaint end on " << typeid(*m_pWindow).name() << " '"
                                               << m_pWindow->GetText() << "'");
#endif
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
