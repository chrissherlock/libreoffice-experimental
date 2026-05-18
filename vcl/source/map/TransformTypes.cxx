/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/CoordinateMapper.hxx>
#include <vcl/TransformTypes.hxx>
#include <vcl/outdev.hxx>

namespace vcl::detail
{
// ========================================================================
// POINTS
// ========================================================================
vcl::WindowPoint
CoordinateCastTraits<vcl::WindowPoint, vcl::LogicPoint>::cast(const OutputDevice& rDev,
                                                              const vcl::LogicPoint& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowPoint(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPoint
CoordinateCastTraits<vcl::LogicPoint, vcl::WindowPoint>::cast(const OutputDevice& rDev,
                                                              const vcl::WindowPoint& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicPoint(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// ========================================================================
// SIZES
// ========================================================================
vcl::WindowSize
CoordinateCastTraits<vcl::WindowSize, vcl::LogicSize>::cast(const OutputDevice& rDev,
                                                            const vcl::LogicSize& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowSize(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicSize
CoordinateCastTraits<vcl::LogicSize, vcl::WindowSize>::cast(const OutputDevice& rDev,
                                                            const vcl::WindowSize& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicSize(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// ========================================================================
// RECTANGLES
// ========================================================================
vcl::WindowRect
CoordinateCastTraits<vcl::WindowRect, vcl::LogicRect>::cast(const OutputDevice& rDev,
                                                            const vcl::LogicRect& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowRect(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicRect
CoordinateCastTraits<vcl::LogicRect, vcl::WindowRect>::cast(const OutputDevice& rDev,
                                                            const vcl::WindowRect& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicRect(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// ========================================================================
// POLYGONS
// ========================================================================
vcl::WindowPolygon
CoordinateCastTraits<vcl::WindowPolygon, vcl::LogicPolygon>::cast(const OutputDevice& rDev,
                                                                  const vcl::LogicPolygon& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowPolygon(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPolygon
CoordinateCastTraits<vcl::LogicPolygon, vcl::WindowPolygon>::cast(const OutputDevice& rDev,
                                                                  const vcl::WindowPolygon& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicPolygon(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// ========================================================================
// POLYPOLYGONS
// ========================================================================
vcl::WindowPolyPolygon CoordinateCastTraits<vcl::WindowPolyPolygon, vcl::LogicPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicPolyPolygon& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowPolyPolygon(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPolyPolygon CoordinateCastTraits<vcl::LogicPolyPolygon, vcl::WindowPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::WindowPolyPolygon& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicPolyPolygon(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// ========================================================================
// REGIONS
// ========================================================================
vcl::WindowRegion
CoordinateCastTraits<vcl::WindowRegion, vcl::LogicRegion>::cast(const OutputDevice& rDev,
                                                                const vcl::LogicRegion& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::WindowRegion(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicRegion
CoordinateCastTraits<vcl::LogicRegion, vcl::WindowRegion>::cast(const OutputDevice& rDev,
                                                                const vcl::WindowRegion& rSrc)
{
    const auto& rMapper = rDev.GetMapper();
    return vcl::LogicRegion(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}
} // namespace vcl::detail

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
