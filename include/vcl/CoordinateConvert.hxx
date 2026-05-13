/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/outdev.hxx>
#include <vcl/CoordinateMapper.hxx>
#include <vcl/TransformTypes.hxx>
#include <vcl/GeometryAdapter.hxx>

namespace vcl
{
// Standard Conversion (uses OutputDevice's current MapMode)
template <typename OutType, typename InType>
inline OutType Convert(const OutputDevice& rOutDev, const InType& rInput,
                       MappingPolicy ePolicy = MappingPolicy::ApplyMapMode)
{
    constexpr CoordinateSpace eFrom = InType::Space;
    constexpr CoordinateSpace eTo = OutType::Space;

    // Unwrap the input with .get(), apply the math, and explicitly wrap the output!
    return OutType(
        GeometryAdapter::Apply(rOutDev.GetMapper().Compile({ eFrom, eTo, ePolicy }), rInput.get()));
}

// Override Conversion (for UI dialogs passing MapMode(MapAppFont), etc.)
template <typename OutType, typename InType>
inline OutType Convert(const OutputDevice& rOutDev, const InType& rInput,
                       const MapMode& rTargetMapMode)
{
    // For custom MapModes, we compile a temporary view transformation
    // NOTE: Assumes Logic -> Device/Window routing for these overrides.
    basegfx::B2DHomMatrix aMat = rOutDev.GetMapper().GetViewTransformation(
        rOutDev.GetMapMode(), rTargetMapMode, rOutDev.GetMappingPolicy());

    // Unwrap the input with .get(), apply the math, and explicitly wrap the output!
    return OutType(GeometryAdapter::Apply(vcl::TransformCompiler::Compile(aMat), rInput.get()));
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
