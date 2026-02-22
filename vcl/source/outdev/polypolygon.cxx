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
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/poly.hxx>

#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <memory>

void OutputDevice::DrawPolyPolygon(const tools::PolyPolygon& rPolyPoly)
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(rPolyPoly);

    const sal_uInt16 nPoly = rPolyPoly.Count();
    if (!nPoly || !IsDeviceOutputNecessary())
        return;

    bool bFill = IsFillColor();
    vcl::rendercontext::StrokeAttributes aStroke;
    vcl::rendercontext::StrokeAttributes* pStroke = nullptr;

    if (IsLineColor())
    {
        aStroke.fTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
        pStroke = &aStroke;
    }

    if (!vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(*this, rPolyPoly, bFill, pStroke))
        vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygonFallback(*this, rPolyPoly);
}

void OutputDevice::DrawPolyPolygon(const basegfx::B2DPolyPolygon& rB2DPolyPoly)
{
    assert(!is_double_buffered_window());

    if (maRecorder.IsActive())
        maRecorder.RecordPolyPolygon(tools::PolyPolygon(rB2DPolyPoly));

    if (!rB2DPolyPoly.count() || !IsDeviceOutputNecessary())
        return;

    bool bFill = IsFillColor();
    vcl::rendercontext::StrokeAttributes aStroke;
    vcl::rendercontext::StrokeAttributes* pStroke = nullptr;

    if (IsLineColor())
    {
        aStroke.fTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
        pStroke = &aStroke;
    }

    vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(*this, rB2DPolyPoly, bFill, pStroke);
}

namespace
{
struct ClippedPolygonData
{
    std::unique_ptr<tools::PolyPolygon> pAllocated;
    tools::PolyPolygon* pActive = nullptr;
};
}

static ClippedPolygonData lcl_GetClippedPolyPolygon(const tools::PolyPolygon& rPolyPoly,
                                                    const tools::PolyPolygon* pClipPolyPoly)
{
    ClippedPolygonData aData;

    if (pClipPolyPoly)
    {
        aData.pAllocated = std::make_unique<tools::PolyPolygon>();
        aData.pActive = aData.pAllocated.get();
        rPolyPoly.GetIntersection(*pClipPolyPoly, *aData.pActive);
    }
    else
    {
        aData.pActive = const_cast<tools::PolyPolygon*>(&rPolyPoly);
    }

    return aData;
}

void OutputDevice::ImplDrawPolyPolygon(const tools::PolyPolygon& rPolyPoly,
                                       const tools::PolyPolygon* pClipPolyPoly)
{
    auto aClippedData = lcl_GetClippedPolyPolygon(rPolyPoly, pClipPolyPoly);
    tools::PolyPolygon* pPolyPoly = aClippedData.pActive;

    if (pPolyPoly->Count() == 1)
        ImplDrawSinglePolygon(pPolyPoly->GetObject(0));
    else if (pPolyPoly->Count())
        ImplDrawMultiplePolygons(*pPolyPoly);
}

void OutputDevice::ImplDrawSinglePolygon(const tools::Polygon& rPoly)
{
    sal_uInt16 nSize = rPoly.GetSize();

    if (nSize >= 2)
    {
        const Point* pPtAry = rPoly.GetConstPointAry();
        mpGraphics->DrawPolygon(nSize, pPtAry, *this);
    }
}

namespace
{
struct PolygonRenderBuffer
{
    std::unique_ptr<sal_uInt32[]> pPointAry;
    std::unique_ptr<const Point* []> pPointAryAry;
    sal_uInt16 nValidCount = 0;

    explicit PolygonRenderBuffer(const tools::PolyPolygon& rPolyPoly)
    {
        sal_uInt16 nTotalCount = rPolyPoly.Count();

        // Allocate the arrays based on the total possible size
        pPointAry.reset(new sal_uInt32[nTotalCount]);
        pPointAryAry.reset(new const Point*[nTotalCount]);

        // Unpack and filter the polygons
        for (sal_uInt16 i = 0; i < nTotalCount; ++i)
        {
            const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();

            if (nSize >= 2)
            {
                pPointAry[nValidCount] = nSize;
                pPointAryAry[nValidCount] = rPoly.GetConstPointAry();
                nValidCount++;
            }
        }
    }
};
}

void OutputDevice::ImplDrawMultiplePolygons(const tools::PolyPolygon& rPolyPoly)
{
    if (!rPolyPoly.Count())
        return;

    PolygonRenderBuffer aBuffer(rPolyPoly);

    if (aBuffer.nValidCount == 1)
    {
        mpGraphics->DrawPolygon(aBuffer.pPointAry[0], aBuffer.pPointAryAry[0], *this);
    }
    else if (aBuffer.nValidCount > 1)
    {
        mpGraphics->DrawPolyPolygon(aBuffer.nValidCount, aBuffer.pPointAry.get(),
                                    aBuffer.pPointAryAry.get(), *this);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
