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
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>

#include <salgdi.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    if (rPoly.GetSize() < 2 || !CanDrawPolyline())
        return;

    if (maRecorder.IsActive())
    {
        if (rLineInfo == LineInfo())
            maRecorder.RecordPolyLine(rPoly);
        else
            maRecorder.RecordPolyLine(rPoly, rLineInfo);
    }

    if (!FlushGraphicsState(vcl::PrepareOutputFlags::Line))
        return;

    vcl::rendercontext::StrokeAttributes aStroke{ rLineInfo.GetWidth(),
                                                  rLineInfo.GetLineJoin(),
                                                  rLineInfo.GetLineCap(),
                                                  basegfx::deg2rad(15.0),
                                                  nullptr,
                                                  0.0 };

    if (vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(
            *GetGraphics(), *mpMapper, rPoly.getB2DPolygon(), aStroke, basegfx::B2DHomMatrix(),
            GetAntialiasing(), GetRasterOp()))
    {
        return;
    }

    if (rLineInfo.GetStyle() == LineStyle::Dash || rLineInfo.GetWidth() > 1)
    {
        basegfx::B2DPolygon aPoly = mpMapper->LogicToDevicePixel(rPoly.getB2DPolygon());
        vcl::rendercontext::PrimitiveRenderer::DrawPolyLineGeometry(
            *GetGraphics(), *mpMapper, basegfx::B2DPolyPolygon(aPoly), rLineInfo);
    }
    else
    {
        vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
        DrawPolygon(rPoly);
    }
}

bool OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2D,
                                const vcl::rendercontext::StrokeAttributes& rStroke,
                                const basegfx::B2DHomMatrix& rObjectTransform)
{
    if (rB2D.count() == 0 || !CanDrawPolyline())
        return true;

    if (maRecorder.IsActive())
        maRecorder.RecordB2DPolyLine(rB2D, rStroke, rObjectTransform);

    if (!FlushGraphicsState(vcl::PrepareOutputFlags::Line))
        return true;

    return vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(*GetGraphics(), *mpMapper, rB2D,
                                                               rStroke, rObjectTransform,
                                                               GetAntialiasing(), GetRasterOp());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
