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
    if (maRecorder.IsActive())
    {
        maRecorder.RecordB2DPolyLine(rB2D, fLineWidth, eLineJoin, eLineCap,
                                     rObjectTransform, fMiterMinimumAngle,
                                     fTransparency, pStroke);
    }

    if (!IsDeviceOutputNecessary())
        return true;

    return vcl::rendercontext::PrimitiveRenderer::DrawPolyLine(
        *this, rB2D, fLineWidth, eLineJoin, eLineCap,
        rObjectTransform, fMiterMinimumAngle, fTransparency, pStroke);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
