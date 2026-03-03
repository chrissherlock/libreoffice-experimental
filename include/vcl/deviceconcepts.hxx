/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <type_traits>
#include <concepts>

// Forward declarations to avoid circular dependencies with outdev.hxx
class OutputDevice;
class Printer;
class VirtualDevice;

namespace vcl
{
class WindowOutputDevice;
class PDFWriterImpl;

/**
 * Trait: supports_hw_acceleration
 * Determines if the device type can potentially utilize GPU/Skia backends
 * for fast transformed drawing.
 */
template <typename T> struct supports_hw_acceleration : std::false_type
{
};
template <typename T>
inline constexpr bool supports_hw_acceleration_v = supports_hw_acceleration<T>::value;

/**
 * Trait: needs_strict_culling
 * Determines if the device requires aggressive geometric culling before
 * rendering to prevent memory exhaustion (e.g., high-DPI Printers).
 */
template <typename T> struct needs_strict_culling : std::false_type
{
};
template <typename T> inline constexpr bool needs_strict_culling_v = needs_strict_culling<T>::value;

/**
 * Trait: is_logical_recorder
 * Determines if the device records logical operations (like a Metafile)
 * rather than rasterizing directly to physical device pixels.
 */
template <typename T> struct is_logical_recorder : std::false_type
{
};
template <typename T> inline constexpr bool is_logical_recorder_v = is_logical_recorder<T>::value;

/**
 * Concept: HWAccelerated
 * Satisfied by devices that provide hardware-accelerated rendering paths.
 */
template <typename T> concept HWAccelerated = supports_hw_acceleration_v<T>;

/**
 * Concept: StrictlyCulled
 * Satisfied by devices that must intersect draw operations with their
 * visible physical boundaries before processing.
 */
template <typename T> concept StrictlyCulled = needs_strict_culling_v<T>;

/**
 * Concept: LogicalRecorder
 * Satisfied by devices that intercept and store vector operations
 * independently of physical screen/printer resolution.
 */
template <typename T> concept LogicalRecorder = is_logical_recorder_v<T>;

/**
 * Concept: PhysicalDevice
 * Satisfied by devices that rasterize directly to physical pixels.
 */
template <typename T> concept PhysicalDevice = !is_logical_recorder_v<T>;

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
