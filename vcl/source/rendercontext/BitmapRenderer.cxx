/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/bitmap.hxx>

#include <salgdi.hxx>

namespace vcl::rendercontext
{
void BitmapRenderer::DrawBitmap(SalGraphics& rGraphics, const SalTwoRect& rPosAry,
                                const Bitmap& rBitmap)
{
    if (rBitmap.IsEmpty())
        return;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return;

    rGraphics.drawBitmap(rPosAry, *pSalBitmap);
}

void BitmapRenderer::DrawAlphaBitmap(SalGraphics& rGraphics, const SalTwoRect& rPosAry,
                                     const Bitmap& rBitmap)
{
    if (rBitmap.IsEmpty())
        return;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return;

    // Bitmap now natively holds its own alpha format internally, so we
    // just pass the SalBitmap directly to the alpha sink.
    rGraphics.drawAlphaBitmap(rPosAry, *pSalBitmap);
}

bool BitmapRenderer::DrawTransformedBitmap(SalGraphics& rGraphics, const basegfx::B2DPoint& rNull,
                                           const basegfx::B2DPoint& rX, const basegfx::B2DPoint& rY,
                                           const Bitmap& rBitmap, double fAlpha)
{
    if (rBitmap.IsEmpty())
        return true;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return false;

    return rGraphics.drawTransformedBitmap(rNull, rX, rY, *pSalBitmap, fAlpha);
}

void BitmapRenderer::DrawMask(SalGraphics& rGraphics, const SalTwoRect& rPosAry,
                              const Bitmap& rBitmap, const Color& rMaskColor)
{
    if (rBitmap.IsEmpty())
        return;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return;

    rGraphics.drawMask(rPosAry, *pSalBitmap, rMaskColor);
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
