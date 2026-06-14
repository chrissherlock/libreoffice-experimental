
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/region.hxx>

#include <clipping/ClipCompiler.hxx>

namespace vcl::clipping
{
ClipPlan ClipCompiler::Compile(const ClipState& rState)
{
    vcl::Region aFinalRegion(rState.maBounds);

    if (rState.maCustomRegion)
        aFinalRegion.Intersect(*rState.maCustomRegion);

    if (rState.bClipChildren)
    {
        for (const auto& rChild : rState.maChildren)
        {
            aFinalRegion.Exclude(rChild.maBounds);
        }
    }

    if (rState.bClipSiblings)
    {
        for (const auto& rSibling : rState.maSiblings)
        {
            aFinalRegion.Exclude(rSibling.maBounds);
        }
    }

    return ClipPlan{ std::move(aFinalRegion) };
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
