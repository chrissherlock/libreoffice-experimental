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
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <devicedispatcher.hxx>
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

void OutputDevice::DrawGradient(const tools::PolyPolygon& rPolyPoly, const Gradient& rGradient)
{
    assert(!is_double_buffered_window());

    Gradient aEffectiveGradient(rGradient);
    if (GetDrawMode() & DrawModeFlags::GrayGradient)
        aEffectiveGradient.MakeGrayscale();

    maRecorder.RecordGradient(rPolyPoly, aEffectiveGradient);

    if (!IsDeviceOutputNecessary() || IsLayoutCalculationNecessary())
        return;

    if (!rPolyPoly.Count() || !rPolyPoly[0].GetSize())
        return;

    if (mpGraphicsState->mnDrawMode & (DrawModeFlags::WhiteGradient | DrawModeFlags::SettingsGradient))
    {
        Color aSolidColor = mpGraphicsState->GetSingleColorGradientFill(GetSettings().GetStyleSettings());
        auto oGroup = maRecorder.CreateScopedGroup("SolidGradientFallback");
        auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR);

        SetLineColor(aSolidColor);
        SetFillColor(aSolidColor);
        DrawPolyPolygon(rPolyPoly);
        return;
    }

    vcl::DispatchDevice(*this, [&](const auto& rConcrete) {
        // Gradients manage their own line colors internally, only Fill prep is strictly needed here
        if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Fill) || !mpGraphics)
            return;

        tools::PolyPolygon aDevPolyPoly(mpMapper->LogicToDevicePixel(rPolyPoly));
        tools::Rectangle aDevRect = aDevPolyPoly.GetBoundRect();

        if (aDevRect.IsEmpty())
            return;

        const bool bRTL = IsRTLEnabled() || (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl);
        if (bRTL)
        {
            tools::Long nWidth = vcl::get_reference_width_v(rConcrete);
            mpMapper->MirrorDevicePixelPolyPolygon(aDevPolyPoly, nWidth, true, ImplIsAntiparallel());
            aDevRect = aDevPolyPoly.GetBoundRect(); // Update bounds after mirroring
        }

        tools::Long nStepCount = aEffectiveGradient.GetCalculatedSteps(aDevRect, GetDPIY());

        using ConcreteType = std::decay_t<decltype(rConcrete)>;
        const bool bAvoidOverdraw = vcl::avoids_vector_overdraw_v<ConcreteType>;

        vcl::rendercontext::PrimitiveRenderer::DrawGradient(
            *mpGraphics, aDevPolyPoly, aEffectiveGradient, nStepCount, bAvoidOverdraw);
    });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
