/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <memory>

// Forward declarations
class SalLayout;
class SalLayoutGlyphs;

namespace vcl::text
{
struct LayoutResources;
class TextLayoutRequest;

/**
 * Interface for font fallback handling.
 * Decouples the "Layout Engine" from the "Font Selection Strategy".
 */
class VCL_DLLPUBLIC IFontFallbackStrategy
{
public:
    virtual ~IFontFallbackStrategy() = default;

    /**
     * Resolves missing glyphs in a layout by finding fallback fonts and
     * merging their layouts into the result.
     * * @param rRes      Resources (Font Cache, Graphics, etc.)
     * @param pLayout   The current (incomplete) layout
     * @param rArgs     The layout request details
     * @param pGlyphs   Existing glyph cache (optional optimization)
     * @return          A complete layout with fallbacks applied
     */
    virtual std::unique_ptr<SalLayout>
    ResolveFallbacks(const LayoutResources& rRes, std::unique_ptr<SalLayout> pLayout,
                     vcl::text::TextLayoutRequest& rArgs, const SalLayoutGlyphs* pGlyphs)
        = 0;
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
