/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

bool RenderContext2::DrawTransformBitmapExDirect(basegfx::B2DHomMatrix const& aFullTransform,
                                                 BitmapEx const& rBitmapEx, double fAlpha)
{
    assert(!is_double_buffered_window());

    bool bDone = false;

    // try to paint directly
    const basegfx::B2DPoint aNull(aFullTransform * basegfx::B2DPoint(0.0, 0.0));
    const basegfx::B2DPoint aTopX(aFullTransform * basegfx::B2DPoint(1.0, 0.0));
    const basegfx::B2DPoint aTopY(aFullTransform * basegfx::B2DPoint(0.0, 1.0));
    SalBitmap* pSalSrcBmp = rBitmapEx.GetBitmap().ImplGetSalBitmap().get();
    Bitmap aAlphaBitmap;

    if (rBitmapEx.IsTransparent())
    {
        if (rBitmapEx.IsAlpha())
        {
            aAlphaBitmap = rBitmapEx.GetAlpha();
        }
        else
        {
            aAlphaBitmap = rBitmapEx.GetMask();
        }
    }
    else if (mpAlphaVDev)
    {
        aAlphaBitmap = AlphaMask(rBitmapEx.GetSizePixel());
        aAlphaBitmap.Erase(COL_BLACK); // opaque
    }

    SalBitmap* pSalAlphaBmp = aAlphaBitmap.ImplGetSalBitmap().get();

    bDone = mpGraphics->DrawTransformedBitmap(aNull, aTopX, aTopY, *pSalSrcBmp, pSalAlphaBmp,
                                              fAlpha, *this);

    if (mpAlphaVDev)
    {
        // Merge bitmap alpha to alpha device
        AlphaMask aAlpha(rBitmapEx.GetSizePixel());
        aAlpha.Erase((1 - fAlpha) * 255);
        mpAlphaVDev->DrawTransformBitmapExDirect(aFullTransform, BitmapEx(aAlpha, aAlphaBitmap));
    }

    return bDone;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
