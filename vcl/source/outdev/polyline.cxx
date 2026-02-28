/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/outdev.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <salgdi.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly)
{
    if (maRecorder.IsActive())
        maRecorder.RecordPolyLine(rPoly);

    if (IsDeviceOutputNecessary())
        {
        vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
        DrawPolyLine(rPoly, LineInfo());
    }
}

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    if (maRecorder.IsActive())
        maRecorder.RecordPolyLine(rPoly, rLineInfo);

    if (IsDeviceOutputNecessary())
        {
        if (rPoly.GetSize() < 2 || !CanDrawPolyline())
            return;

        FlushGraphicsState();

        if (RasterOp::OverPaint == GetRasterOp() && IsLineColor())
        {
            const bool bPixelSnapHairline
                = (mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
                  && rPoly.GetSize() < 1000;

            if (mpGraphics->DrawPolyLine(basegfx::B2DHomMatrix(), rPoly.getB2DPolygon(), 0.0,
                                         rLineInfo.GetWidth(), nullptr, rLineInfo.GetLineJoin(),
                                         rLineInfo.GetLineCap(), basegfx::deg2rad(15.0),
                                         bPixelSnapHairline, *this))
            {
                return;
            }
        }

        if (rLineInfo.GetStyle() == LineStyle::Dash || rLineInfo.GetWidth() > 1)
        {
            basegfx::B2DPolygon aPoly = mpMapper->LogicToDevicePixel(rPoly.getB2DPolygon());
            vcl::rendercontext::PrimitiveRenderer::DrawPolyLineGeometry(*this, basegfx::B2DPolyPolygon(aPoly), rLineInfo);
        }
        else
        {
            {
            vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
            DrawPolygon(rPoly);
        }
        }
    }
}

bool OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2D,
                                const vcl::rendercontext::StrokeAttributes& rStroke,
                                const basegfx::B2DHomMatrix& rObjectTransform)
{
    if (rB2D.count() == 0 || !CanDrawPolyline())
        return true;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    return vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(*this, rB2D, rStroke, rObjectTransform);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
