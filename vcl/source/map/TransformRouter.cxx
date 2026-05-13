/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/TransformRouter.hxx>

#include <TransformCompiler.hxx>

#include "CoordinateMath.hxx"

#include <cstdlib>

namespace vcl
{
const TransformPlan& TransformRouter::Compile(const CoordinateState& rState,
                                              const TransformRequest& rReq) const
{
    TransformKey eKey = ResolveKey(rReq);

    if (const TransformPlan* pCached = maCache.Get(eKey))
        return *pCached;

    basegfx::B2DHomMatrix aMat = BuildMatrix(rState, rReq);

    maCache.Store(eKey, vcl::TransformCompiler::Compile(aMat));

    return *maCache.Get(eKey);
}

TransformKey TransformRouter::ResolveKey(const TransformRequest& rReq) const
{
    const bool bMapped = (rReq.Policy == vcl::MappingPolicy::ApplyMapMode);

    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Window)
        return bMapped ? TransformKey::LogicToWindow_Mapped : TransformKey::LogicToWindow_Unmapped;
    if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::WindowToLogic_Mapped : TransformKey::WindowToLogic_Unmapped;
    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Device)
        return bMapped ? TransformKey::LogicToDevice_Mapped : TransformKey::LogicToDevice_Unmapped;
    if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::DeviceToLogic_Mapped : TransformKey::DeviceToLogic_Unmapped;
    if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Window)
        return TransformKey::DeviceToWindow;
    if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Device)
        return TransformKey::WindowToDevice;

    std::abort(); // Failsafe for invalid un-indexed routing
}

basegfx::B2DHomMatrix TransformRouter::BuildMatrix(const CoordinateState& rState,
                                                   const TransformRequest& rReq) const
{
    basegfx::B2DHomMatrix aMat;

    // Composition Phase 1: Logic-to-Window (The Zoom/MapMode layer)
    if (rReq.Policy == vcl::MappingPolicy::ApplyMapMode)
    {
        const double fScaleX = rState.GetMapRes().mfScaleX * static_cast<double>(rState.GetDPIX())
                               * rState.GetDPIScaleFactor();
        const double fScaleY = rState.GetMapRes().mfScaleY * static_cast<double>(rState.GetDPIY())
                               * rState.GetDPIScaleFactor();

        aMat = vcl::BuildAffineMatrix(fScaleX, fScaleY,
                                      static_cast<double>(rState.GetMapRes().mnTranslationX
                                                          + rState.GetLogicToAbsoluteOffsetX()),
                                      static_cast<double>(rState.GetMapRes().mnTranslationY
                                                          + rState.GetLogicToAbsoluteOffsetY()),
                                      static_cast<double>(rState.GetWindowToViewOffsetX()),
                                      static_cast<double>(rState.GetWindowToViewOffsetY()));
    }
    else
    {
        // Unmapped math is just the window viewport translation
        aMat.translate(static_cast<double>(rState.GetWindowToViewOffsetX()),
                       static_cast<double>(rState.GetWindowToViewOffsetY()));
    }

    // Composition Phase 2: Extend to Device Space if either end of the request is 'Device'
    if (rReq.eTo == CoordinateSpace::Device || rReq.eFrom == CoordinateSpace::Device)
    {
        aMat.translate(static_cast<double>(rState.GetDeviceToWindowOffsetX()),
                       static_cast<double>(rState.GetDeviceToWindowOffsetY()));
    }

    // Composition Phase 3: Directionality
    // If we are coming FROM Window/Device, we need the Inverse of the Logic->Physical stack
    if (rReq.eFrom == CoordinateSpace::Window || rReq.eFrom == CoordinateSpace::Device)
    {
        if (aMat.isInvertible())
            aMat.invert();
        else
            return basegfx::B2DHomMatrix(); // Singular fallback
    }

    return aMat;
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
