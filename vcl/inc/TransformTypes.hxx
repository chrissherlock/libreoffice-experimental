/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <bitset>

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
    Window,
    Device
};

struct TransformRequest
{
    CoordinateSpace eFrom = CoordinateSpace::Logic;
    CoordinateSpace eTo = CoordinateSpace::Device;
    vcl::MappingPolicy Policy = vcl::MappingPolicy::ApplyMapMode; // Changed from bool
};

// Explicit slots for the O(1) Transform Register File
enum class TransformSlot : size_t
{
    LogicToWindow_Mapped = 0,
    LogicToWindow_Unmapped = 1,
    LogicToDevice_Mapped = 2,
    LogicToDevice_Unmapped = 3,
    WindowToLogic_Mapped = 4,
    WindowToLogic_Unmapped = 5,
    DeviceToLogic_Mapped = 6,
    DeviceToLogic_Unmapped = 7,
    Count = 8
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

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
