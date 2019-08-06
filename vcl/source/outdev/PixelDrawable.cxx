/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/drawables/PixelDrawable.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>

#include <cassert>

bool PixelDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    Color aColor;
    if (mbUsesColor)
        aColor = pRenderContext->ImplDrawModeToColor(maColor);

    GDIMetaFile* pMetaFile = pRenderContext->GetConnectMetaFile();
    if (pMetaFile)
    {
        pMetaFile->AddAction(mpMetaAction);
    }

    if (!pRenderContext->IsDeviceOutputNecessary() || !pRenderContext->IsLineColor()
        || pRenderContext->ImplIsRecordLayout())
        return false;

    Point aPt = pRenderContext->ImplLogicToDevicePixel(maPt);

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    if (pRenderContext->IsClipRegionInitialized())
        pRenderContext->InitClipRegion();

    if (pRenderContext->IsOutputClipped())
        return false;

    if (pRenderContext->IsLineColor())
        pRenderContext->InitLineColor();

    if (mbUsesColor)
        pGraphics->DrawPixel(aPt.X(), aPt.Y(), aColor, pRenderContext);
    else
        pGraphics->DrawPixel(aPt.X(), aPt.Y(), pRenderContext);

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
    {
        if (mbUsesColor)
        {
            Color aAlphaColor(aColor.GetTransparency(), aColor.GetTransparency(),
                              aColor.GetTransparency());
            Drawable::Draw(pAlphaVDev, PixelDrawable(aPt, aAlphaColor));
        }
        else
        {
            Drawable::Draw(pAlphaVDev, PixelDrawable(aPt));
        }
    }

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
