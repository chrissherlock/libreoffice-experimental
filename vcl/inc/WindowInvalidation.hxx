/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <o3tl/typed_flags_set.hxx>

#include <vcl/dllapi.h>
#include <vcl/region.hxx>
#include <vcl/wintypes.hxx>
#include <vcl/window.hxx>

enum class ImplPaintFlags
{
    NONE = 0x0000,
    Paint = 0x0001,
    PaintAll = 0x0002,
    PaintAllChildren = 0x0004,
    PaintChildren = 0x0008,
    Erase = 0x0010,
    CheckRtl = 0x0020,
};
namespace o3tl
{
template <> struct typed_flags<ImplPaintFlags> : is_typed_flags<ImplPaintFlags, 0x003f>
{
};
}

struct WindowInvalidation
{
public:
    WindowInvalidation();
    ~WindowInvalidation();

    bool isUpdateSuppressed() const { return mbNoUpdate; }
    void suppressUpdates(bool bSuppress = true) { mbNoUpdate = bSuppress; }

    bool isChildTransparent() const { return mbChildTransparent; }
    void setChildTransparent(bool bEnable = true) { mbChildTransparent = bEnable; }

    bool isPaintTransparent() const { return mbPaintTransparent; }
    void makePaintTransparent() { mbPaintTransparent = true; }
    void makePaintOpaque() { mbPaintTransparent = false; }

    bool isInPaint() const { return mbInPaint; }
    void setInPaint(bool bInPaint) { mbInPaint = bInPaint; }

    bool isPaintFrame() const { return mbPaintFrame; }
    void setPaintFrame(bool bPaint = true) { mbPaintFrame = bPaint; }

    bool isPaintDisabled() const { return mbPaintDisabled; }
    void enablePaint() { mbPaintDisabled = false; }
    void disablePaint() { mbPaintDisabled = true; }

    bool isParentUpdateSuppressed() const { return mbNoParentUpdate; }
    void suppressParentUpdates(bool bSuppress = true) { mbNoParentUpdate = bSuppress; }

    bool isDoubleBufferingRequested() const { return mbDoubleBufferingRequested; }
    void requestDoubleBuffering(bool bRequest = true) { mbDoubleBufferingRequested = bRequest; }

    bool hasPaintRegion() const { return mpPaintRegion != nullptr; }
    vcl::Region* getPaintRegion() { return mpPaintRegion; }
    const vcl::Region* getPaintRegion() const { return mpPaintRegion; }
    void setPaintRegion(vcl::Region* pRegion) { mpPaintRegion = pRegion; }
    void resetPaintRegion() { mpPaintRegion = nullptr; }

    void expandPaintRegion(const vcl::Region& rRegion)
    {
        if (mpPaintRegion)
            mpPaintRegion->Union(rRegion);
    }

    bool isDrawSelectionBackground() const { return mbDrawSelectionBackground; }

    const vcl::Region& getInvalidateRegion() const { return maInvalidateRegion; }
    void invalidate(const vcl::Region* pRegion, InvalidateFlags nFlags);
    void validateRegion(const vcl::Region* pRegion, const tools::Rectangle& rOutputRectPixel);
    vcl::Region determineChildInvalidateRegion(const tools::Rectangle& rOutputRectPixel) const;
    void clearInvalidateRegion() { maInvalidateRegion.SetEmpty(); }
    void excludeInvalidateRegion(const vcl::Region& rRegion)
    {
        maInvalidateRegion.Exclude(rRegion);
    }
    void clearPaintAll() { mnPaintFlags &= ~ImplPaintFlags::PaintAll; }

    ImplPaintFlags accumulatePaintFlags(ImplPaintFlags nIncomingFlags, bool bHasChildren);

    static constexpr InvalidateFlags LOK_INVALIDATE_FLAGS
        = InvalidateFlags::NoChildren | InvalidateFlags::NoErase | InvalidateFlags::NoTransparent
          | InvalidateFlags::NoClipChildren;

    ImplPaintFlags getPaintFlags() const { return mnPaintFlags; }
    bool isPaintNeeded() const { return bool(mnPaintFlags & ImplPaintFlags::Paint); }
    bool isPartialPaintNeeded() const;
    bool shouldPaintAll() const { return bool(mnPaintFlags & ImplPaintFlags::PaintAll); }
    bool shouldPaintAllChildren() const
    {
        return bool(mnPaintFlags & ImplPaintFlags::PaintAllChildren);
    }
    bool shouldPaintChildren() const { return bool(mnPaintFlags & ImplPaintFlags::PaintChildren); }
    bool shouldPaintAnyChildren() const
    {
        return bool(mnPaintFlags
                    & (ImplPaintFlags::PaintChildren | ImplPaintFlags::PaintAllChildren));
    }

    bool hasPendingPaint() const
    {
        return bool(mnPaintFlags & (ImplPaintFlags::Paint | ImplPaintFlags::PaintChildren));
    }
    void clearPaintFlags() { mnPaintFlags = ImplPaintFlags::NONE; }
    void addPaintFlags(ImplPaintFlags nFlags) { mnPaintFlags |= nFlags; }
    void scrollInvalidateRegion(const tools::Rectangle& rRect, tools::Long nHorzScroll,
                                tools::Long nVertScroll);
    void moveInvalidateRegion(const tools::Rectangle& rRect, tools::Long nHorzScroll,
                              tools::Long nVertScroll);
    bool accumulatePaintAllRegion(vcl::Region& rPaintAllRegion) const;

    bool hasInvalidateRegion() const { return !maInvalidateRegion.IsEmpty(); }
    void setCheckRtl() { mnPaintFlags |= ImplPaintFlags::CheckRtl; }
    void setInvalidateRegion(const vcl::Region& rRegion) { maInvalidateRegion = rRegion; }
    void unionInvalidateRegion(const vcl::Region& rRegion) { maInvalidateRegion.Union(rRegion); }
    void unionInvalidateRegion(const tools::Rectangle& rRect) { maInvalidateRegion.Union(rRect); }
    void intersectInvalidateRegion(const vcl::Region& rRegion)
    {
        maInvalidateRegion.Intersect(rRegion);
    }

    void setDrawSelectionBackground(bool bDraw = true) { mbDrawSelectionBackground = bDraw; }

    void setPaintTransparent(bool bTransparent) { mbPaintTransparent = bTransparent; }

private:
    vcl::Region maInvalidateRegion; // region that has to be redrawn (frame coordinates)
    vcl::Region* mpPaintRegion; // only set during Paint() method call (window coordinates)

    ImplPaintFlags mnPaintFlags = ImplPaintFlags::NONE; // Flags for ImplCallPaint

    bool mbInPaint : 1 = false;
    bool mbPaintFrame : 1 = false;
    bool mbPaintDisabled : 1 = false;
    bool mbPaintTransparent : 1 = false;

    bool mbNoUpdate : 1 = false;
    bool mbNoParentUpdate : 1 = false;
    bool mbChildTransparent : 1 = false;
    bool mbDoubleBufferingRequested : 1 = false;
    bool mbDrawSelectionBackground : 1 = false;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
