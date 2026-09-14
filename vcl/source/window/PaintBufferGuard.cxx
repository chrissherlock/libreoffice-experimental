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

#include <tools/mapunit.hxx>

#if HAVE_FEATURE_OPENGL
#include <vcl/opengl/OpenGLHelper.hxx>
#endif

#include <window/window.h>
#include <window/ImplFrameData.hxx>
#include <window/PaintBufferGuard.hxx>

namespace vcl
{
PaintBufferGuard::PaintBufferGuard(ImplFrameData* pFrameData, vcl::Window* pWindow)
    : mpFrameData(pFrameData)
    , m_pWindow(pWindow)
    , mbBackground(false)
    , mnOutOffX(0)
    , mnOutOffY(0)
{
    if (!pFrameData->mpBuffer)
        return;

    // transfer various settings
    // FIXME: this must disappear as we move to RenderContext only,
    // the painting must become state-less, so that no actual
    // vcl::Window setting affects this
    mbBackground = pFrameData->mpBuffer->HasBackground();
    if (pWindow->HasBackground())
    {
        maBackground = pFrameData->mpBuffer->GetBackground();
        pFrameData->mpBuffer->SetBackground(pWindow->GetBackground());
    }
    //else
    //SAL_WARN("vcl.window", "the root of the double-buffering hierarchy should not have a transparent background");

    vcl::PushFlags nFlags = vcl::PushFlags::NONE;
    nFlags |= vcl::PushFlags::CLIPREGION;
    nFlags |= vcl::PushFlags::FILLCOLOR;
    nFlags |= vcl::PushFlags::FONT;
    nFlags |= vcl::PushFlags::LINECOLOR;
    nFlags |= vcl::PushFlags::MAPMODE;
    maSettings = pFrameData->mpBuffer->GetSettings();
    nFlags |= vcl::PushFlags::REFPOINT;
    nFlags |= vcl::PushFlags::TEXTCOLOR;
    nFlags |= vcl::PushFlags::TEXTLINECOLOR;
    nFlags |= vcl::PushFlags::OVERLINECOLOR;
    nFlags |= vcl::PushFlags::TEXTFILLCOLOR;
    nFlags |= vcl::PushFlags::TEXTALIGN;
    nFlags |= vcl::PushFlags::RASTEROP;
    nFlags |= vcl::PushFlags::TEXTLAYOUTMODE;
    nFlags |= vcl::PushFlags::TEXTLANGUAGE;
    pFrameData->mpBuffer->Push(nFlags);
    auto& rDev = *pWindow->GetOutDev();
    pFrameData->mpBuffer->SetClipRegion(rDev.GetClipRegion());
    pFrameData->mpBuffer->SetFillColor(rDev.GetFillColor());
    pFrameData->mpBuffer->SetFont(pWindow->GetFont());
    if (!rDev.HasAlpha() && rDev.GetLineColor() == COL_TRANSPARENT)
        pFrameData->mpBuffer->SetLineColor();
    else
        pFrameData->mpBuffer->SetLineColor(rDev.GetLineColor());
    pFrameData->mpBuffer->SetMapMode(pWindow->GetMapMode());
    pFrameData->mpBuffer->SetReferencePoint(rDev.GetReferencePoint());
    pFrameData->mpBuffer->SetSettings(pWindow->GetSettings());
    pFrameData->mpBuffer->SetTextColor(pWindow->GetTextColor());
    pFrameData->mpBuffer->SetTextLineColor(pWindow->GetTextLineColor());
    pFrameData->mpBuffer->SetOverlineColor(pWindow->GetOverlineColor());
    pFrameData->mpBuffer->SetTextFillColor(pWindow->GetTextFillColor());
    pFrameData->mpBuffer->SetTextAlign(pWindow->GetTextAlign());
    pFrameData->mpBuffer->SetRasterOp(rDev.GetRasterOp());
    pFrameData->mpBuffer->SetLayoutMode(rDev.GetLayoutMode());
    pFrameData->mpBuffer->SetDigitLanguage(rDev.GetDigitLanguage());

    mnOutOffX = pFrameData->mpBuffer->GetDeviceOriginX();
    mnOutOffY = pFrameData->mpBuffer->GetDeviceOriginY();
    pFrameData->mpBuffer->SetDeviceOriginX(pWindow->GetDeviceOriginX());
    pFrameData->mpBuffer->SetDeviceOriginY(pWindow->GetDeviceOriginY());
    pFrameData->mpBuffer->EnableRTL(pWindow->IsRTLEnabled());
}

PaintBufferGuard::~PaintBufferGuard()
{
    if (!mpFrameData->mpBuffer)
        return;

    if (!m_aPaintRect.IsEmpty())
    {
        // copy the buffer content to the actual window
        // export VCL_DOUBLEBUFFERING_AVOID_PAINT=1 to see where we are
        // painting directly instead of using Invalidate()
        // [ie. everything you can see was painted directly to the
        // window either above or in eg. an event handler]
        if (!getenv("VCL_DOUBLEBUFFERING_AVOID_PAINT"))
        {
            // Make sure that the +1 value GetSize() adds to the size is in pixels.
            Size aPaintRectSize;
            if (m_pWindow->GetMapMode().GetMapUnit() == MapUnit::MapPixel)
            {
                aPaintRectSize = m_aPaintRect.GetSize();
            }
            else
            {
                const auto aRectanglePixel = m_pWindow->convertTo<vcl::WindowRect>(
                    vcl::LogicRect(m_aPaintRect), m_pWindow->GetMapMode());

                aPaintRectSize = m_pWindow->convertTo<vcl::LogicSize>(
                    vcl::WindowSize(aRectanglePixel->GetSize()));
            }

            m_pWindow->GetOutDev()->DrawOutDev(m_aPaintRect.TopLeft(), aPaintRectSize,
                                               m_aPaintRect.TopLeft(), aPaintRectSize,
                                               *mpFrameData->mpBuffer);
        }
    }

    // Restore buffer state.
    mpFrameData->mpBuffer->SetDeviceOriginX(mnOutOffX);
    mpFrameData->mpBuffer->SetDeviceOriginY(mnOutOffY);

    mpFrameData->mpBuffer->Pop();
    mpFrameData->mpBuffer->SetSettings(maSettings);
    if (mbBackground)
        mpFrameData->mpBuffer->SetBackground(maBackground);
    else
        mpFrameData->mpBuffer->SetBackground();
}

void PaintBufferGuard::SetPaintRect(const tools::Rectangle& rRectangle)
{
    m_aPaintRect = rRectangle;
}

vcl::RenderContext* PaintBufferGuard::GetRenderContext()
{
    if (mpFrameData->mpBuffer)
        return mpFrameData->mpBuffer;
    else
        return m_pWindow->GetOutDev();
}
} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
