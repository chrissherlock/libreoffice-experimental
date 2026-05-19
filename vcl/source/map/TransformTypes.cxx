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
#include <vcl/mapmod.hxx>

namespace vcl::detail
{
// POINTS

vcl::WindowPoint CoordinateCastTraits<vcl::WindowPoint, vcl::LogicPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicPoint& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowPoint(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowPoint(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPoint CoordinateCastTraits<vcl::LogicPoint, vcl::WindowPoint>::cast(
    const OutputDevice& rDev, const vcl::WindowPoint& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicPoint(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicPoint(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// SIZES

vcl::WindowSize CoordinateCastTraits<vcl::WindowSize, vcl::LogicSize>::cast(
    const OutputDevice& rDev, const vcl::LogicSize& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowSize(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowSize(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicSize CoordinateCastTraits<vcl::LogicSize, vcl::WindowSize>::cast(
    const OutputDevice& rDev, const vcl::WindowSize& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicSize(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicSize(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// RECTANGLES

vcl::WindowRect CoordinateCastTraits<vcl::WindowRect, vcl::LogicRect>::cast(
    const OutputDevice& rDev, const vcl::LogicRect& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowRect(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowRect(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicRect CoordinateCastTraits<vcl::LogicRect, vcl::WindowRect>::cast(
    const OutputDevice& rDev, const vcl::WindowRect& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicRect(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicRect(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// POLYGONS

vcl::WindowPolygon CoordinateCastTraits<vcl::WindowPolygon, vcl::LogicPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicPolygon& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowPolygon(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowPolygon(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPolygon CoordinateCastTraits<vcl::LogicPolygon, vcl::WindowPolygon>::cast(
    const OutputDevice& rDev, const vcl::WindowPolygon& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicPolygon(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicPolygon(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// POLYPOLYGONS

vcl::WindowPolyPolygon CoordinateCastTraits<vcl::WindowPolyPolygon, vcl::LogicPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicPolyPolygon& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowPolyPolygon(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowPolyPolygon(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicPolyPolygon CoordinateCastTraits<vcl::LogicPolyPolygon, vcl::WindowPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::WindowPolyPolygon& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicPolyPolygon(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicPolyPolygon(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

// REGIONS

vcl::WindowRegion CoordinateCastTraits<vcl::WindowRegion, vcl::LogicRegion>::cast(
    const OutputDevice& rDev, const vcl::LogicRegion& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowRegion(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowRegion(
        rMapper
            .Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}

vcl::LogicRegion CoordinateCastTraits<vcl::LogicRegion, vcl::WindowRegion>::cast(
    const OutputDevice& rDev, const vcl::WindowRegion& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::LogicRegion(rMapper.WindowToLogicUnits(rSrc.get(), aConv));
    }
    return vcl::LogicRegion(
        rMapper
            .Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, rDev.GetMappingPolicy() })
            .apply(rSrc.get()));
}
} // namespace vcl::detail

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
