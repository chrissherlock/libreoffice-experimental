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
#include <sal/log.hxx>
#include <tools/debug.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/metaact.hxx>
#include <vcl/rendercontext/State.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>

void OutputDevice::Push(vcl::PushFlags nFlags)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaPushAction(nFlags));

    // Snapshot the MVCC RenderState
    // We create a new frame, save the requested mask, and take a full copy
    // of the current authoritative state. This is copy-by-value and
    // is extremely efficient for modern CPUs.
    vcl::rstate::PushFrame aFrame;
    aFrame.isolationMask = nFlags;
    aFrame.renderSnapshot = m_aRenderState;

    aFrame.renderDelta = {};

    m_aPushFrames.push_back(aFrame);

    // Bridge to legacy state
    // We call LegacyPush to handle non-refactored state like Fonts, Clips, and MapModes.
    LegacyPush(nFlags);
}

void OutputDevice::LegacyPush(vcl::PushFlags nFlags)
{
    maOutDevStateStack.emplace_back();
    vcl::State& rState = maOutDevStateStack.back();

    rState.mnFlags = nFlags;

    // Note: We intentionally skip LINECOLOR, FILLCOLOR, and RASTEROP
    // because they are now handled by the new vcl::rstate::RenderState pipeline.

    if (nFlags & vcl::PushFlags::FONT)
        rState.mpFont = maFont;

    if (nFlags & vcl::PushFlags::TEXTCOLOR)
        rState.mpTextColor = GetTextColor();

    if (nFlags & vcl::PushFlags::TEXTFILLCOLOR && IsTextFillColor())
        rState.mpTextFillColor = GetTextFillColor();

    if (nFlags & vcl::PushFlags::TEXTLINECOLOR && IsTextLineColor())
        rState.mpTextLineColor = GetTextLineColor();

    if (nFlags & vcl::PushFlags::OVERLINECOLOR && IsOverlineColor())
        rState.mpOverlineColor = GetOverlineColor();

    if (nFlags & vcl::PushFlags::TEXTALIGN)
        rState.meTextAlign = GetTextAlign();

    if (nFlags & vcl::PushFlags::TEXTLAYOUTMODE)
        rState.mnTextLayoutMode = GetLayoutMode();

    if (nFlags & vcl::PushFlags::TEXTLANGUAGE)
        rState.meTextLanguage = GetDigitLanguage();

    if (nFlags & vcl::PushFlags::MAPMODE)
    {
        rState.mpMapMode = maMapMode;
        rState.meMapMode = GetMappingPolicy();
    }

    if (nFlags & vcl::PushFlags::CLIPREGION && mbClipRegion)
        rState.mpClipRegion.reset(new vcl::Region(maRegion));

    if (nFlags & vcl::PushFlags::REFPOINT && mbRefPoint)
        rState.mpRefPoint = maRefPoint;
}

void OutputDevice::Pop()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaPopAction());

    GDIMetaFile* pOldMetaFile = mpMetaFile;
    mpMetaFile = nullptr; // Suspend recording

    comphelper::ScopeGuard aMetaGuard([this, pOldMetaFile]() {
        this->mpMetaFile = pOldMetaFile; // Restore recording on exit
    });

    if (m_aPushFrames.empty())
    {
        SAL_WARN("vcl.gdi", "OutputDevice::Pop() without OutputDevice::Push()");
        return;
    }

    vcl::rstate::PushFrame rFrame = m_aPushFrames.back();
    m_aPushFrames.pop_back();

    bool bStateReconciled = false;

    if ((rFrame.isolationMask & vcl::PushFlags::LINECOLOR) &&
        (m_aRenderState.lineColor != rFrame.renderSnapshot.lineColor ||
         m_aRenderState.bLineColorSet != rFrame.renderSnapshot.bLineColorSet))
    {
        m_aRenderState.lineColor = rFrame.renderSnapshot.lineColor;
        m_aRenderState.bLineColorSet = rFrame.renderSnapshot.bLineColorSet;
        m_aRenderState.changeMask |= vcl::rstate::RenderChangeMask::LineColor;
        bStateReconciled = true;
    }

    if ((rFrame.isolationMask & vcl::PushFlags::FILLCOLOR) &&
        (m_aRenderState.fillColor != rFrame.renderSnapshot.fillColor ||
         m_aRenderState.bFillColorSet != rFrame.renderSnapshot.bFillColorSet))
    {
        m_aRenderState.fillColor = rFrame.renderSnapshot.fillColor;
        m_aRenderState.bFillColorSet = rFrame.renderSnapshot.bFillColorSet;
        m_aRenderState.changeMask |= vcl::rstate::RenderChangeMask::FillColor;
        bStateReconciled = true;
    }

    if ((rFrame.isolationMask & vcl::PushFlags::RASTEROP) &&
        (m_aRenderState.rasterOp != rFrame.renderSnapshot.rasterOp))
    {
        m_aRenderState.rasterOp = rFrame.renderSnapshot.rasterOp;
        m_aRenderState.changeMask |= vcl::rstate::RenderChangeMask::RasterOp;
        bStateReconciled = true;
    }

    if (bStateReconciled)
    {
        m_aRenderState.epoch++;
        // Force an immediate flush to the backend to prevent subsequent
        // direct-backend calls from inheriting stale ROP or Color states.
        SyncRenderStateToBackend();
    }

    LegacyPop();
}

void OutputDevice::LegacyPop()
{
    // This is the original logic that was previously inside OutputDevice::Pop()
    // but moved here so we can call it after our MVCC reconciliation.

    if (maOutDevStateStack.empty())
    {
        SAL_WARN("vcl.gdi", "OutputDevice::LegacyPop() without OutputDevice::Push()");
        return;
    }

    const vcl::State& rState = maOutDevStateStack.back();

    if (rState.mnFlags & vcl::PushFlags::FONT)
        SetFont(*rState.mpFont);

    if (rState.mnFlags & vcl::PushFlags::TEXTCOLOR)
        SetTextColor(*rState.mpTextColor);

    if (rState.mnFlags & vcl::PushFlags::TEXTFILLCOLOR)
    {
        if (rState.mpTextFillColor)
            SetTextFillColor(*rState.mpTextFillColor);
        else
            SetTextFillColor();
    }

    if (rState.mnFlags & vcl::PushFlags::TEXTLINECOLOR)
    {
        if (rState.mpTextLineColor)
            SetTextLineColor(*rState.mpTextLineColor);
        else
            SetTextLineColor();
    }

    if (rState.mnFlags & vcl::PushFlags::OVERLINECOLOR)
    {
        if (rState.mpOverlineColor)
            SetOverlineColor(*rState.mpOverlineColor);
        else
            SetOverlineColor();
    }

    if (rState.mnFlags & vcl::PushFlags::TEXTALIGN)
        SetTextAlign(rState.meTextAlign);

    if (rState.mnFlags & vcl::PushFlags::TEXTLAYOUTMODE)
        SetLayoutMode(rState.mnTextLayoutMode);

    if (rState.mnFlags & vcl::PushFlags::TEXTLANGUAGE)
        SetDigitLanguage(rState.meTextLanguage);

    if (rState.mnFlags & vcl::PushFlags::MAPMODE)
    {
        if (rState.mpMapMode)
            SetMapMode(*rState.mpMapMode);
        else
            SetMapMode();
        SetMappingPolicy(rState.meMapMode);
    }

    if (rState.mnFlags & vcl::PushFlags::CLIPREGION)
        SetDeviceClipRegion(rState.mpClipRegion.get());

    if (rState.mnFlags & vcl::PushFlags::REFPOINT)
    {
        if (rState.mpRefPoint)
            SetRefPoint(*rState.mpRefPoint);
        else
            SetRefPoint();
    }

    maOutDevStateStack.pop_back();
}

void OutputDevice::ClearStack()
{
    // Drain the primary MVCC stack.
    // Pop() internally handles the Metafile recording, state reconciliation,
    // and automatically calls LegacyPop() for the legacy state variables.
    while ( !m_aPushFrames.empty() )
    {
        Pop();
    }

    // Failsafe: Drain any orphaned legacy state frames.
    // This guarantees we satisfy the ~OutputDevice() assertion that
    // Push() calls == Pop() calls, even if the stacks somehow desynced.
    while ( !maOutDevStateStack.empty() )
    {
        LegacyPop();
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
