/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <vcl/drawables/CheckeredRectDrawable.hxx>
#include <vcl/drawables/RectangleDrawable.hxx>
#include <vcl/outdev.hxx>

#include <cassert>

bool CheckeredRectDrawable::execute(OutputDevice* pRenderContext) const
{
    assert(!pRenderContext->is_double_buffered_window());

    const sal_uInt32 nMaxX(maPos.X() + maSize.Width());
    const sal_uInt32 nMaxY(maPos.Y() + maSize.Height());

    pRenderContext->Push(PushFlags::LINECOLOR | PushFlags::FILLCOLOR);
    pRenderContext->SetLineColor();

    bool bRet = true;

    for (sal_uInt32 x(0), nX(maPos.X()); nX < nMaxX; x++, nX += mnLen)
    {
        const sal_uInt32 nRight(std::min(nMaxX, nX + mnLen));

        for (sal_uInt32 y(0), nY(maPos.Y()); nY < nMaxY; y++, nY += mnLen)
        {
            const sal_uInt32 nBottom(std::min(nMaxY, nY + mnLen));

            pRenderContext->SetFillColor(((x & 0x0001) ^ (y & 0x0001)) ? maStartColor : maEndColor);
            Drawable::Draw(pRenderContext,
                           RectangleDrawable(tools::Rectangle(nX, nY, nRight, nBottom)));
        }
    }

    pRenderContext->Pop();

    return bRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
