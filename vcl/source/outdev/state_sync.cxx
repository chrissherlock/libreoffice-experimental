/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>

#include <vcl/outdev.hxx>
#include <vcl/rendercontext/State.hxx>
#include <vcl/state/RenderState.hxx>

#include <salgdi.hxx>

void OutputDevice::SyncRenderStateToBackend() const
{
    if (!mpGraphics)
        return;

    // We only perform sync operations if there is something actually dirty.
    // This allows unit tests that manipulate the backend directly to proceed
    // without being overwritten by stale RenderState values.
    if (m_aRenderState.changeMask == vcl::rstate::RenderChangeMask::None)
        return;

    if (m_aRenderState.changeMask & vcl::rstate::RenderChangeMask::RasterOp)
    {
        bool bInvert = (m_aRenderState.rasterOp == RasterOp::Invert);
        bool bXor = (m_aRenderState.rasterOp == RasterOp::Xor);
        mpGraphics->SetXORMode(bInvert || bXor, bInvert);

        // ROP changes require refreshing both color states
        m_aRenderState.changeMask |= vcl::rstate::RenderChangeMask::LineColor;
        m_aRenderState.changeMask |= vcl::rstate::RenderChangeMask::FillColor;

        m_aRenderState.changeMask &= ~vcl::rstate::RenderChangeMask::RasterOp;
    }

    if (m_aRenderState.changeMask & vcl::rstate::RenderChangeMask::LineColor)
    {
        if (!m_aRenderState.bLineColorSet)
        {
            mpGraphics->SetLineColor();
        }
        else
        {
            switch (m_aRenderState.rasterOp)
            {
                case RasterOp::N0:
                    mpGraphics->SetROPLineColor(SalROPColor::N0);
                    break;
                case RasterOp::N1:
                    mpGraphics->SetROPLineColor(SalROPColor::N1);
                    break;
                case RasterOp::Invert:
                    mpGraphics->SetROPLineColor(SalROPColor::Invert);
                    break;
                default:
                    mpGraphics->SetLineColor(m_aRenderState.lineColor);
                    break;
            }
        }

        m_aRenderState.changeMask &= ~vcl::rstate::RenderChangeMask::LineColor;
    }

    if (m_aRenderState.changeMask & vcl::rstate::RenderChangeMask::FillColor)
    {
        if (!m_aRenderState.bFillColorSet)
        {
            mpGraphics->SetFillColor();
        }
        else
        {
            switch (m_aRenderState.rasterOp)
            {
                case RasterOp::N0:
                    mpGraphics->SetROPFillColor(SalROPColor::N0);
                    break;
                case RasterOp::N1:
                    mpGraphics->SetROPFillColor(SalROPColor::N1);
                    break;
                case RasterOp::Invert:
                    mpGraphics->SetROPFillColor(SalROPColor::Invert);
                    break;
                default:
                    mpGraphics->SetFillColor(m_aRenderState.fillColor);
                    break;
            }
        }

        m_aRenderState.changeMask &= ~vcl::rstate::RenderChangeMask::FillColor;
    }

    m_aRenderState.lastSyncedEpoch = m_aRenderState.epoch;
}

void OutputDevice::ResetRenderStateSync() const
{
    m_aRenderState.changeMask = vcl::rstate::RenderChangeMask::All;
    m_aRenderState.lastSyncedEpoch = 0;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
