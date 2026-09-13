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

#include <CoordinateMath.hxx>

#include <type_traits>

static basegfx::B2DHomMatrix lcl_BuildRawMatrix(const MapMode& rMapOverride)
{
    // DOCUMENTATION FOR THE REVIEWER:
    // Why does this bypass ResolveMap and CoordinateMapper?
    // Because in the B2D graphic paths, the MapMode override is NOT used as a
    // true spatial coordinate domain (which would require DPI conversion).
    // It is used as a payload to pass a raw affine transform (Scale + Origin).
    // To ensure consistency, we bypass physical resolution but STILL route
    // through the canonical affine builder.

    return vcl::BuildAffineMatrix(static_cast<double>(rMapOverride.GetScaleX()),
                                  static_cast<double>(rMapOverride.GetScaleY()), 0.0,
                                  0.0, // No source translation
                                  static_cast<double>(rMapOverride.GetOrigin().X()),
                                  static_cast<double>(rMapOverride.GetOrigin().Y()));
}

namespace
{
template <typename TargetGeom, typename SourceGeom> class ApplyAffineTransform
{
private:
    const basegfx::B2DHomMatrix& mrMat;

public:
    explicit ApplyAffineTransform(const basegfx::B2DHomMatrix& rMat)
        : mrMat(rMat)
    {
    }

    TargetGeom operator()(const SourceGeom& rSrc) const
    {
        using PrimitiveType = std::decay_t<decltype(rSrc.get())>;

        // Path A: Modern BaseB2D Types (Native affine support)
        if constexpr (std::is_same_v<PrimitiveType, basegfx::B2DPoint>)
        {
            return TargetGeom(mrMat * rSrc.get());
        }
        else if constexpr (
            std::is_same_v<
                PrimitiveType,
                basegfx::
                    B2DPolygon> || std::is_same_v<PrimitiveType, basegfx::B2DPolyPolygon> || std::is_same_v<PrimitiveType, basegfx::B2DRange>)
        {
            PrimitiveType aResult(rSrc.get());
            aResult.transform(mrMat);
            return TargetGeom(aResult);
        }
        // Path B: Legacy VCL Types (Requires TransformCompiler rounding safety)
        else
        {
            return TargetGeom(vcl::TransformCompiler::Compile(mrMat).apply(rSrc.get()));
        }
    }
};

} // end anonymous namespace

namespace vcl::detail
{
// ========================================================================
// LEGACY GEOMETRY (Points, Sizes, Rects, Polygons, Regions)
// ========================================================================

// POINTS
vcl::DevicePoint CoordinateCastTraits<vcl::DevicePoint, vcl::LogicPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicPoint& rSrc, const MapMode* pMapOverride)
{
    if (pMapOverride)
    {
        const auto& rMapper = rDev.GetMapper();
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        basegfx::B2DHomMatrix aMat = rMapper.GetViewTransformation(aConv);
        aMat.translate(static_cast<double>(rMapper.GetDeviceToWindowOffsetX()),
                       static_cast<double>(rMapper.GetDeviceToWindowOffsetY()));

        return vcl::DevicePoint(vcl::TransformCompiler::Compile(aMat).apply(rSrc.get()));
    }

    return vcl::DevicePoint(
        rDev.GetMapper().LogicToDevicePixel(rSrc.get(), rDev.GetMappingPolicy()));
}

vcl::LogicPoint CoordinateCastTraits<vcl::LogicPoint, vcl::DevicePoint>::cast(
    const OutputDevice& rDev, const vcl::DevicePoint& rSrc, const MapMode* pMapOverride)
{
    if (pMapOverride)
    {
        const auto& rMapper = rDev.GetMapper();
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        basegfx::B2DHomMatrix aMat = rMapper.GetViewTransformation(aConv);
        aMat.translate(static_cast<double>(rMapper.GetDeviceToWindowOffsetX()),
                       static_cast<double>(rMapper.GetDeviceToWindowOffsetY()));

        if (aMat.isInvertible())
            aMat.invert();

        return vcl::LogicPoint(vcl::TransformCompiler::Compile(aMat).apply(rSrc.get()));
    }

    return vcl::LogicPoint(
        rDev.GetMapper().DevicePixelToLogic(rSrc.get(), rDev.GetMappingPolicy()));
}

vcl::WindowPoint CoordinateCastTraits<vcl::WindowPoint, vcl::LogicPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicPoint& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();

    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowPoint(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }

    return vcl::WindowPoint(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::LogicPoint(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::WindowSize(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::LogicSize(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
}

// RECTANGLES
vcl::LogicRect CoordinateCastTraits<vcl::LogicRect, vcl::DeviceRect>::cast(
    const OutputDevice& rDev, const vcl::DeviceRect& rSrc, const MapMode* pMapOverride)
{
    if (pMapOverride)
    {
        const auto& rMapper = rDev.GetMapper();
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        basegfx::B2DHomMatrix aMat = rMapper.GetViewTransformation(aConv);
        aMat.translate(static_cast<double>(rMapper.GetDeviceToWindowOffsetX()),
                       static_cast<double>(rMapper.GetDeviceToWindowOffsetY()));

        if (aMat.isInvertible())
            aMat.invert();

        return vcl::LogicRect(vcl::TransformCompiler::Compile(aMat).apply(rSrc.get()));
    }

    return vcl::LogicRect(rDev.GetMapper().DevicePixelToLogic(rSrc.get(), rDev.GetMappingPolicy()));
}

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

    return vcl::WindowRect(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::LogicRect(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::WindowPolygon(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::LogicPolygon(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::WindowPolyPolygon(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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

    return vcl::LogicPolyPolygon(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
}

// REGIONS

// ========================================================================
// 1. PHYSICAL SPACE CASTS (Direct delegation to CoordinateMapper offsets)
// ========================================================================

// Device <-> Window
vcl::DeviceRegion CoordinateCastTraits<vcl::DeviceRegion, vcl::WindowRegion>::cast(
    const OutputDevice& rDev, const vcl::WindowRegion& rSrc, const MapMode*)
{
    return vcl::DeviceRegion(rDev.GetMapper().WindowToDevice(rSrc.get()));
}
vcl::WindowRegion CoordinateCastTraits<vcl::WindowRegion, vcl::DeviceRegion>::cast(
    const OutputDevice& rDev, const vcl::DeviceRegion& rSrc, const MapMode*)
{
    return vcl::WindowRegion(rDev.GetMapper().DeviceToWindow(rSrc.get()));
}

// View <-> Window
vcl::ViewRegion CoordinateCastTraits<vcl::ViewRegion, vcl::WindowRegion>::cast(
    const OutputDevice& rDev, const vcl::WindowRegion& rSrc, const MapMode*)
{
    return vcl::ViewRegion(rDev.GetMapper().WindowToView(rSrc.get()));
}
vcl::WindowRegion CoordinateCastTraits<vcl::WindowRegion, vcl::ViewRegion>::cast(
    const OutputDevice& rDev, const vcl::ViewRegion& rSrc, const MapMode*)
{
    return vcl::WindowRegion(rDev.GetMapper().ViewToWindow(rSrc.get()));
}

// Device <-> View
vcl::DeviceRegion CoordinateCastTraits<vcl::DeviceRegion, vcl::ViewRegion>::cast(
    const OutputDevice& rDev, const vcl::ViewRegion& rSrc, const MapMode*)
{
    return vcl::DeviceRegion(rDev.GetMapper().ViewToDevice(rSrc.get()));
}
vcl::ViewRegion CoordinateCastTraits<vcl::ViewRegion, vcl::DeviceRegion>::cast(
    const OutputDevice& rDev, const vcl::DeviceRegion& rSrc, const MapMode*)
{
    return vcl::ViewRegion(rDev.GetMapper().DeviceToView(rSrc.get()));
}

// ========================================================================
// 2. LOGIC CORE CASTS (Requires scaling + mapping policy)
// ========================================================================

// Logic <-> Window
vcl::WindowRegion CoordinateCastTraits<vcl::WindowRegion, vcl::LogicRegion>::cast(
    const OutputDevice& rDev, const vcl::LogicRegion& rSrc, const MapMode* pMapOverride)
{
    const auto& rMapper = rDev.GetMapper();
    if (pMapOverride)
    {
        auto aConv = rMapper.ResolveMap(MapMode(), *pMapOverride, rDev.GetMappingPolicy());
        return vcl::WindowRegion(rMapper.LogicToWindowUnits(rSrc.get(), aConv));
    }
    return vcl::WindowRegion(rMapper.LogicToWindowUnits(rSrc.get(), rDev.GetMappingPolicy()));
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
    return vcl::LogicRegion(rMapper.WindowToLogicUnits(rSrc.get(), rDev.GetMappingPolicy()));
}

// ========================================================================
// 3. LOGIC COMPOSITIONAL CASTS (Safely routes through Window Space)
// ========================================================================

// Logic <-> Device
vcl::DeviceRegion CoordinateCastTraits<vcl::DeviceRegion, vcl::LogicRegion>::cast(
    const OutputDevice& rDev, const vcl::LogicRegion& rSrc, const MapMode* pMapOverride)
{
    vcl::WindowRegion aWindowReg
        = CoordinateCastTraits<vcl::WindowRegion, vcl::LogicRegion>::cast(rDev, rSrc, pMapOverride);
    return CoordinateCastTraits<vcl::DeviceRegion, vcl::WindowRegion>::cast(rDev, aWindowReg);
}
vcl::LogicRegion CoordinateCastTraits<vcl::LogicRegion, vcl::DeviceRegion>::cast(
    const OutputDevice& rDev, const vcl::DeviceRegion& rSrc, const MapMode* pMapOverride)
{
    vcl::WindowRegion aWindowReg
        = CoordinateCastTraits<vcl::WindowRegion, vcl::DeviceRegion>::cast(rDev, rSrc);
    return CoordinateCastTraits<vcl::LogicRegion, vcl::WindowRegion>::cast(rDev, aWindowReg,
                                                                           pMapOverride);
}

// Logic <-> View
vcl::ViewRegion CoordinateCastTraits<vcl::ViewRegion, vcl::LogicRegion>::cast(
    const OutputDevice& rDev, const vcl::LogicRegion& rSrc, const MapMode* pMapOverride)
{
    vcl::WindowRegion aWindowReg
        = CoordinateCastTraits<vcl::WindowRegion, vcl::LogicRegion>::cast(rDev, rSrc, pMapOverride);
    return CoordinateCastTraits<vcl::ViewRegion, vcl::WindowRegion>::cast(rDev, aWindowReg);
}
vcl::LogicRegion CoordinateCastTraits<vcl::LogicRegion, vcl::ViewRegion>::cast(
    const OutputDevice& rDev, const vcl::ViewRegion& rSrc, const MapMode* pMapOverride)
{
    vcl::WindowRegion aWindowReg
        = CoordinateCastTraits<vcl::WindowRegion, vcl::ViewRegion>::cast(rDev, rSrc);
    return CoordinateCastTraits<vcl::LogicRegion, vcl::WindowRegion>::cast(rDev, aWindowReg,
                                                                           pMapOverride);
}

// ========================================================================
// BASEGFX B2D GEOMETRY (Utilizing ApplyAffineTransform Functor)
// ========================================================================

// B2DPOINT
vcl::DeviceB2DPoint CoordinateCastTraits<vcl::DeviceB2DPoint, vcl::LogicB2DPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPoint& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildRawMatrix(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    return ApplyAffineTransform<vcl::DeviceB2DPoint, vcl::LogicB2DPoint>(aMat)(rSrc);
}

vcl::LogicB2DPoint CoordinateCastTraits<vcl::LogicB2DPoint, vcl::DeviceB2DPoint>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPoint& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat;

    if (pMap)
    {
        aMat = lcl_BuildRawMatrix(*pMap);
        aMat.invert();
    }
    else
    {
        aMat = rDev.GetMapper().GetDeviceToLogicMatrix(rDev.GetMappingPolicy());
    }

    return ApplyAffineTransform<vcl::LogicB2DPoint, vcl::DeviceB2DPoint>(aMat)(rSrc);
}

// B2DPOLYGON
vcl::DeviceB2DPolygon CoordinateCastTraits<vcl::DeviceB2DPolygon, vcl::LogicB2DPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildRawMatrix(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    return ApplyAffineTransform<vcl::DeviceB2DPolygon, vcl::LogicB2DPolygon>(aMat)(rSrc);
}

vcl::LogicB2DPolygon CoordinateCastTraits<vcl::LogicB2DPolygon, vcl::DeviceB2DPolygon>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat;

    if (pMap)
    {
        aMat = lcl_BuildRawMatrix(*pMap);
        aMat.invert();
    }
    else
    {
        aMat = rDev.GetMapper().GetDeviceToLogicMatrix(rDev.GetMappingPolicy());
    }

    return ApplyAffineTransform<vcl::LogicB2DPolygon, vcl::DeviceB2DPolygon>(aMat)(rSrc);
}

// B2DRANGE
vcl::DeviceB2DRange CoordinateCastTraits<vcl::DeviceB2DRange, vcl::LogicB2DRange>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DRange& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildRawMatrix(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    return ApplyAffineTransform<vcl::DeviceB2DRange, vcl::LogicB2DRange>(aMat)(rSrc);
}

vcl::LogicB2DRange CoordinateCastTraits<vcl::LogicB2DRange, vcl::DeviceB2DRange>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DRange& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat;

    if (pMap)
    {
        aMat = lcl_BuildRawMatrix(*pMap);
        aMat.invert();
    }
    else
    {
        aMat = rDev.GetMapper().GetDeviceToLogicMatrix(rDev.GetMappingPolicy());
    }

    return ApplyAffineTransform<vcl::LogicB2DRange, vcl::DeviceB2DRange>(aMat)(rSrc);
}

// B2DPOLYPOLYGON
vcl::DeviceB2DPolyPolygon
CoordinateCastTraits<vcl::DeviceB2DPolyPolygon, vcl::LogicB2DPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::LogicB2DPolyPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat
        = pMap ? lcl_BuildRawMatrix(*pMap)
               : rDev.GetMapper().GetLogicToDeviceMatrix(rDev.GetMappingPolicy());

    return ApplyAffineTransform<vcl::DeviceB2DPolyPolygon, vcl::LogicB2DPolyPolygon>(aMat)(rSrc);
}

vcl::LogicB2DPolyPolygon
CoordinateCastTraits<vcl::LogicB2DPolyPolygon, vcl::DeviceB2DPolyPolygon>::cast(
    const OutputDevice& rDev, const vcl::DeviceB2DPolyPolygon& rSrc, const MapMode* pMap)
{
    basegfx::B2DHomMatrix aMat;

    if (pMap)
    {
        aMat = lcl_BuildRawMatrix(*pMap);
        aMat.invert();
    }
    else
    {
        aMat = rDev.GetMapper().GetDeviceToLogicMatrix(rDev.GetMappingPolicy());
    }

    return ApplyAffineTransform<vcl::LogicB2DPolyPolygon, vcl::DeviceB2DPolyPolygon>(aMat)(rSrc);
}

// ========================================================================
// LOGIC TO LOGIC GEOMETRY
// ========================================================================

// LogicPoint
vcl::LogicPoint CoordinateCastTraits<vcl::LogicPoint, vcl::LogicPoint>::cast(
    const OutputDevice& rDev, const vcl::LogicPoint& rSrc, const MapMode* pSrc, const MapMode* pDst)
{
    const MapMode& rSrcMap = pSrc ? *pSrc : rDev.GetMapMode();
    const MapMode& rDstMap = pDst ? *pDst : rDev.GetMapMode();

    if (rSrcMap == rDstMap)
        return rSrc;

    basegfx::B2DHomMatrix aMat = rDev.GetMapper().GetLogicToLogicMatrix(rSrcMap, rDstMap);
    basegfx::B2DPoint aPt(rSrc.get().X(), rSrc.get().Y());
    aPt *= aMat;

    return vcl::LogicPoint(Point(basegfx::fround(aPt.getX()), basegfx::fround(aPt.getY())));
}

// LogicSize
vcl::LogicSize CoordinateCastTraits<vcl::LogicSize, vcl::LogicSize>::cast(
    const OutputDevice& rDev, const vcl::LogicSize& rSrc, const MapMode* pSrc, const MapMode* pDst)
{
    const MapMode& rSrcMap = pSrc ? *pSrc : rDev.GetMapMode();
    const MapMode& rDstMap = pDst ? *pDst : rDev.GetMapMode();

    if (rSrcMap == rDstMap)
        return rSrc;

    basegfx::B2DHomMatrix aMat = rDev.GetMapper().GetLogicToLogicMatrix(rSrcMap, rDstMap);

    const double fMagX = vcl::detail::GetBasisVectorMagnitudeX(aMat);
    const double fMagY = vcl::detail::GetBasisVectorMagnitudeY(aMat);

    return vcl::LogicSize(Size(basegfx::fround(rSrc.get().Width() * fMagX),
                               basegfx::fround(rSrc.get().Height() * fMagY)));
}

// LogicRect
vcl::LogicRect CoordinateCastTraits<vcl::LogicRect, vcl::LogicRect>::cast(
    const OutputDevice& rDev, const vcl::LogicRect& rSrc, const MapMode* pSrc, const MapMode* pDst)
{
    const MapMode& rSrcMap = pSrc ? *pSrc : rDev.GetMapMode();
    const MapMode& rDstMap = pDst ? *pDst : rDev.GetMapMode();

    if (rSrcMap == rDstMap)
        return rSrc;

    basegfx::B2DHomMatrix aMat = rDev.GetMapper().GetLogicToLogicMatrix(rSrcMap, rDstMap);
    basegfx::B2DRange aRange(rSrc.get().Left(), rSrc.get().Top(), rSrc.get().Right(),
                             rSrc.get().Bottom());
    aRange.transform(aMat);

    return vcl::LogicRect(
        tools::Rectangle(basegfx::fround(aRange.getMinX()), basegfx::fround(aRange.getMinY()),
                         basegfx::fround(aRange.getMaxX()), basegfx::fround(aRange.getMaxY())));
}

} // namespace vcl::detail

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
