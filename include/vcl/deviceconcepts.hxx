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
 * Trait: supports_native_alpha
 * Determines if the device can handle per-pixel alpha blending natively.
 * Most raster devices do; high-level vector devices (Printers) often do not.
 */
template <typename T> struct supports_native_alpha : std::true_type
{
};
// Specialization for Printer (or any other non-alpha-capable device)
template <> struct supports_native_alpha<Printer> : std::false_type
{
};

template <typename T>
inline constexpr bool supports_native_alpha_v = supports_native_alpha<T>::value;

/**
 * Trait: requires_banding
 * Determines if the device requires transparency to be handled via
 * banding or slicing (typical for Printer/PostScript paths).
 */
template <typename T> struct requires_banding : std::false_type
{
};
template <> struct requires_banding<Printer> : std::true_type
{
};

template <typename T> inline constexpr bool requires_banding_v = requires_banding<T>::value;

/**
 * Trait: supports_subsampling
 * Determines if the device benefits from high-quality software subsampling
 * during bitmap downscaling. Typically true for screen/raster devices,
 * false for printers which prefer raw high-res data.
 */
template <typename T> struct supports_subsampling : std::true_type
{
};

// Specialization for Printer (Printers handle their own high-res scaling)
template <> struct supports_subsampling<Printer> : std::false_type
{
};

template <typename T> inline constexpr bool supports_subsampling_v = supports_subsampling<T>::value;

/**
 * Trait: uses_managed_font_cache
 * * Defines whether a device relies on the vcl::font::FontController
 * for its resource lifecycle.
 * * Most OutputDevice subclasses (VirtualDevice, Window) are 'true' by default.
 * Printers are a known exception as they handle font instances manually
 * during the print job lifecycle.
 */
template <typename T> struct uses_managed_font_cache : std::true_type
{
};

/** Specialization: Printers do not use the managed FontController cache. */
template <> struct uses_managed_font_cache<Printer> : std::false_type
{
};

/**
 * Concept: ManagedFontCache
 * * Constrains templates to device types that support FontController-based
 * resource management. This allows for compile-time branching in
 * font release policies.
 */
template <typename T> concept ManagedFontCache = uses_managed_font_cache<T>::value;

/** * Font Resource Policy
 * * Forward declaration of the static policy used to clean up font resources.
 * The implementation is isolated in vcl/source/outdev/font.cxx to keep
 * internal font headers (like ImplFontCache) private.
 */
template <typename T> struct font_resource_policy;

/**
 * Font Resource Policy
 * The static interface for resource cleanup.
 */
template <typename T> struct font_resource_policy;

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

/**
 * Concept: AlphaCapable
 * Satisfied by devices that can handle DrawDeviceBitmap with Alpha.
 */
template <typename T> concept AlphaCapable = supports_native_alpha_v<T>;

/**
 * Concept: BandedPrinting
 * Satisfied by devices requiring ImplPrintTransparent.
 */
template <typename T> concept BandedPrinting = requires_banding_v<T>;

/**
 * Concept: SubsamplingCapable
 * Satisfied by devices that should utilize BitmapRenderer::ApplySubsampling.
 */
template <typename T> concept SubsamplingCapable = supports_subsampling_v<T>;

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
