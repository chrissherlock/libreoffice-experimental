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
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>

#include <vcl/MappingPolicy.hxx>

#include <bitset>

namespace tools
{
class Polygon;
}

// The strict execution instruction set
enum class TransformMode
{
    Identity,
    Translation,
    AxisAlignedAffine,
    AffineFallback
};

enum class CoordinateSpace
{
    Logic,
    View,
    Window,
    Device
};

struct TransformRequest
{
    CoordinateSpace eFrom = CoordinateSpace::Logic;
    CoordinateSpace eTo = CoordinateSpace::Device;
    vcl::MappingPolicy Policy = vcl::MappingPolicy::ApplyMapMode; // Changed from bool
};

/**
 * Explicit routing keys for the TransformPlan cache.
 * This maps a CoordinateSpace pair + a MappingPolicy directly to a cache slot,
 * completely eliminating fragile slot arithmetic.
 */
enum class TransformKey : size_t
{
    LogicToWindow_Mapped = 0,
    LogicToWindow_Unmapped,
    WindowToLogic_Mapped,
    WindowToLogic_Unmapped,
    LogicToDevice_Mapped,
    LogicToDevice_Unmapped,
    DeviceToLogic_Mapped,
    DeviceToLogic_Unmapped,
    DeviceToWindow,
    WindowToDevice,
    Count
};

enum class GeometryInvariant : size_t
{
    AxisAlignment, // Edges remain parallel to X/Y axes (Critical for Rectangle/Scalar)
    Orthogonality, // Basis vectors remain 90° to each other (Critical for Shear-safety)
    Orientation, // Handedness/Mirroring state (Critical for Size)
    Parallelism, // Parallel lines stay parallel (Always true for Affine)
    Connectivity, // Shapes stay "in one piece" (Always true for Affine)
    COUNT
};

struct TransformContract
{
    std::bitset<static_cast<size_t>(GeometryInvariant::COUNT)> maPreserved;

    bool preserves(GeometryInvariant inv) const
    {
        return maPreserved.test(static_cast<size_t>(inv));
    }
};

// ========================================================================
// ZERO-COST PHANTOM TYPES (Compile-Time Coordinate Space Safety)
// ========================================================================

namespace vcl
{
// The Phantom Tags (Representing Coordinate Spaces)
struct SpaceView
{
};
struct SpaceLogic
{
};
struct SpaceWindow
{
};
struct SpaceDevice
{
};

// The Universal Strongly-Typed Wrapper
template <typename Space, typename T> struct TypedGeom
{
    T maData;

    // Explicit constructor prevents accidental implicit conversions
    explicit TypedGeom(const T& rData)
        : maData(rData)
    {
    }
    explicit TypedGeom(T&& rData)
        : maData(std::move(rData))
    {
    }

    // Allow explicit unwrapping when interfacing with legacy APIs
    const T& get() const { return maData; }
    T& get() { return maData; }

    // Transparent operator overloading for base type
    const T* operator->() const { return &maData; }
    T* operator->() { return &maData; }

    /**
     * The Monadic Map Operation
     * Takes a callable function/lambda, applies it to the underlying geometry,
     * and returns a NEW TypedGeom wrapped safely back in the SAME space context.
     */
    template <typename Func> constexpr auto map(Func&& f) const
    {
        // std::invoke_result_t automatically deduces the return type of the function
        using ReturnType = std::invoke_result_t<Func, const T&>;
        return TypedGeom<Space, ReturnType>(f(maData));
    }
};

// --- Type Aliases for the Modern API ---
using LogicPoint = TypedGeom<SpaceLogic, Point>;
using ViewPoint = TypedGeom<SpaceView, Point>;
using WindowPoint = TypedGeom<SpaceWindow, Point>;
using DevicePoint = TypedGeom<SpaceDevice, Point>;

using LogicSize = TypedGeom<SpaceLogic, Size>;
using ViewSize = TypedGeom<SpaceView, Size>;
using WindowSize = TypedGeom<SpaceWindow, Size>;
using DeviceSize = TypedGeom<SpaceDevice, Size>;

using LogicRect = TypedGeom<SpaceLogic, tools::Rectangle>;
using ViewRect = TypedGeom<SpaceView, tools::Rectangle>;
using WindowRect = TypedGeom<SpaceWindow, tools::Rectangle>;
using DeviceRect = TypedGeom<SpaceDevice, tools::Rectangle>;

using LogicPolygon = TypedGeom<SpaceLogic, tools::Polygon>;
using ViewPolygon = TypedGeom<SpaceView, tools::Polygon>;
using WindowPolygon = TypedGeom<SpaceWindow, tools::Polygon>;
using DevicePolygon = TypedGeom<SpaceDevice, tools::Polygon>;

// PolyPolygon
using LogicPolyPolygon = TypedGeom<SpaceLogic, tools::PolyPolygon>;
using DevicePolyPolygon = TypedGeom<SpaceDevice, tools::PolyPolygon>;
} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
