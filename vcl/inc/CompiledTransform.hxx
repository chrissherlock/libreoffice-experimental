/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/types.h>
#include <tools/long.hxx>
#include <o3tl/hash_combine.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>

#include <o3tl/hash_combine.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>

#include <vcl/dllapi.h>
#include <vcl/lineinfo.hxx>
#include <vcl/mapconvert.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>
#include <vcl/MappingPolicy.hxx>

#include <MappingCoefficients.hxx>
#include <TransformTypes.hxx>

#include <optional>
#include <atomic>
#include <memory>
#include <concepts>
#include <type_traits>
#include <array>

class CoordinateMapper;

struct VCL_DLLPUBLIC CompiledTransform
{
public:
    basegfx::B2DHomMatrix maMatrix;
    uint64_t mnSemanticKey = 0;

    TransformMode meMode = TransformMode::AffineFallback;

    tools::Long mnLogicTx = 0, mnLogicTy = 0;
    tools::Long mnDeviceTx = 0, mnDeviceTy = 0;

    TransformContract maContract;

public:
    CompiledTransform() = default;

    const TransformContract& GetContract() const { return maContract; }
    bool PreservesAxisAlignment() const
    {
        return maContract.preserves(GeometryInvariant::AxisAlignment);
    }
    bool CheckRectilinearContract() const;

    uint64_t GetSemanticKey() const { return mnSemanticKey; }
    TransformMode GetMode() const { return meMode; }

    tools::Long GetLogicTx() const { return mnLogicTx; }
    tools::Long GetLogicTy() const { return mnLogicTy; }
    tools::Long GetDeviceTx() const { return mnDeviceTx; }
    tools::Long GetDeviceTy() const { return mnDeviceTy; }

    const basegfx::B2DHomMatrix& GetMatrix() const { return maMatrix; }

    template <typename T> T Apply(const T& rGeometry) const;

    bool IsIdentity() const { return meMode == TransformMode::Identity; }
    bool IsPureTranslation() const { return meMode == TransformMode::Translation; }

    Size ApplyRectilinear(const Size& rSize) const;
    tools::Rectangle ApplyRectilinear(const tools::Rectangle& rRect) const;
};

// Declare explicit specializations to prevent implicit instantiation errors
template <> VCL_DLLPUBLIC Point CompiledTransform::Apply<Point>(const Point& rPt) const;
template <> VCL_DLLPUBLIC Size CompiledTransform::Apply<Size>(const Size& rSize) const;
template <>
VCL_DLLPUBLIC tools::Rectangle
CompiledTransform::Apply<tools::Rectangle>(const tools::Rectangle& rRect) const;
template <>
VCL_DLLPUBLIC tools::Polygon
CompiledTransform::Apply<tools::Polygon>(const tools::Polygon& rPoly) const;
template <>
VCL_DLLPUBLIC tools::PolyPolygon
CompiledTransform::Apply<tools::PolyPolygon>(const tools::PolyPolygon& rPolyPoly) const;
template <>
VCL_DLLPUBLIC basegfx::B2DPolygon
CompiledTransform::Apply<basegfx::B2DPolygon>(const basegfx::B2DPolygon& rPoly) const;
template <>
VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CompiledTransform::Apply<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon& rPolyPoly) const;
template <>
VCL_DLLPUBLIC vcl::Region CompiledTransform::Apply<vcl::Region>(const vcl::Region& rRegion) const;
template <>
VCL_DLLPUBLIC LineInfo CompiledTransform::Apply<LineInfo>(const LineInfo& rLineInfo) const;
template <>
VCL_DLLPUBLIC basegfx::B2DRange
CompiledTransform::Apply<basegfx::B2DRange>(const basegfx::B2DRange& rRange) const;

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
