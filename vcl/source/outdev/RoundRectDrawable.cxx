/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/drawables/RoundRectDrawable.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>

#include <cassert>

bool RoundRectDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    GDIMetaFile* pMetaFile = pRenderContext->GetConnectMetaFile();
    if (pMetaFile)
        pMetaFile->AddAction(mpMetaAction);

    if (!pRenderContext->IsDeviceOutputNecessary()
        || (!pRenderContext->IsLineColor() && !pRenderContext->IsFillColor())
        || pRenderContext->ImplIsRecordLayout())
        return false;

    tools::Rectangle aRect(pRenderContext->ImplLogicToDevicePixel(maRect));

    if (aRect.IsEmpty())
        return false;

    aRect.Justify();

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    if (pRenderContext->IsClipRegionInitialized())
        pRenderContext->InitClipRegion();

    if (pRenderContext->IsOutputClipped())
        return false;

    if (pRenderContext->IsLineColorInitialized())
        pRenderContext->InitLineColor();

    if (pRenderContext->IsFillColorInitialized())
        pRenderContext->InitFillColor();

    sal_uLong nHorzRadius = pRenderContext->ImplLogicWidthToDevicePixel(mnHorzRadius);
    sal_uLong nVertRadius = pRenderContext->ImplLogicWidthToDevicePixel(mnVertRadius);

    if (!nHorzRadius && !nVertRadius)
    {
        pGraphics->DrawRect(aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(),
                            pRenderContext);
    }
    else
    {
        tools::Polygon aRoundRectPoly(aRect, nHorzRadius, nVertRadius);

        if (aRoundRectPoly.GetSize() >= 2)
        {
            SalPoint* pPtAry = reinterpret_cast<SalPoint*>(aRoundRectPoly.GetPointAry());

            if (!pRenderContext->IsFillColor())
                pGraphics->DrawPolyLine(aRoundRectPoly.GetSize(), pPtAry, pRenderContext);
            else
                pGraphics->DrawPolygon(aRoundRectPoly.GetSize(), pPtAry, pRenderContext);
        }
    }

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
        Drawable::Draw(pAlphaVDev, RoundRectDrawable(maRect, mnHorzRadius, mnVertRadius));

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
