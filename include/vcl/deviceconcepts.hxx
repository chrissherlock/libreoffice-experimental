/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/long.hxx>

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

/**
 * Trait: supports_animation
 * Interactive windows support VCL's internal timer-based animation loop.
 * VirtualDevices and Printers are static and return false.
 */
template <typename T> struct supports_animation : std::false_type
{
};
template <> struct supports_animation<vcl::WindowOutputDevice> : std::true_type
{
};

/**
 * Concept: Animatable
 * * Constrains template logic and policy dispatch to device types capable
 * of handling asynchronous frame updates and repaints.
 * * This allows the Animation subsystem to bypass timer overhead on static
 * devices at compile-time.
 */
template <typename T> concept Animatable = supports_animation<T>::value;

/**
 * Trait: is_screen_compatible
 * * * Indicates whether a device's logical coordinate system and resolution (DPI)
 * should remain synchronized with the primary system display.
 * * * Behavior:
 * - When TRUE: The device is treated as a "Soft-copy" surface. It will inherit
 * system-wide changes to DPI, font scaling, and UI settings (see svapp.cxx).
 * This ensures VirtualDevices used for UI buffering match the Window they
 * render into.
 * - When FALSE: The device is a "Hard-copy" surface (like a Printer). Its
 * metrics are governed by physical page characteristics or fixed document
 * definitions, independent of the user's monitor resolution.
 */
template <typename T> struct is_screen_compatible : std::true_type
{
};

/** Specialization: Printers operate on physical page metrics, not screen metrics. */
template <> struct is_screen_compatible<Printer> : std::false_type
{
};

/**
 * Concept: ScreenCompatible
 * * Constrains logic to device types that participate in the global UI scaling
 * and settings synchronization loop.
 */
template <typename T> concept ScreenCompatible = is_screen_compatible<T>::value;

/**
 * Trait: device_reference_width
 * * STL-style trait to resolve the physical width of a device.
 * * Implementation is provided in headers where the concrete types are complete.
 */
template <typename T> struct device_reference_width
{
    static tools::Long get(const T& rDevice);
};

/** Helper: get_reference_width_v */
template <typename T> inline tools::Long get_reference_width_v(const T& rDevice)
{
    return device_reference_width<T>::get(rDevice);
}

/**
 * Concept: ReferenceDevice
 * * Constraints types to those that can provide physical dimensions.
 */
template <typename T> concept ReferenceDevice = requires(const T& rDevice)
{
    {
        vcl::get_reference_width_v(rDevice)
    }
    ->std::convertible_to<tools::Long>;
};

/**
 * Trait: requires_high_contrast_borders
 * Determines if a device requires borders to be rendered as solid,
 * high-contrast closed paths (COL_BLACK) for legibility on physical media.
 * Typically true for Printers, false for screen-based raster devices.
 */
template <typename T> struct requires_high_contrast_borders : std::false_type
{
};

// Specialization: Printers require high-contrast boundaries on physical paper.
template <> struct requires_high_contrast_borders<Printer> : std::true_type
{
};

template <typename T>
inline constexpr bool requires_high_contrast_borders_v = requires_high_contrast_borders<T>::value;

/**
 * Concept: HighContrastOutput
 * Satisfied by devices that must prioritize visibility and stroke
 * consistency over themed or decorative line styles.
 */
template <typename T> concept HighContrastOutput = requires_high_contrast_borders_v<T>;

/**
 * Trait: avoids_vector_overdraw
 * Determines if the device penalizes overlapping vector operations (the "Painter's Algorithm").
 * When true, gradient renderers must calculate exact, non-overlapping geometric bands
 * to prevent physical ink saturation, spool file bloat, or RIP memory exhaustion.
 * Typically false for screens, true for printers.
 */
template <typename T> struct avoids_vector_overdraw : std::false_type
{
};

// Specialization: Printers require non-overlapping exact geometry.
template <> struct avoids_vector_overdraw<Printer> : std::true_type
{
};

template <typename T>
inline constexpr bool avoids_vector_overdraw_v = avoids_vector_overdraw<T>::value;

/**
 * Concept: AvoidsOverdraw
 * Satisfied by physical devices that require disjoint, non-overlapping
 * geometry to function correctly and efficiently.
 */
template <typename T> concept AvoidsOverdraw = avoids_vector_overdraw_v<T>;

/**
 * Trait: supports_paint_events
 * Determines if the device type processes OS-level repaints and must
 * restrict drawing to a specific dirty region.
 */
template <typename T> struct supports_paint_events : std::false_type
{
};

// Specialization: WindowOutputDevice handles OS window paints.
template <> struct supports_paint_events<vcl::WindowOutputDevice> : std::true_type
{
};

template <typename T>
inline constexpr bool supports_paint_events_v = supports_paint_events<T>::value;

/**
 * Concept: PaintEventCapable
 * Satisfied by windowing devices that manage OS-level paint regions.
 */
template <typename T> concept PaintEventCapable = supports_paint_events_v<T>;

/**
 * Trait: has_page_offset
 * Explicitly marks a device as having a physical page offset (Hard Margins).
 */
template <typename T> struct has_page_offset : std::false_type
{
};

template <typename T> inline constexpr bool has_page_offset_v = has_page_offset<T>::value;

/**
 * Concept: PageDevice
 * Enforces that any device opting into has_page_offset MUST implement
 * a GetPageOffset() method.
 */
template <typename T> concept PageDevice = has_page_offset_v<T>&& requires(const T& rDev)
{
    { rDev.GetPageOffset() }; // The compiler will fail if this method doesn't exist
};

/**
 * Trait: supports_glyph_synthesis
 * Determines if the device rendering engine can synthetically manipulate
 * glyphs (e.g., arbitrary rotation, faux bold, faux italic, scaling)
 * when the native font metric lacks hardware support.
 * Defaults to true, as most VCL devices are raster-backed.
 */
template <typename T> struct supports_glyph_synthesis : std::true_type
{
};

template <typename T>
inline constexpr bool supports_glyph_synthesis_v = supports_glyph_synthesis<T>::value;

/**
 * Concept: GlyphSynthesisCapable
 */
template <typename T> concept GlyphSynthesisCapable = supports_glyph_synthesis_v<T>;

/**
 * @brief Type trait: is_readable_raster_device_v
 *
 * Determines if a given OutputDevice type inherently possesses a two-way,
 * readable pixel backing store in memory.
 *
 * By default, this evaluates to `false` (Fail-Safe). It strictly whitelists
 * VirtualDevice and Window, automatically covering any of their derived
 * classes (e.g., SystemWindow, WorkWindow) via std::is_base_of_v.
 *
 * Write-only output mediums like Printer and PDFWriter will correctly
 * evaluate to false.
 */
template <typename T>
inline constexpr bool is_readable_raster_device_v
    = std::is_base_of_v<VirtualDevice, T> || std::is_base_of_v<WindowOutputDevice, T>;

/**
 * @brief Concept: ReadableRasterDevice
 *
 * Satisfied exclusively by devices that can safely execute pixel-read
 * operations, such as GetBitmap() or GetPixel().
 *
 * Prevents write-only devices (like Printers or PDF exporters) from
 * executing structurally impossible raster readbacks.
 */
template <typename T> concept ReadableRasterDevice = is_readable_raster_device_v<T>;

/**
 * @brief Type trait: is_subtractive_color_device_v
 *
 * Determines if a given OutputDevice relies on a subtractive color model
 * (e.g., CMYK on printers).
 *
 * In a subtractive model, "white" is the absence of ink (the raw canvas),
 * meaning white UI elements cannot be painted over a white background and
 * must be shaded to remain visible.
 */
template <typename T>
inline constexpr bool is_subtractive_color_device_v = std::is_base_of_v<Printer, T>;

/**
 * @brief Concept: SubtractiveColorDevice
 *
 * Satisfied by devices that use subtractive rendering. This concept is
 * used to trigger necessary UI shading adjustments when rendering elements
 * that would otherwise rely on additive white pixel emission.
 */
template <typename T> concept SubtractiveColorDevice = is_subtractive_color_device_v<T>;

/**
 * @brief Type trait: reduces_gradients_v
 *
 * Determines if a given OutputDevice inherently requires high-resolution
 * gradients to be downgraded (banded or flattened) to conserve memory
 * or spool size on physical media.
 */
template <typename T> inline constexpr bool reduces_gradients_v = std::is_base_of_v<Printer, T>;

/**
 * @brief Concept: GradientReductionCapable
 *
 * Satisfied by devices that intercept and simplify gradient rendering
 * based on hardware or user-configured constraints.
 */
template <typename T> concept GradientReductionCapable = reduces_gradients_v<T>;

/**
 * @brief Type trait: is_framed_device_v
 *
 * Determines if the device has physical or logical boundaries (frames/insets)
 * managed by an external entity (like a Window Manager).
 */
template <typename T>
inline constexpr bool is_framed_device_v = std::is_base_of_v<WindowOutputDevice, T>;

/**
 * @brief Concept: FramedDevice
 *
 * Satisfied by devices that must account for external border insets
 * when calculating their total device area.
 */
template <typename T> concept FramedDevice = is_framed_device_v<T>;
} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
