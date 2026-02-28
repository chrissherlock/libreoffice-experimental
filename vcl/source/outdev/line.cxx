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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>
#include <tools/debug.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>

#include <vcl/metafile/MetafileRecorder.hxx>
#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <numeric>
#include <utility>

const Color& OutputDevice::GetLineColor() const
{
    return mpGraphicsState->maLineColor;
}

bool OutputDevice::IsLineColor() const
{
    return mpGraphicsState->mbLineColor;
}

void OutputDevice::SetLineColor()
{
    maRecorder.RecordLineColor( Color(), false );

    if (mpGraphicsState->mbLineColor)
    {
        mbLineColorDirty = true;
        mpGraphicsState->mbLineColor = false;
        mpGraphicsState->maLineColor = COL_TRANSPARENT;
    }
}

void OutputDevice::SetLineColor(const Color& rColor)
{
    Color aColor = vcl::drawmode::GetLineColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    maRecorder.RecordLineColor( aColor, true );

    if (mpGraphicsState->maLineColor != aColor)
    {
        mbLineColorDirty = true;
        mpGraphicsState->mbLineColor = true;
        mpGraphicsState->maLineColor = aColor;
    }
}

void OutputDevice::InitLineColor()
{
    DBG_TESTSOLARMUTEX();

    if( mpGraphicsState->mbLineColor )
    {
        if( RasterOp::N0 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::N0 );
        else if( RasterOp::N1 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::N1 );
        else if( RasterOp::Invert == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPLineColor( SalROPColor::Invert );
        else
            mpGraphics->SetLineColor(mpGraphicsState->maLineColor);
    }
    else
    {
        mpGraphics->SetLineColor();
    }

    mbLineColorDirty = false;
}

void OutputDevice::DrawLine(const Point& rStartPt, const Point& rEndPt)
{
    assert(!is_double_buffered_window());

    maRecorder.RecordLine(rStartPt, rEndPt);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line) || !mpGraphics)
        return;

    const bool bTryAA = (RasterOp::OverPaint == GetRasterOp() && IsLineColor());
    const bool bPixelSnapHairline = bool(mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    Point aDeviceStart = mpMapper->LogicToDevicePixel(rStartPt);
    Point aDeviceEnd = mpMapper->LogicToDevicePixel(rEndPt);

    bool bRTL = IsRTLEnabled() || (mpGraphics && (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
    bool bAntiparallel = ImplIsAntiparallel();
    tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();

    mpMapper->MirrorDevicePixelPoint(aDeviceStart, nFrameWidth, bRTL, bAntiparallel);
    mpMapper->MirrorDevicePixelPoint(aDeviceEnd, nFrameWidth, bRTL, bAntiparallel);

    vcl::rendercontext::PrimitiveRenderer::DrawDeviceLine(*mpGraphics, aDeviceStart, aDeviceEnd,
                                                          bTryAA, bPixelSnapHairline);
}

void OutputDevice::DrawLine(const Point& rStartPt, const Point& rEndPt, const LineInfo& rLineInfo)
{
    assert(!is_double_buffered_window());

    // Fallback for default lines
    if (rLineInfo.IsDefault())
    {
        DrawLine( rStartPt, rEndPt );
        return;
    }

    maRecorder.RecordLine(rStartPt, rEndPt, rLineInfo);

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line) || !mpGraphics || rLineInfo.GetStyle() == LineStyle::NONE)
        return;

    const LineInfo aInfo(mpMapper->LogicToDevicePixel(rLineInfo));

    if (aInfo.GetStyle() != LineStyle::Dash && aInfo.GetWidth() <= 1)
    {
        // Simple solid hairline
        Point aDeviceStart = mpMapper->LogicToDevicePixel(rStartPt);
        Point aDeviceEnd = mpMapper->LogicToDevicePixel(rEndPt);

        bool bRTL = IsRTLEnabled() || (mpGraphics && (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
        bool bAntiparallel = ImplIsAntiparallel();
        tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : mpGraphics->GetGraphicsWidth();

        mpMapper->MirrorDevicePixelPoint(aDeviceStart, nFrameWidth, bRTL, bAntiparallel);
        mpMapper->MirrorDevicePixelPoint(aDeviceEnd, nFrameWidth, bRTL, bAntiparallel);

        vcl::rendercontext::PrimitiveRenderer::DrawDeviceLine(*mpGraphics, aDeviceStart, aDeviceEnd);
        return;
    }

    DrawLineGeometry(rStartPt, rEndPt, rLineInfo);
}

void OutputDevice::DrawLineGeometry(const Point& rStartPt, const Point& rEndPt,
                                    const LineInfo& rLineInfo)
{
    const Point aStartPt(LogicToDevicePixel(rStartPt));
    const Point aEndPt(LogicToDevicePixel(rEndPt));

    basegfx::B2DPolygon aLine;
    aLine.append(basegfx::B2DPoint(aStartPt.X(), aStartPt.Y()));
    aLine.append(basegfx::B2DPoint(aEndPt.X(), aEndPt.Y()));

    vcl::rendercontext::PrimitiveRenderer::DrawPolyLineGeometry(*GetGraphics(), *mpMapper, basegfx::B2DPolyPolygon(aLine), rLineInfo);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
