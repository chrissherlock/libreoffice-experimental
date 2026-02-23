/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <vcl/dllapi.h>
#include <o3tl/typed_flags_set.hxx>

namespace vcl::rendercontext
{
/**
 * Determines which parts of a complex, self-intersecting path are filled.
 * Correlates to SVG fill-rule and PDF fill operators.
 */
enum class FillRule
{
    NonZeroWinding = 0x00, // Default: SVG "nonzero", PDF "f"
    EvenOdd = 0x01 // Parity: SVG "evenodd", PDF "f*"
};

} // namespace vcl::rendercontext

namespace o3tl
{
template <>
struct typed_flags<vcl::rendercontext::FillRule>
    : is_typed_flags<vcl::rendercontext::FillRule, 0x01>
{
};
}

namespace vcl::rendercontext
{
/**
 * Encapsulates the visual properties of a shape's interior.
 * Designed for stateless rendering in PrimitiveRenderer.
 */
struct VCL_DLLPUBLIC FillAttributes
{
    Color maColor = COL_TRANSPARENT;
    double fTransparency = 0.0; // 0.0 (opaque) to 1.0 (fully transparent)
    FillRule eFillRule = FillRule::NonZeroWinding;

    // Default constructor (No Fill)
    FillAttributes() = default;

    explicit FillAttributes(const Color& rColor, double fTrans = 0.0,
                            FillRule eRule = FillRule::NonZeroWinding)
        : maColor(rColor)
        , fTransparency(fTrans)
        , eFillRule(eRule)
    {
    }

    bool isNone() const { return maColor == COL_TRANSPARENT || fTransparency >= 1.0; }
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
