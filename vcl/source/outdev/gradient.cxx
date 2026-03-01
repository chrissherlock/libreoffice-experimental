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

#include <tools/poly.hxx>

#include <vcl/gradient.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <com/sun/star/awt/GradientStyle.hpp>

#include <cassert>
#include <memory>

void OutputDevice::DrawGradient( const tools::Rectangle& rRect,
                                 const Gradient& rGradient )
{
    assert(!is_double_buffered_window());

    // Convert rectangle to a tools::PolyPolygon by first converting to a Polygon
    tools::Polygon aPolygon ( rRect );
    tools::PolyPolygon aPolyPoly ( aPolygon );

    DrawGradient ( aPolyPoly, rGradient );
}

void OutputDevice::DrawGradient(const tools::PolyPolygon& rPolyPoly,
                                const Gradient& rGradient)
{
    assert(!is_double_buffered_window());

    if (GetDrawMode() & DrawModeFlags::GrayGradient)
    {
        Gradient aGrayGrad(rGradient);
        aGrayGrad.MakeGrayscale();
        maRecorder.RecordGradient(rPolyPoly, aGrayGrad);
    }
    else
    {
        maRecorder.RecordGradient(rPolyPoly, rGradient);
    }

    if (!IsDeviceOutputNecessary() || IsLayoutCalculationNecessary())
        return;

    if (!rPolyPoly.Count() || !rPolyPoly[0].GetSize())
        return;

    if (mpGraphicsState->mnDrawMode & (DrawModeFlags::WhiteGradient | DrawModeFlags::SettingsGradient))
    {
        Color aColor = GetSingleColorGradientFill();
        auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR);
        SetLineColor(aColor);
        SetFillColor(aColor);
        DrawPolyPolygon(rPolyPoly);
        return;
    }

    Gradient aEffectiveGradient(rGradient);
    if (mpGraphicsState->mnDrawMode & DrawModeFlags::GrayGradient)
        aEffectiveGradient.MakeGrayscale();

    const tools::Rectangle aBoundRect(rPolyPoly.GetBoundRect());
    tools::Rectangle aDeviceRect(LogicToDevicePixel(aBoundRect));
    aDeviceRect.Normalize();

    if (aDeviceRect.IsEmpty())
        return;

    tools::PolyPolygon aDevicePolyPoly(mpMapper->LogicToDevicePixel(rPolyPoly));

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsRTLEnabled() || (mpGraphics && (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl)))
    {
        bool bAntiparallel = ImplIsAntiparallel();
        tools::Long nFrameWidth = IsVirtual() ? GetOutputWidthPixel() : (mpGraphics ? mpGraphics->GetGraphicsWidth() : 0);

        mpMapper->MirrorDevicePixelPolyPolygon(aDevicePolyPoly, nFrameWidth, true, bAntiparallel);
        aDeviceRect = aDevicePolyPoly.GetBoundRect();
    }

    auto popIt = ScopedPush(vcl::PushFlags::CLIPREGION);
    IntersectClipRegion(aBoundRect);

    if (mpClippingController->IsDirty())
        InitClipRegion();

    if (IsOutputCulled())
        return;

    if (vcl::rendercontext::PrimitiveRenderer::DrawGradient(*mpGraphics, aDevicePolyPoly, aEffectiveGradient))
        return;

    if (mpGraphicsState->mbLineColor || mbLineColorDirty)
    {
        mpGraphics->SetLineColor();
        mbLineColorDirty = true;
    }
    mbFillColorDirty = true;

    // RESTORE HACK: Expand the rectangle to prevent antialiasing gaps
    // and to align the integer math for the unit tests!
    if (rPolyPoly.IsRect())
    {
        aDeviceRect.AdjustLeft(-1);
        aDeviceRect.AdjustTop(-1);
        aDeviceRect.AdjustRight(1);
        aDeviceRect.AdjustBottom(1);
    }

    tools::Rectangle aGradientBoundRect;
    Point aCenter;
    aEffectiveGradient.GetBoundRect(aDeviceRect, aGradientBoundRect, aCenter);

    tools::Long nStepCount = GetGradientSteps(aEffectiveGradient, aGradientBoundRect);

    vcl::rendercontext::PrimitiveRenderer::DrawGradient(
        *mpGraphics,
        aDeviceRect,
        aEffectiveGradient,
        nStepCount,
        (meOutDevType == OUTDEV_PRINTER), /* Avoid vector overdraw */
        aDevicePolyPoly.IsRect() ? nullptr : &aDevicePolyPoly);
}

bool OutputDevice::is_double_buffered_window() const
{
    auto pOwnerWindow = GetOwnerWindow();
    return pOwnerWindow && pOwnerWindow->SupportsDoubleBuffering();
}

tools::Long OutputDevice::GetGradientStepCount( tools::Long nMinRect )
{
    tools::Long nInc = (nMinRect < 50) ? 2 : 4;

    return nInc;
}

tools::Long OutputDevice::GetGradientSteps(Gradient const& rGradient, tools::Rectangle const& rRect)
{
    // calculate step count
    tools::Long nStepCount = rGradient.GetSteps();

    if (nStepCount)
        return nStepCount;

    tools::Long nMinRect = 0;

    if (rGradient.GetStyle() == css::awt::GradientStyle_LINEAR || rGradient.GetStyle() == css::awt::GradientStyle_AXIAL)
        nMinRect = rRect.GetHeight();
    else
        nMinRect = std::min(rRect.GetWidth(), rRect.GetHeight());

    tools::Long nInc = GetGradientStepCount(nMinRect);

    if (!nInc)
        nInc = 1;

    return nMinRect / nInc;
}

Color OutputDevice::GetSingleColorGradientFill()
{
    Color aColor;

    // we should never call on this function if any of these aren't set!
    assert( mpGraphicsState->mnDrawMode & ( DrawModeFlags::WhiteGradient | DrawModeFlags::SettingsGradient) );

    if ( mpGraphicsState->mnDrawMode & DrawModeFlags::WhiteGradient )
        aColor = COL_WHITE;
    else if ( mpGraphicsState->mnDrawMode & DrawModeFlags::SettingsGradient )
    {
        if (mpGraphicsState->mnDrawMode & DrawModeFlags::SettingsForSelection)
            aColor = GetSettings().GetStyleSettings().GetHighlightColor();
        else
            aColor = GetSettings().GetStyleSettings().GetWindowColor();
    }

    return aColor;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
