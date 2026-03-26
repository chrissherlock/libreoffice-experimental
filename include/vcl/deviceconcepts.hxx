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
#include <tools/gen.hxx> // for Point
#include <type_traits>
#include <concepts>

namespace vcl
{
// Helper for Member Detection
// We use a primary template that defaults to false, and a specialization
// that triggers if the static member exists.

// Hardware Acceleration
template <typename T, typename = void> struct supports_hw_acceleration : std::false_type
{
};
template <typename T>
struct supports_hw_acceleration<T, std::void_t<decltype(T::is_hw_accelerated_v)>>
    : std::bool_constant<T::is_hw_accelerated_v>
{
};

// Strict Culling
template <typename T, typename = void> struct needs_strict_culling : std::false_type
{
};
template <typename T>
struct needs_strict_culling<T, std::void_t<decltype(T::is_strictly_culled_v)>>
    : std::bool_constant<T::is_strictly_culled_v>
{
};

// Logical Operations (Metafile recording)
template <typename T, typename = void> struct is_logical_recorder : std::false_type
{
};
template <typename T>
struct is_logical_recorder<T, std::void_t<decltype(T::is_logical_recorder_v)>>
    : std::bool_constant<T::is_logical_recorder_v>
{
};

// Native Alpha Support
template <typename T, typename = void> struct supports_native_alpha : std::true_type
{
}; // Default to true
template <typename T>
struct supports_native_alpha<T, std::void_t<decltype(T::is_alpha_capable_v)>>
    : std::bool_constant<T::is_alpha_capable_v>
{
};

// Banding Requirement
template <typename T, typename = void> struct requires_banding : std::false_type
{
};
template <typename T>
struct requires_banding<T, std::void_t<decltype(T::is_banded_printing_v)>>
    : std::bool_constant<T::is_banded_printing_v>
{
};

// Bitmap Subsampling
template <typename T, typename = void> struct supports_subsampling : std::true_type
{
}; // Default to true
template <typename T>
struct supports_subsampling<T, std::void_t<decltype(T::is_subsampling_capable_v)>>
    : std::bool_constant<T::is_subsampling_capable_v>
{
};

// Font Cache Management
template <typename T, typename = void> struct uses_managed_cache : std::true_type
{
}; // Default to true
template <typename T>
struct uses_managed_cache<T, std::void_t<decltype(T::is_managed_font_cache_v)>>
    : std::bool_constant<T::is_managed_font_cache_v>
{
};

// Animation/Timer support
template <typename T, typename = void> struct supports_animation : std::false_type
{
};
template <typename T>
struct supports_animation<T, std::void_t<decltype(T::is_animatable_v)>>
    : std::bool_constant<T::is_animatable_v>
{
};

// Screen/UI Scaling Compatibility
template <typename T, typename = void> struct is_screen_compatible : std::true_type
{
}; // Default to true
template <typename T>
struct is_screen_compatible<T, std::void_t<decltype(T::is_screen_compatible_v)>>
    : std::bool_constant<T::is_screen_compatible_v>
{
};

// High Contrast Borders
template <typename T, typename = void> struct is_high_contrast : std::false_type
{
};
template <typename T>
struct is_high_contrast<T, std::void_t<decltype(T::is_high_contrast_v)>>
    : std::bool_constant<T::is_high_contrast_v>
{
};

// Overdraw Avoidance (Painter's Algorithm bypass)
template <typename T, typename = void> struct avoids_overdraw : std::false_type
{
};
template <typename T>
struct avoids_overdraw<T, std::void_t<decltype(T::is_overdraw_avoided_v)>>
    : std::bool_constant<T::is_overdraw_avoided_v>
{
};

// OS Paint Events
template <typename T, typename = void> struct supports_paint_events : std::false_type
{
};
template <typename T>
struct supports_paint_events<T, std::void_t<decltype(T::is_paint_event_capable_v)>>
    : std::bool_constant<T::is_paint_event_capable_v>
{
};

// Page Offset / Margins
template <typename T, typename = void> struct has_page_offset : std::false_type
{
};
template <typename T>
struct has_page_offset<T, std::void_t<decltype(T::is_page_device_v)>>
    : std::bool_constant<T::is_page_device_v>
{
};

// Raster Readback (GetPixel/GetBitmap)
template <typename T, typename = void> struct is_readable_raster : std::false_type
{
};
template <typename T>
struct is_readable_raster<T, std::void_t<decltype(T::is_readable_raster_v)>>
    : std::bool_constant<T::is_readable_raster_v>
{
};

// Subtractive vs Additive Color
template <typename T, typename = void> struct is_subtractive : std::false_type
{
};
template <typename T>
struct is_subtractive<T, std::void_t<decltype(T::is_subtractive_v)>>
    : std::bool_constant<T::is_subtractive_v>
{
};

// UI Theme Synchronization
template <typename T, typename = void> struct is_themed : std::false_type
{
};
template <typename T>
struct is_themed<T, std::void_t<decltype(T::is_themed_v)>> : std::bool_constant<T::is_themed_v>
{
};

// Framed (Window-managed) boundaries
template <typename T, typename = void> struct is_framed : std::false_type
{
};
template <typename T>
struct is_framed<T, std::void_t<decltype(T::is_framed_v)>> : std::bool_constant<T::is_framed_v>
{
};

// Glyph Synthesis Support
template <typename T, typename = void> struct supports_glyph_synthesis : std::true_type
{
}; // Default to true
template <typename T>
struct supports_glyph_synthesis<T, std::void_t<decltype(T::is_glyph_synthesis_capable_v)>>
    : std::bool_constant<T::is_glyph_synthesis_capable_v>
{
};

// Auto-Mirroring Support (Legacy hotfix for AOO i55719)
template <typename T, typename = void> struct allows_auto_mirroring : std::true_type
{
}; // Default to true
template <typename T>
struct allows_auto_mirroring<T, std::void_t<decltype(T::allows_auto_mirroring_v)>>
    : std::bool_constant<T::allows_auto_mirroring_v>
{
};

// Double Buffering Support
template <typename T, typename = void> struct supports_double_buffering : std::false_type
{
};
template <typename T>
struct supports_double_buffering<T, std::void_t<decltype(T::is_double_buffered_v)>>
    : std::bool_constant<T::is_double_buffered_v>
{
};

// Viewport-based clipping requirements
template <typename T, typename = void> struct requires_viewport_clipping : std::false_type
{
};
template <typename T>
struct requires_viewport_clipping<T, std::void_t<decltype(T::is_viewport_clipping_required_v)>>
    : std::bool_constant<T::is_viewport_clipping_required_v>
{
};

// Native Widget Support (OS Theming)
template <typename T, typename = void> struct supports_native_widgets : std::false_type
{
};
template <typename T>
struct supports_native_widgets<T, std::void_t<decltype(T::is_native_widget_capable_v)>>
    : std::bool_constant<T::is_native_widget_capable_v>
{
};

// Flush Support (Hardware/OS Buffer Synchronization)
template <typename T, typename = void> struct supports_flush : std::false_type
{
};
template <typename T>
struct supports_flush<T, std::void_t<decltype(T::is_flushable_v)>>
    : std::bool_constant<T::is_flushable_v>
{
};

// RTL Mirroring Reference Support
template <typename T, typename = void> struct has_rtl_frame_width : std::false_type
{
};
template <typename T>
struct has_rtl_frame_width<T, std::void_t<decltype(T::is_rtl_capable_v)>>
    : std::bool_constant<T::is_rtl_capable_v>
{
};

template <typename T> concept ManagedFontCache = uses_managed_cache<T>::value;
template <typename T> concept HWAccelerated = supports_hw_acceleration<T>::value;
template <typename T> concept StrictlyCulled = needs_strict_culling<T>::value;
template <typename T> concept LogicalRecorder = is_logical_recorder<T>::value;
template <typename T> concept PhysicalDevice = !is_logical_recorder<T>::value;
template <typename T> concept AlphaCapable = supports_native_alpha<T>::value;
template <typename T> concept BandedPrinting = requires_banding<T>::value;
template <typename T> concept SubsamplingCapable = supports_subsampling<T>::value;
template <typename T> concept Animatable = supports_animation<T>::value;
template <typename T> concept ScreenCompatible = is_screen_compatible<T>::value;
template <typename T> concept HighContrastOutput = is_high_contrast<T>::value;
template <typename T> concept AvoidsOverdraw = avoids_overdraw<T>::value;
template <typename T> concept PaintEventCapable = supports_paint_events<T>::value;
template <typename T> concept ReadableRasterDevice = is_readable_raster<T>::value;
template <typename T> concept SubtractiveColorDevice = is_subtractive<T>::value;
template <typename T> concept ThemedDevice = is_themed<T>::value;
template <typename T> concept FramedDevice = is_framed<T>::value;
template <typename T> concept GlyphSynthesisCapable = supports_glyph_synthesis<T>::value;
template <typename T> concept AutoMirroringCapable = allows_auto_mirroring<T>::value;
template <typename T> concept DoubleBuffered = supports_double_buffering<T>::value;
template <typename T> concept ViewportClamped = requires_viewport_clipping<T>::value;
template <typename T> concept NativeWidgetCapable = supports_native_widgets<T>::value;
template <typename T> concept FlushableDevice = supports_flush<T>::value;
template <typename T> concept RTLCapableDevice = has_rtl_frame_width<T>::value;

/**
 * @brief PageDevice
 * Combines the tag detection with both logical and physical
 * coordinate method requirements.
 */
template <typename T> concept PageDevice = has_page_offset<T>::value&& requires(const T& rDev)
{
    {
        rDev.GetPageOffset()
    }
    ->std::convertible_to<Point>;
    {
        rDev.GetPageOffsetPixel()
    }
    ->std::convertible_to<Point>;
    {
        rDev.GetPaperSizePixel()
    }
    ->std::convertible_to<Size>;
};

// --- Reference Dimensions ---

template <typename T> struct device_reference_width
{
    static tools::Long get(const T& rDevice);
};

template <typename T> inline tools::Long get_reference_width_v(const T& rDevice)
{
    return device_reference_width<T>::get(rDevice);
}

template <typename T> concept ReferenceDevice = requires(const T& rDevice)
{
    {
        vcl::get_reference_width_v(rDevice)
    }
    ->std::convertible_to<tools::Long>;
};

} // namespace vcl
