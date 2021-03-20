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

#include <sal/types.h>
#include <tools/helpers.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/polygon/WaveLine.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>
#include <impglyphitem.hxx>

#include <cassert>

void OutputDevice::ImplInitTextLineSize()
{
    mpFontInstance->mxFontMetric->ImplInitTextLineSize(this);
}

void OutputDevice::ImplInitAboveTextLineSize()
{
    mpFontInstance->mxFontMetric->ImplInitAboveTextLineSize();
}

void OutputDevice::SetTextLineColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLineColorAction(Color(), false));

    RenderContext2::SetTextLineColor();
}

void OutputDevice::SetTextLineColor(const Color& rColor)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLineColorAction(
            GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()), true));

    RenderContext2::SetTextLineColor(rColor);
}

void OutputDevice::SetOverlineColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaOverlineColorAction(Color(), false));

    RenderContext2::SetOverlineColor();
}

void OutputDevice::SetOverlineColor(Color const& rColor)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaOverlineColorAction(
            GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()), true));

    RenderContext2::SetOverlineColor(rColor);
}

void OutputDevice::DrawTextLine(const Point& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                                FontLineStyle eUnderline, FontLineStyle eOverline,
                                bool bUnderlineAbove)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(
            new MetaTextLineAction(rPos, nWidth, eStrikeout, eUnderline, eOverline));
    }

    RenderContext2::DrawTextLine(rPos, nWidth, eStrikeout, eUnderline, eOverline, bUnderlineAbove);
}

void OutputDevice::DrawWaveLine(const Point& rStartPos, const Point& rEndPos,
                                tools::Long nLineWidth)
{
    assert(!is_double_buffered_window());

    if (!IsDeviceOutputNecessary() || ImplIsRecordLayout())
        return;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (!InitFont())
        return;

    Point aStartPt = ImplLogicToDevicePixel(rStartPos);
    Point aEndPt = ImplLogicToDevicePixel(rEndPos);

    tools::Long nStartX = aStartPt.X();
    tools::Long nStartY = aStartPt.Y();
    tools::Long nEndX = aEndPt.X();
    tools::Long nEndY = aEndPt.Y();
    double fOrientation = 0.0;

    // handle rotation
    if (nStartY != nEndY || nStartX > nEndX)
    {
        tools::Long nLengthX = nEndX - nStartX;
        fOrientation = std::atan2(nStartY - nEndY, (nLengthX == 0 ? 0.000000001 : nLengthX));
        fOrientation /= F_PI180;
        // un-rotate the end point
        aStartPt.RotateAround(nEndX, nEndY, Degree10(static_cast<sal_Int16>(-fOrientation * 10.0)));
    }

    tools::Long nWaveHeight = 3;

    // Handle HiDPI
    float fScaleFactor = GetDPIScaleFactor();
    if (fScaleFactor > 1.0f)
    {
        nWaveHeight *= fScaleFactor;

        nStartY
            += fScaleFactor - 1; // Shift down additional pixel(s) to create more visual separation.

        // odd heights look better than even
        if (nWaveHeight % 2 == 0)
        {
            nWaveHeight--;
        }
    }

    // #109280# make sure the waveline does not exceed the descent to avoid paint problems
    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    if (nWaveHeight > pFontInstance->mxFontMetric->GetWavelineUnderlineSize())
    {
        nWaveHeight = pFontInstance->mxFontMetric->GetWavelineUnderlineSize();
        // tdf#124848 hairline
        nLineWidth = 0;
    }

    const basegfx::B2DRectangle aWaveLineRectangle(nStartX, nStartY, nEndX, nEndY + nWaveHeight);
    const basegfx::B2DPolygon aWaveLinePolygon = basegfx::createWaveLinePolygon(aWaveLineRectangle);
    const basegfx::B2DHomMatrix aRotationMatrix = basegfx::utils::createRotateAroundPoint(
        nStartX, nStartY, basegfx::deg2rad(-fOrientation));
    const bool bPixelSnapHairline(mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    mpGraphics->SetLineColor(GetLineColor());
    mpGraphics->DrawPolyLine(aRotationMatrix, aWaveLinePolygon, 0.0, nLineWidth,
                             nullptr, // MM01
                             basegfx::B2DLineJoin::NONE, css::drawing::LineCap_BUTT,
                             basegfx::deg2rad(15.0), bPixelSnapHairline, *this);

    if (mpAlphaVDev)
        mpAlphaVDev->DrawWaveLine(rStartPos, rEndPos, nLineWidth);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
