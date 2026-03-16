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

#include <config_features.h>

#include <vcl/alpha.hxx>
#include <vcl/image.hxx>
#include <vcl/outdev.hxx>

#include <bitmap/BitmapColorizeFilter.hxx>

void OutputDevice::DrawImage(const Point& rPos, const Image& rImage, DrawImageFlags nStyle)
{
    assert(!is_double_buffered_window());

    DrawImage(rPos, Size(), rImage, nStyle);
}

namespace
{
constexpr DrawImageFlags UI_EFFECTS_MASK = DrawImageFlags::Highlight | DrawImageFlags::Deactive
                                           | DrawImageFlags::SemiTransparent
                                           | DrawImageFlags::Invert;

constexpr DrawImageFlags UI_THEME_TINT_MASK = DrawImageFlags::Highlight | DrawImageFlags::Deactive;
}

static void lcl_ApplyUIStylesToBitmap(Bitmap& rBitmap, DrawImageFlags nStyle,
                                      const StyleSettings& rSettings)
{
    if (nStyle & DrawImageFlags::Disable)
        return;

    if (!(nStyle & UI_EFFECTS_MASK))
        return;

    if (nStyle & UI_THEME_TINT_MASK)
    {
        Color aColor = (nStyle & DrawImageFlags::Highlight) ? rSettings.GetHighlightColor()
                                                            : rSettings.GetDeactiveColor();
        BitmapFilter::Filter(rBitmap, BitmapColorizeFilter(aColor));
    }

    if (nStyle & DrawImageFlags::SemiTransparent)
    {
        Bitmap aTempBitmap(rBitmap);

        if (aTempBitmap.HasAlpha())
        {
            Bitmap aAlphaBmp(aTempBitmap.CreateAlphaMask().GetBitmap());
            aAlphaBmp.Adjust(50);
            aTempBitmap = Bitmap(aTempBitmap.CreateColorBitmap(), AlphaMask(aAlphaBmp));
        }
        else
        {
            sal_uInt8 cErase = 128;
            aTempBitmap = Bitmap(aTempBitmap, AlphaMask(aTempBitmap.GetSizePixel(), &cErase));
        }

        rBitmap = std::move(aTempBitmap);
    }

    if (nStyle & DrawImageFlags::Invert)
        rBitmap.Adjust(0, 0, 0, 0, 0, 0, true, false);
}

void OutputDevice::DrawImage(const Point& rPos, const Size& rSize, const Image& rImage,
                             DrawImageFlags nStyle)
{
    assert(!is_double_buffered_window());

    if (rImage.IsEmpty() || rSize.IsEmpty())
        return;

    // We need the SalGraphics context to ask the Image for the correct HiDPI variant.
    // However, if we are purely recording a metafile, we won't have (or need) a hardware context.
    SalGraphics* pGraphics = nullptr;

    if (IsDeviceOutputNecessary())
    {
        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::None) || !mpGraphics)
            return;

        pGraphics = mpGraphics;
    }
    else if (GetConnectMetaFile() == nullptr)
    {
        return;
    }

    // Extract the Bitmap (uses SalGraphics for HiDPI if available, otherwise falls back to standard)
    Bitmap aRenderBmp = rImage.GetBitmapForHiDPI(bool(nStyle & DrawImageFlags::Disable), pGraphics);

    if (aRenderBmp.IsEmpty())
        return;

    lcl_ApplyUIStylesToBitmap(aRenderBmp, nStyle, GetSettings().GetStyleSettings());

    DrawBitmap(rPos, rSize, aRenderBmp);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
