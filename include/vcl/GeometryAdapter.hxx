/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>

#include <vcl/dllapi.h>
#include <vcl/region.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/TransformPlan.hxx>
#include <vcl/ResolvedTransform.hxx>

namespace vcl
{
struct TransformPlan;
}

namespace vcl::GeometryAdapter
{
VCL_DLLPUBLIC Point Apply(const vcl::TransformPlan& rPlan, const Point& rPt);
VCL_DLLPUBLIC Size Apply(const vcl::TransformPlan& rPlan, const Size& rSize);
VCL_DLLPUBLIC tools::Rectangle Apply(const vcl::TransformPlan& rPlan,
                                     const tools::Rectangle& rRect);

VCL_DLLPUBLIC tools::Polygon Apply(const vcl::TransformPlan& rPlan, const tools::Polygon& rPoly);
VCL_DLLPUBLIC tools::PolyPolygon Apply(const vcl::TransformPlan& rPlan,
                                       const tools::PolyPolygon& rPolyPoly);
VCL_DLLPUBLIC vcl::Region Apply(const vcl::TransformPlan& rPlan, const vcl::Region& rRegion);
VCL_DLLPUBLIC LineInfo Apply(const vcl::TransformPlan& rPlan, const LineInfo& rLine);

VCL_DLLPUBLIC basegfx::B2DPolygon Apply(const vcl::TransformPlan& rPlan,
                                        const basegfx::B2DPolygon& rPoly);
VCL_DLLPUBLIC basegfx::B2DPolyPolygon Apply(const vcl::TransformPlan& rPlan,
                                            const basegfx::B2DPolyPolygon& rPolyPoly);
VCL_DLLPUBLIC basegfx::B2DRange Apply(const vcl::TransformPlan& rPlan,
                                      const basegfx::B2DRange& rRange);
}

// ========================================================================
// Inline execution adapter (Must be defined AFTER Apply declarations)
// ========================================================================
namespace vcl
{
template <typename T> inline auto TransformPlan::apply(const T& rGeom) const
{
    return vcl::GeometryAdapter::Apply(*this, rGeom);
}
}

template <typename Geom>
auto ApplyTransform(const vcl::ResolvedTransform& rTransform,
                    const vcl::TypedGeom<vcl::SpaceLogic, Geom>& rLogicGeom)
{
    auto aResult = vcl::GeometryAdapter::Apply(rTransform.mrPlan, rLogicGeom.get());

    return vcl::TypedGeom<vcl::SpaceDevice, decltype(aResult)>(aResult);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
