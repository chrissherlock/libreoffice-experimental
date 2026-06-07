/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/log.hxx>

#include <vcl/TransformRouter.hxx>

#include <CoordinateMath.hxx>
#include <TransformCompiler.hxx>

#include <cstdlib>

namespace vcl
{
const TransformPlan& TransformRouter::Compile(const CoordinateState& rState,
                                              const vcl::MappingPolicy ePolicy) const
{
    size_t nStateHash = rState.GetHash();

    if (const TransformPlan* pCached = maCache.Get(nStateHash, ePolicy))
        return *pCached;

    basegfx::B2DHomMatrix aMat = BuildMatrix(rState, ePolicy);
    TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    maCache.Store(nStateHash, ePolicy, aPlan);

    // Return the reference FROM THE CACHE, not the local aPlan
    // Note: It is safe to dereference here because we just stored it.
    return *maCache.Get(nStateHash, ePolicy);
}

basegfx::B2DHomMatrix TransformRouter::BuildMatrix(const CoordinateState& rState,
                                                   vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DHomMatrix aMat;

    if (ePolicy == vcl::MappingPolicy::ApplyMapMode)
    {
        // Safe logical scaling leveraging both Physical DPI and UI scaling percentage
        const double fUiScale = static_cast<double>(rState.GetDPIScalePercentage()) / 100.0;
        const double fScaleX
            = rState.GetMapRes().mfScaleX * static_cast<double>(rState.GetDPIX()) * fUiScale;
        const double fScaleY
            = rState.GetMapRes().mfScaleY * static_cast<double>(rState.GetDPIY()) * fUiScale;

        aMat = vcl::BuildAffineMatrix(fScaleX, fScaleY,
                                      static_cast<double>(rState.GetMapRes().mnTranslationX
                                                          + rState.GetLogicToAbsoluteOffsetX()),
                                      static_cast<double>(rState.GetMapRes().mnTranslationY
                                                          + rState.GetLogicToAbsoluteOffsetY()),
                                      static_cast<double>(rState.GetWindowToViewOffsetX()),
                                      static_cast<double>(rState.GetWindowToViewOffsetY()));
    }
    else
    {
        // CRITICAL FIX: IgnoreMapMode means purely unscaled pixel-to-pixel mapping.
        // We must strictly return to purely shifting the unmapped viewport offset.
        aMat.translate(static_cast<double>(rState.GetWindowToViewOffsetX()),
                       static_cast<double>(rState.GetWindowToViewOffsetY()));
    }

    return aMat;
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
