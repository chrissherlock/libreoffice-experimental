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
            vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
            DrawPolygon(rPoly);
        }
    }
}

bool OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2D, const vcl::rendercontext::StrokeAttributes& rStroke, const basegfx::B2DHomMatrix& rObjectTransform)
{
    if (rB2D.count() == 0 || !CanDrawPolyline())
        return true;

    if (maRecorder.IsActive())
    {
        basegfx::B2DPolygon aRecordPoly(rB2D);
        if (!rObjectTransform.isIdentity())
            aRecordPoly.transform(rObjectTransform);
        maRecorder.RecordB2DPolyLine(aRecordPoly, rStroke);
    }

    if (!IsDeviceOutputNecessary())
        return true;

    if (!GetGraphics() && !AcquireGraphics())
        return false;

    FlushGraphicsState();

    const basegfx::B2DHomMatrix aTransform(GetViewTransformation() * rObjectTransform);
    bool bSuccess = false;

    if (GetRasterOp() == RasterOp::OverPaint && IsLineColor())
    {
        const bool bPixelSnapHairline
            = (mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline)
              && rB2D.count() < 1000;

        SalGraphics* pGraphics = GetGraphics();
        if (pGraphics
            && pGraphics->DrawPolyLine(aTransform, rB2D, rStroke.fTransparency,
                                       rStroke.fWidth, rStroke.pDashArray, rStroke.eJoin,
                                       rStroke.eCap, rStroke.fMiterMinimumAngle, bPixelSnapHairline,
                                       *this))
        {
            bSuccess = true;
        }
    }

    if (!bSuccess)
    {
        basegfx::B2DPolygon aDevicePoly(rB2D);
        aDevicePoly.transform(aTransform);

        basegfx::B2DPolyPolygon aPolyPolygon(aDevicePoly);
        if (rStroke.pDashArray && !rStroke.pDashArray->empty())
        {
            basegfx::B2DPolyPolygon aDashedPolyPoly;
            basegfx::utils::applyLineDashing(basegfx::B2DPolyPolygon(aDevicePoly), *rStroke.pDashArray,
                                             &aDashedPolyPoly);
            aPolyPolygon = aDashedPolyPoly;
        }

        LineInfo aInfo;
        aInfo.SetWidth(std::round(rStroke.fWidth));
        aInfo.SetLineJoin(rStroke.eJoin);
        aInfo.SetLineCap(rStroke.eCap);

        vcl::rendercontext::PrimitiveRenderer::DrawPolyLineGeometry(*this, aPolyPolygon, aInfo);
        bSuccess = true;
    }

    return bSuccess;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
