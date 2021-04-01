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

#include <osl/diagnose.h>
#include <rtl/math.hxx>
#include <tools/debug.hxx>
#include <tools/helpers.hxx>
#include <o3tl/unit_conversion.hxx>

#include <vcl/BitmapMonochromeFilter.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/image.hxx>
#include <vcl/metaact.hxx>
#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/virdev.hxx>

#include <bitmap/BitmapWriteAccess.hxx>
#include <bitmap/bmpfast.hxx>
#include <salbmp.hxx>
#include <salgdi.hxx>

#include <memory>

void OutputDevice::DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap)
{
    if (ImplIsRecordLayout())
        return;

    if (ProcessBitmapRasterOpInvert(rDestPt, PixelToLogic(rBitmap.GetSizePixel())))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, PixelToLogic(rBitmap.GetSizePixel())))
        return;

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaBmpAction(rDestPt, ProcessBitmapDrawModeGray(rBitmap)));

    DrawBitmap(rDestPt, PixelToLogic(rBitmap.GetSizePixel()), Point(),
               PixelToLogic(rBitmap.GetSizePixel()), rBitmap);
}

void OutputDevice::DrawBitmap(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap)
{
    if (ImplIsRecordLayout())
        return;

    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(
            new MetaBmpScaleAction(rDestPt, rDestSize, ProcessBitmapDrawModeGray(rBitmap)));
    }

    RenderContext2::DrawBitmap(rDestPt, rDestSize, rBitmap);
}

void OutputDevice::DrawBitmap(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                              Size const& rSrcSizePixel, Bitmap const& rBitmap)
{
    if (ImplIsRecordLayout())
        return;

    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaBmpScalePartAction(
            rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, ProcessBitmapDrawModeGray(rBitmap)));
    }

    if (ImplIsRecordLayout())
        return;

    RenderContext2::DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap);
}

void OutputDevice::DrawScaledBitmap(Point const& rDestPt, Size const& rDestSize,
                                    Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                    Bitmap const& rBitmap)
{
    if (ImplIsRecordLayout())
        return;

    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(
            new MetaBmpScaleAction(rDestPt, rDestSize, ProcessBitmapDrawModeGray(rBitmap)));
    }

    RenderContext2::DrawScaledBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
