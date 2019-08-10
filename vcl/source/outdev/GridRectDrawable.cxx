/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with pRenderContext
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/drawables/GridRectDrawable.hxx>
#include <vcl/drawables/RectangleDrawable.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>

#include <cassert>

bool GridRectDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    if (!pRenderContext->IsDeviceOutputNecessary() || pRenderContext->ImplIsRecordLayout())
        return false;

    const long nDistX = std::max(maDistance.Width(), 1L);
    const long nDistY = std::max(maDistance.Height(), 1L);

    tools::Rectangle aDstRect(pRenderContext->PixelToLogic(Point()),
                              pRenderContext->GetOutputSize());
    aDstRect.Intersection(maRect);

    long nX = (maRect.Left() >= aDstRect.Left())
                  ? maRect.Left()
                  : (maRect.Left() + ((aDstRect.Left() - maRect.Left()) / nDistX) * nDistX);
    long nY = (maRect.Top() >= aDstRect.Top())
                  ? maRect.Top()
                  : (maRect.Top() + ((aDstRect.Top() - maRect.Top()) / nDistY) * nDistY);
    const long nRight = aDstRect.Right();
    const long nBottom = aDstRect.Bottom();
    const long nStartX = pRenderContext->ImplLogicXToDevicePixel(nX);
    const long nEndX = pRenderContext->ImplLogicXToDevicePixel(nRight);
    const long nStartY = pRenderContext->ImplLogicYToDevicePixel(nY);
    const long nEndY = pRenderContext->ImplLogicYToDevicePixel(nBottom);
    long nHorzCount = 0;
    long nVertCount = 0;

    std::vector<sal_Int32> aVertBuf;
    std::vector<sal_Int32> aHorzBuf;

    if ((meFlags & DrawGridFlags::Dots) || (meFlags & DrawGridFlags::HorzLines))
    {
        aVertBuf.resize(aDstRect.GetHeight() / nDistY + 2);
        aVertBuf[nVertCount++] = nStartY;
        while ((nY += nDistY) <= nBottom)
        {
            aVertBuf[nVertCount++] = pRenderContext->ImplLogicYToDevicePixel(nY);
        }
    }

    if ((meFlags & DrawGridFlags::Dots) || (meFlags & DrawGridFlags::VertLines))
    {
        aHorzBuf.resize(aDstRect.GetWidth() / nDistX + 2);
        aHorzBuf[nHorzCount++] = nStartX;
        while ((nX += nDistX) <= nRight)
        {
            aHorzBuf[nHorzCount++] = pRenderContext->ImplLogicXToDevicePixel(nX);
        }
    }

    if (pRenderContext->IsLineColorInitialized())
        pRenderContext->InitLineColor();

    if (pRenderContext->IsFillColorInitialized())
        pRenderContext->InitFillColor();

    const bool bOldMap = pRenderContext->IsMapModeEnabled();
    pRenderContext->EnableMapMode(false);

    SalGraphics* pGraphics = pRenderContext->GetGraphics();
    if (!pGraphics)
        return false;

    if (meFlags & DrawGridFlags::Dots)
    {
        for (long i = 0; i < nVertCount; i++)
        {
            for (long j = 0, Y = aVertBuf[i]; j < nHorzCount; j++)
            {
                pGraphics->DrawPixel(aHorzBuf[j], Y, pRenderContext);
            }
        }
    }
    else
    {
        if (meFlags & DrawGridFlags::HorzLines)
        {
            for (long i = 0; i < nVertCount; i++)
            {
                nY = aVertBuf[i];
                pGraphics->DrawLine(nStartX, nY, nEndX, nY, pRenderContext);
            }
        }

        if (meFlags & DrawGridFlags::VertLines)
        {
            for (long i = 0; i < nHorzCount; i++)
            {
                nX = aHorzBuf[i];
                pGraphics->DrawLine(nX, nStartY, nX, nEndY, pRenderContext);
            }
        }
    }

    pRenderContext->EnableMapMode(bOldMap);

    VirtualDevice* pAlphaVDev = pRenderContext->GetAlphaVirtDev();
    if (pAlphaVDev)
        Drawable::Draw(pAlphaVDev, GridRectDrawable(maRect, maDistance, meFlags));

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
