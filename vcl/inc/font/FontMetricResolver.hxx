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

#include <font/LogicalFontInstance.hxx>

namespace vcl::font
{
/**
 * A lightweight snapshot of the physical rendering capabilities of a device,
 * passed to the resolver to dictate how typography should be synthesized.
 */
struct DeviceFontCapabilities
{
    bool bSupportsGlyphSynthesis = true;
    // (We can easily add more physical constraints here later, like DPI or Color Depth)
};

class VCL_DLLPUBLIC FontMetricResolver
{
public:
    static void ResolveMetrics(const DeviceFontCapabilities& rCaps,
                               LogicalFontInstance* pFontInstance);

private:
    static void ResolveOrientation(const DeviceFontCapabilities& rCaps,
                                   LogicalFontInstance* pFontInstance);
};

} // namespace vcl::font

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
