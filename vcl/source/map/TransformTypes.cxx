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
vcl::DeviceRect CoordinateCastTraits<vcl::DeviceRect, vcl::LogicRect>::cast(
    const OutputDevice& rDev, const vcl::LogicRect& rSrc, const MapMode* pMapOverride)
{
    const MapMode& rMapMode = pMapOverride ? *pMapOverride : rDev.GetMapMode();

    return rDev.GetMapper().MapToDevice(rSrc, rMapMode);
}

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
vcl::DeviceRegion CoordinateCastTraits<vcl::DeviceRegion, vcl::LogicRegion>::cast(
    const OutputDevice& rDev, const vcl::LogicRegion& rSrc, const MapMode* pMapOverride)
{
    const MapMode& rMapMode = pMapOverride ? *pMapOverride : rDev.GetMapMode();

    return rDev.GetMapper().MapToDevice(rSrc, rMapMode);
}

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

static basegfx::B2DHomMatrix lcl_BuildMatrixFromMapMode(const MapMode& rMap)
{
    basegfx::B2DHomMatrix aMat;
    aMat.scale(rMap.GetScaleX(), rMap.GetScaleY());
    aMat.translate(rMap.GetOrigin().X(), rMap.GetOrigin().Y());
    return aMat;
}

// BASEGFX B2DPOINT
vcl::DeviceB2DPoint CoordinateCastTraits<vcl::DeviceB2DPoint, vcl::LogicB2DPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPoint& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    return vcl::DeviceB2DPoint(aMat * rSrc.get());
}

vcl::LogicB2DPoint CoordinateCastTraits<vcl::LogicB2DPoint, vcl::DeviceB2DPoint>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPoint& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    aMat.invert();

    return vcl::LogicB2DPoint(aMat * rSrc.get());
}

// BASEGFX B2DPOLYGON
vcl::DeviceB2DPolygon CoordinateCastTraits<vcl::DeviceB2DPolygon, vcl::LogicB2DPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    basegfx::B2DPolygon aResult(rSrc.get());
    aResult.transform(aMat);
    return vcl::DeviceB2DPolygon(aResult);
}

vcl::LogicB2DPolygon CoordinateCastTraits<vcl::LogicB2DPolygon, vcl::DeviceB2DPolygon>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    aMat.invert();

    basegfx::B2DPolygon aResult(rSrc.get());
    aResult.transform(aMat);
    return vcl::LogicB2DPolygon(aResult);
}

// BASEGFX B2DRANGE
vcl::DeviceB2DRange CoordinateCastTraits<vcl::DeviceB2DRange, vcl::LogicB2DRange>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DRange& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    // Ranges require the transform() method for batch-processing the bounds
    basegfx::B2DRange aResult(rSrc.get());
    aResult.transform(aMat);

    return vcl::DeviceB2DRange(aResult);
}

vcl::LogicB2DRange CoordinateCastTraits<vcl::LogicB2DRange, vcl::DeviceB2DRange>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DRange& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    aMat.invert();

    // Ranges require the transform() method
    basegfx::B2DRange aResult(rSrc.get());
    aResult.transform(aMat);

    return vcl::LogicB2DRange(aResult);
}

// BASEGFX B2DPOLYPOLYGON
vcl::DeviceB2DPolyPolygon
CoordinateCastTraits<vcl::DeviceB2DPolyPolygon, vcl::LogicB2DPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPolyPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    // PolyPolygons require the transform() method
    basegfx::B2DPolyPolygon aResult(rSrc.get());
    aResult.transform(aMat);

    return vcl::DeviceB2DPolyPolygon(aResult);
}

vcl::LogicB2DPolyPolygon
CoordinateCastTraits<vcl::LogicB2DPolyPolygon, vcl::DeviceB2DPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPolyPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildMatrixFromMapMode(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    aMat.invert();

    // PolyPolygons require the transform() method
    basegfx::B2DPolyPolygon aResult(rSrc.get());
    aResult.transform(aMat);
    return vcl::LogicB2DPolyPolygon(aResult);
}

} // namespace vcl::detail

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
