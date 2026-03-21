/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <font/FontMetricResolver.hxx>

namespace vcl::font
{
void FontMetricResolver::ResolveMetrics(const DeviceFontCapabilities& rCaps,
                                        LogicalFontInstance* pFontInstance)
{
    // We only resolve the hardware-dependent synthetic typography here.
    // Standard metrics are handled natively by FontMetricEngine.
    ResolveOrientation(rCaps, pFontInstance);
}

void FontMetricResolver::ResolveOrientation(const DeviceFontCapabilities& rCaps,
                                            LogicalFontInstance* pFontInstance)
{
    if (rCaps.bSupportsGlyphSynthesis)
    {
        if (pFontInstance->GetFontSelectPattern().mnOrientation
            && !pFontInstance->mxFontMetric->GetOrientation())
        {
            pFontInstance->mnOwnOrientation = pFontInstance->GetFontSelectPattern().mnOrientation;
            pFontInstance->mnOrientation = pFontInstance->mnOwnOrientation;
        }
        else
        {
            pFontInstance->mnOrientation = pFontInstance->mxFontMetric->GetOrientation();
        }
    }
    else
    {
        // Strict hardware fonts: blindly trust the native metric
        pFontInstance->mnOrientation = pFontInstance->mxFontMetric->GetOrientation();
    }
}
} // namespace vcl::font

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
