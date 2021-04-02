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

#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>

void OutputDevice::DrawMask(Point const& rDestPt, Bitmap const& rBitmap, Color const& rMaskColor)
{
    if (ImplIsRecordLayout())
        return;

    const Size aSizePix(rBitmap.GetSizePixel());

    if (meRasterOp == RasterOp::Invert)
    {
        mpMetaFile->AddAction(
            new MetaRectAction(tools::Rectangle(rDestPt, PixelToLogic(aSizePix))));
        return;
    }

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMaskAction(rDestPt, rBitmap, rMaskColor));

    DrawMask(rDestPt, PixelToLogic(aSizePix), Point(), aSizePix, rBitmap, rMaskColor);
}

void OutputDevice::DrawMask(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap,
                            Color const& rMaskColor)
{
    if (ImplIsRecordLayout())
        return;

    if (meRasterOp == RasterOp::Invert)
    {
        mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
        return;
    }

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMaskScaleAction(rDestPt, rDestSize, rBitmap, rMaskColor));

    DrawMask(rDestPt, rDestSize, Point(), rBitmap.GetSizePixel(), rBitmap, rMaskColor);
}

void OutputDevice::DrawMask(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                            Size const& rSrcSizePixel, Bitmap const& rBitmap,
                            Color const& rMaskColor)
{
    if (ImplIsRecordLayout())
        return;

    if (meRasterOp == RasterOp::Invert)
    {
        mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
        return;
    }

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaMaskScalePartAction(rDestPt, rDestSize, rSrcPtPixel,
                                                          rSrcSizePixel, rBitmap, rMaskColor));
    }

    RenderContext2::DrawMask(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, rMaskColor);
}

void OutputDevice::DrawScaledMask(Point const& rDestPt, Size const& rDestSize,
                                  Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                  Bitmap const& rBitmap, Color const& rMaskColor)
{
    if (ImplIsRecordLayout())
        return;

    if (meRasterOp == RasterOp::Invert)
    {
        mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
        return;
    }

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMaskScaleAction(rDestPt, rDestSize, rBitmap, rMaskColor));

    RenderContext2::DrawScaledMask(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap,
                                   rMaskColor);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
