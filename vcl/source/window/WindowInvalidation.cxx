/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <WindowInvalidation.hxx>

#include <stdlib.h>

WindowInvalidation::WindowInvalidation()
    : mpPaintRegion(nullptr)
{
    static bool bDoubleBuffer = getenv("VCL_DOUBLEBUFFERING_FORCE_ENABLE");
    mbDoubleBufferingRequested
        = bDoubleBuffer; // when we are not sure, assume it cannot do double-buffering via RenderContext
}

WindowInvalidation::~WindowInvalidation() = default;

ImplPaintFlags WindowInvalidation::accumulatePaintFlags(ImplPaintFlags nIncomingFlags,
                                                        bool bHasChildren)
{
    mbPaintFrame = false;

    if (nIncomingFlags & ImplPaintFlags::PaintAllChildren)
        mnPaintFlags |= ImplPaintFlags::Paint | ImplPaintFlags::PaintAllChildren
                        | (nIncomingFlags & ImplPaintFlags::PaintAll);

    if (nIncomingFlags & ImplPaintFlags::PaintChildren)
        mnPaintFlags |= ImplPaintFlags::PaintChildren;

    if (nIncomingFlags & ImplPaintFlags::Erase)
        mnPaintFlags |= ImplPaintFlags::Erase;

    if (nIncomingFlags & ImplPaintFlags::CheckRtl)
        mnPaintFlags |= ImplPaintFlags::CheckRtl;

    if (!bHasChildren)
        mnPaintFlags &= ~ImplPaintFlags::PaintAllChildren;

    return mnPaintFlags & ~ImplPaintFlags::Paint;
}

void WindowInvalidation::invalidate(const vcl::Region* pRegion, InvalidateFlags nFlags)
{
    mnPaintFlags |= ImplPaintFlags::Paint;

    if (nFlags & InvalidateFlags::Children)
        mnPaintFlags |= ImplPaintFlags::PaintAllChildren;

    if (!(nFlags & InvalidateFlags::NoErase))
        mnPaintFlags |= ImplPaintFlags::Erase;

    if (!pRegion)
    {
        mnPaintFlags |= ImplPaintFlags::PaintAll;
    }
    else if (!shouldPaintAll())
    {
        // if not everything has to be redrawn, add the region to it
        maInvalidateRegion.Union(*pRegion);
    }
}

bool WindowInvalidation::isPartialPaintNeeded() const
{
    return (mnPaintFlags & (ImplPaintFlags::Paint | ImplPaintFlags::PaintAll))
           == ImplPaintFlags::Paint;
}

void WindowInvalidation::scrollInvalidateRegion(const tools::Rectangle& rRect,
                                                tools::Long nHorzScroll, tools::Long nVertScroll)
{
    vcl::Region aTempRegion = maInvalidateRegion;
    aTempRegion.Intersect(rRect);
    aTempRegion.Move(nHorzScroll, nVertScroll);
    maInvalidateRegion.Union(aTempRegion);
}

void WindowInvalidation::moveInvalidateRegion(const tools::Rectangle& rRect,
                                              tools::Long nHorzScroll, tools::Long nVertScroll)
{
    if (isPartialPaintNeeded())
    {
        vcl::Region aTempRegion = maInvalidateRegion;
        aTempRegion.Intersect(rRect);
        aTempRegion.Move(nHorzScroll, nVertScroll);
        maInvalidateRegion.Union(aTempRegion);
    }
}

/**
 * Accumulates child invalidate regions from this window.
 * Returns false if painting all (signaling that accumulation should stop),
 * or true to continue traversal.
 */
bool WindowInvalidation::accumulatePaintAllRegion(vcl::Region& rPaintAllRegion) const
{
    if (!shouldPaintAllChildren())
        return true;

    if (shouldPaintAll())
    {
        rPaintAllRegion.SetEmpty();
        return false;
    }

    rPaintAllRegion.Union(maInvalidateRegion);

    return true;
}

vcl::Region
WindowInvalidation::determineChildInvalidateRegion(const tools::Rectangle& rOutputRectPixel) const
{
    if (shouldPaintAll())
        return vcl::Region(rOutputRectPixel);

    return maInvalidateRegion;
}

void WindowInvalidation::validateRegion(const vcl::Region* pRegion,
                                        const tools::Rectangle& rOutputRectPixel)
{
    if (!pRegion)
    {
        clearInvalidateRegion();
        return;
    }

    if (shouldPaintAll())
        maInvalidateRegion = rOutputRectPixel;

    maInvalidateRegion.Exclude(*pRegion);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
