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
    double fLineTransparency = 0.0;

    if (IsLineColor())
    {
        aStroke.fWidth = 0.0; // Hairline fallback
        aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        aStroke.eCap = css::drawing::LineCap_BUTT;
        aStroke.fMiterMinimumAngle = basegfx::deg2rad(15.0);
        fLineTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
        pStroke = &aStroke;
    }

    if (!vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(*this, rPolyPoly, bFill, pStroke,
                                                                fLineTransparency))
    {
        ImplDrawPolyPolygonFallback(nPoly, rPolyPoly);
    }
}

void OutputDevice::ImplDrawPolyPolygonFallback(sal_uInt16 nPoly,
                                               const tools::PolyPolygon& rPolyPoly)
{
    if (nPoly == 1)
    {
        // #100127# Map to DrawPolygon
        const tools::Polygon& aPoly = rPolyPoly.GetObject(0);
        if (aPoly.GetSize() >= 2)
        {
            vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
            DrawPolygon(aPoly);
        }
    }
    else if (nPoly > 1)
    {
        // #100127# moved real tools::PolyPolygon draw to separate method,
        // have to call recursively, avoiding duplicate
        // ImplLogicToDevicePixel calls
        ImplDrawPolyPolygon(nPoly, mpMapper->LogicToDevicePixel(rPolyPoly));
    }
}

// Caution: This method is nearly the same as
// OutputDevice::DrawTransparent( const basegfx::B2DPolyPolygon& rB2DPolyPoly, double fTransparency),
// so when changes are made here do not forget to make changes there, too

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
    double fLineTransparency = 0.0;

    if (IsLineColor())
    {
        aStroke.fWidth = 0.0; // Hairline fallback
        aStroke.eJoin = basegfx::B2DLineJoin::NONE;
        aStroke.eCap = css::drawing::LineCap_BUTT;
        aStroke.fMiterMinimumAngle = basegfx::deg2rad(15.0);
        fLineTransparency = (255.0 - GetLineColor().GetAlpha()) / 255.0;
        pStroke = &aStroke;
    }

    if (!vcl::rendercontext::PrimitiveRenderer::DrawPolyPolygon(*this, rB2DPolyPoly, bFill, pStroke,
                                                                fLineTransparency))
    {
        // Fallback to legacy tools::PolyPolygon rasterizer
        const tools::PolyPolygon aToolsPolyPolygon(rB2DPolyPoly);
        const tools::PolyPolygon aPixelPolyPolygon
            = mpMapper->LogicToDevicePixel(aToolsPolyPolygon);
        ImplDrawPolyPolygon(aPixelPolyPolygon.Count(), aPixelPolyPolygon);
    }
}

namespace
{
constexpr sal_uInt16 OUTDEV_POLYPOLY_STACKBUF = 32;

struct PolyPolyBuffer
{
    // The fast stack buffers
    sal_uInt32 aStackAry1[OUTDEV_POLYPOLY_STACKBUF];
    const Point* aStackAry2[OUTDEV_POLYPOLY_STACKBUF];
    const PolyFlags* aStackAry3[OUTDEV_POLYPOLY_STACKBUF];

    // The active pointers (will point to stack OR heap)
    sal_uInt32* pPointAry;
    const Point** pPointAryAry;
    const PolyFlags** pFlagAryAry;

    bool bUseHeap;

    // Extracted state variables
    sal_uInt16 mnValidCount = 0;
    sal_uInt16 mnLastIndex = 0;
    bool mbHaveBezier = false;

    explicit PolyPolyBuffer(const tools::PolyPolygon& rPolyPoly)
        : bUseHeap(rPolyPoly.Count() > OUTDEV_POLYPOLY_STACKBUF)
    {
        sal_uInt16 nPoly = rPolyPoly.Count();

        if (bUseHeap)
        {
            pPointAry = new sal_uInt32[nPoly];
            pPointAryAry = new const Point*[nPoly];
            pFlagAryAry = new const PolyFlags*[nPoly];
        }
        else
        {
            pPointAry = aStackAry1;
            pPointAryAry = aStackAry2;
            pFlagAryAry = aStackAry3;
        }

        // Flattens valid sub-polygons into parallel C-arrays for the graphics
        // backend and detects Bézier curves.

        for (sal_uInt16 i = 0; i < nPoly; ++i)
        {
            const tools::Polygon& rPoly = rPolyPoly.GetObject(i);
            sal_uInt16 nSize = rPoly.GetSize();

            if (nSize)
            {
                pPointAry[mnValidCount] = nSize;
                pPointAryAry[mnValidCount] = rPoly.GetConstPointAry();
                pFlagAryAry[mnValidCount] = rPoly.GetConstFlagAry();
                mnLastIndex = i;

                if (pFlagAryAry[mnValidCount])
                    mbHaveBezier = true;

                ++mnValidCount;
            }
        }
    }

    ~PolyPolyBuffer()
    {
        if (bUseHeap)
        {
            delete[] pPointAry;
            delete[] pPointAryAry;
            delete[] pFlagAryAry;
        }
    }
};

} // end anonymous namespace

void OutputDevice::ImplDrawPolyPolygon(sal_uInt16 nPoly, const tools::PolyPolygon& rPolyPoly)
{
    if (!nPoly)
        return;

    PolyPolyBuffer aBuffer(rPolyPoly);

    if (aBuffer.mnValidCount == 0)
        return;

    if (aBuffer.mnValidCount == 1)
    {
        // Only one valid polygon, fallback to singular draw
        const tools::Polygon& rPoly = rPolyPoly.GetObject(aBuffer.mnLastIndex);
        ImplDrawPolygon(rPoly);

        return;
    }

    // Draw the full PolyPolygon
    if (aBuffer.mbHaveBezier)
    {
        if (!mpGraphics->DrawPolyPolygonBezier(aBuffer.mnValidCount, aBuffer.pPointAry,
                                               aBuffer.pPointAryAry, aBuffer.pFlagAryAry, *this))
        {
            // Backend doesn't support beziers natively, subdivide and retry
            tools::PolyPolygon aPolyPoly = tools::PolyPolygon::SubdivideBezier(rPolyPoly);
            ImplDrawPolyPolygon(aPolyPoly.Count(), aPolyPoly);
        }

        return;
    }

    mpGraphics->DrawPolyPolygon(aBuffer.mnValidCount, aBuffer.pPointAry, aBuffer.pPointAryAry,
                                *this);
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
