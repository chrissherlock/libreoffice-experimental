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

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly)
{
    if (maRecorder.IsActive())
        maRecorder.RecordPolyLine(rPoly);

    if (IsDeviceOutputNecessary())
        vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(*this, rPoly);
}

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    if (maRecorder.IsActive())
        maRecorder.RecordPolyLine(rPoly, rLineInfo);

    if (IsDeviceOutputNecessary())
        vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(*this, rPoly, rLineInfo);
}

bool OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2D, double fLineWidth,
                               basegfx::B2DLineJoin eLineJoin, css::drawing::LineCap eLineCap,
                               const basegfx::B2DHomMatrix& rObjectTransform,
                               double fMiterMinimumAngle, double fTransparency,
                               const std::vector<double>* pStroke)
{
    // Semantically pack the geometry-generation attributes
    vcl::rendercontext::StrokeAttributes aStroke{fLineWidth, eLineJoin, eLineCap, fMiterMinimumAngle, pStroke};

    if (maRecorder.IsActive())
    {
        basegfx::B2DPolygon aRecordPoly(rB2D);
        if (!rObjectTransform.isIdentity())
            aRecordPoly.transform(rObjectTransform);
        maRecorder.RecordB2DPolyLine(aRecordPoly, aStroke, fTransparency);
    }

    if (!IsDeviceOutputNecessary())
        return true;

    return vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(*this, rB2D, aStroke, rObjectTransform, fTransparency);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */

bool OutputDevice::DrawPolyLine(const basegfx::B2DPolygon& rB2D,
                                const vcl::rendercontext::StrokeAttributes& rStroke,
                                const basegfx::B2DHomMatrix& rObjectTransform)
{
    // Phase 1: Safely unpack the struct and route to the legacy 8-arg implementation
    return DrawPolyLine(rB2D, rStroke.fWidth, rStroke.eJoin, rStroke.eCap,
                        rObjectTransform, rStroke.fMiterMinimumAngle,
                        rStroke.fTransparency, rStroke.pDashArray);
}

