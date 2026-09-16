/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <config_features.h>

#include <comphelper/lok.hxx>
#include <comphelper/profilezone.hxx>

#include <vcl/CoordinateMapper.hxx>
#include <vcl/cursor.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/syswin.hxx>
#include <vcl/vclevent.hxx>

#if HAVE_FEATURE_OPENGL
#include <vcl/opengl/OpenGLHelper.hxx>
#endif

#include <window/state/ImplFrameData.hxx>
#include <window/state/ImplWinData.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowControlAppearance.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowInvalidation.hxx>
#include <window/state/WindowFocusState.hxx>
#include <clipping/ClippingBridge.hxx>
#include <clipping/ClipStateBuilder.hxx>
#include <window/state/clipping_window.hxx>
#include <salframe.hxx>
#include <svdata.hxx>

#include "window/paint/PaintHelper.hxx"

namespace vcl
{

static tools::Rectangle lcl_SetupPaintRegion(PaintHelper& rHelper,
                                             WindowInvalidation& rInvalidation,
                                             OutputDevice& rOutDev,
                                             vcl::RenderContext& rRenderContext)
{
    rOutDev.GetClipState().Invalidate();
    rInvalidation.setInPaint(true);

    vcl::Region& rPaintRegion = rHelper.GetPaintRegion();
    rPaintRegion = rInvalidation.getInvalidateRegion();
    tools::Rectangle aPaintDeviceRect = rPaintRegion.GetBoundRect();

    if (rOutDev.ImplIsAntiparallel())
    {
        rRenderContext.ReMirror(aPaintDeviceRect);
        rRenderContext.ReMirror(rPaintRegion);
    }

    rInvalidation.setPaintRegion(&rPaintRegion);
    rInvalidation.clearInvalidateRegion();

    return rOutDev.convertTo<vcl::LogicRect>(vcl::DeviceRect(aPaintDeviceRect)).get();
}

static void lcl_EraseBackground(vcl::Window& rWindow,
                                const PaintHelper& rHelper,
                                vcl::RenderContext& rRenderContext)
{
    if (!(rHelper.GetPaintFlags() & ImplPaintFlags::Erase) || !rRenderContext.HasBackground())
        return;

    if (!rRenderContext.IsClipRegion())
    {
        rWindow.Erase(rRenderContext);
        return;
    }

    const vcl::Region aOldRegion = rRenderContext.GetClipRegion();
    rRenderContext.SetClipRegion();
    rWindow.Erase(rRenderContext);
    rRenderContext.SetClipRegion(aOldRegion);
}

void Window::PushPaintHelper(PaintHelper *pHelper, vcl::RenderContext& rRenderContext)
{
    pHelper->SetPop();

    if (mpControlAppearance->getCursor())
        pHelper->SetRestoreCursor(mpControlAppearance->suspendCursor());

    const tools::Rectangle aPaintRect = lcl_SetupPaintRegion(
        *pHelper, *mpInvalidation, *GetOutDev(), rRenderContext);

    lcl_EraseBackground(*this, *pHelper, rRenderContext);

    // #98943# trigger drawing of toolbox selection after all children are painted
    if (mpInvalidation->isDrawSelectionBackground())
        pHelper->SetSelectionRect(aPaintRect);

    pHelper->SetPaintRect(aPaintRect);
}

void Window::PopPaintHelper(PaintHelper const *pHelper)
{
    if (mpWinData && mpFocusState->isFocusVisible())
        ImplInvertFocus(*mpWinData->mpFocusRect);

    mpInvalidation->setInPaint(false);
    GetOutDev()->GetClipState().Invalidate();
    mpInvalidation->resetPaintRegion();

    if (mpControlAppearance->getCursor())
        mpControlAppearance->resumeCursor(pHelper->GetRestoreCursor());
}

void Window::ImplCallPaint(const vcl::Region* pRegion, ImplPaintFlags nPaintFlags)
{
    // call PrePaint. PrePaint may add to the invalidate region as well as
    // other parameters used below.
    PrePaint(*GetOutDev());

    const ImplPaintFlags nFlagsNoPaint = mpInvalidation->accumulatePaintFlags(nPaintFlags, mpHierarchy->hasChildren());

    // If tiled rendering is used, windows are only invalidated, never painted to.
    if (mpInvalidation->isPaintDisabled() || comphelper::LibreOfficeKit::isActive())
    {
        if (mpInvalidation->shouldPaintAll())
            Invalidate(WindowInvalidation::LOK_INVALIDATE_FLAGS);
        else if (pRegion)
            Invalidate(*pRegion, WindowInvalidation::LOK_INVALIDATE_FLAGS);

        PostPaint(*GetOutDev());

        return;
    }

    PaintHelper aHelper(this, nFlagsNoPaint);

    if (mpInvalidation->isPaintNeeded())
        aHelper.DoPaint(pRegion);
    else
        mpInvalidation->clearPaintFlags();

    PostPaint(*GetOutDev());
}

void Window::ImplCallOverlapPaint()
{
    if (!mpClassification)
        return;

    // Emit overlapping windows first
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [](vcl::Window& rOverlapWin) {
            const WindowVisibilityState* pVisibility = rOverlapWin.ImplGetVisibilityState();
            if (pVisibility && pVisibility->mbReallyVisible)
                rOverlapWin.ImplCallOverlapPaint();
        });
    }

    // Only then ourself
    if (mpInvalidation && mpInvalidation->hasPendingPaint())
    {
        // RTL: notify ImplCallPaint to check for re-mirroring
        // because we were called from the Sal layer
        ImplCallPaint(nullptr, mpInvalidation->getPaintFlags());
    }
}

IMPL_LINK_NOARG(Window, ImplHandlePaintHdl, Timer *, void)
{
    comphelper::ProfileZone aZone("VCL idle re-paint");

    // save paint events until layout is done
    if (IsSystemWindow() && static_cast<const SystemWindow*>(this)->hasPendingLayout())
    {
        mpPlatformState->mpFrameData->maPaintIdle.Start();
        return;
    }

    // save paint events until resizing or initial sizing done
    if (mpClassification->mbFrame &&
        mpPlatformState->mpFrameData->maResizeIdle.IsActive())
    {
        mpPlatformState->mpFrameData->maPaintIdle.Start();
    }
    else if ( mpVisibilityState->mbReallyVisible )
    {
        ImplCallOverlapPaint();
        if (comphelper::LibreOfficeKit::isActive() &&
            mpPlatformState->mpFrameData->maPaintIdle.IsActive())
            mpPlatformState->mpFrameData->maPaintIdle.Stop();
    }
}

IMPL_LINK_NOARG(Window, ImplHandleResizeTimerHdl, Timer *, void)
{
    comphelper::ProfileZone aZone("VCL idle resize");

    if( mpVisibilityState->mbReallyVisible )
    {
        ImplCallResize();
        if( mpPlatformState->mpFrameData->maPaintIdle.IsActive() )
        {
            mpPlatformState->mpFrameData->maPaintIdle.Stop();
            mpPlatformState->mpFrameData->maPaintIdle.Invoke( nullptr );
        }
    }
}

static void lcl_PropagatePaintChildrenToOverlap(vcl::Window& rWindow)
{
    if (rWindow.ImplIsOverlapWindow())
        return;

    vcl::Window* pTempWindow = &rWindow;
    ImplPaintFlags nTranspPaint = rWindow.IsPaintTransparent() ? ImplPaintFlags::Paint : ImplPaintFlags::NONE;

    do
    {
        pTempWindow = pTempWindow->ImplGetParent();
        WindowInvalidation* pInvalidation = pTempWindow->ImplGetWindowInvalidation();
        if (pInvalidation->shouldPaintChildren())
            break;

        pInvalidation->addPaintFlags(ImplPaintFlags::PaintChildren | nTranspPaint);
        if (!pTempWindow->IsPaintTransparent())
            nTranspPaint = ImplPaintFlags::NONE;
    }
    while (!pTempWindow->ImplIsOverlapWindow());
}

static void lcl_InvalidateOpaqueParent(vcl::Window& rWindow,
                                      const WindowInvalidation& rInvalidation,
                                      InvalidateFlags nFlags)
{
    const bool bNeedsTransparent = (rWindow.IsPaintTransparent() && !(nFlags & InvalidateFlags::NoTransparent))
                                || (nFlags & InvalidateFlags::Transparent);

    if (!bNeedsTransparent || !rWindow.ImplGetParent())
        return;

    vcl::Window* pParent = rWindow.ImplGetParent();
    while (pParent && pParent->IsPaintTransparent())
    {
        pParent = pParent->ImplGetParent();
    }

    if (!pParent)
        return;

    const vcl::Region* pChildRegion = rInvalidation.shouldPaintAll()
        ? &rWindow.ImplGetClippingState()->getWinChildClipRegion(rWindow)
        : &rInvalidation.getInvalidateRegion();

    nFlags |= InvalidateFlags::Children; // paint should also be done on all children
    nFlags &= ~InvalidateFlags::NoErase; // parent should paint and erase to create proper background

    pParent->ImplInvalidateFrameRegion(pChildRegion, nFlags);
}

void Window::ImplInvalidateFrameRegion( const vcl::Region* pRegion, InvalidateFlags nFlags )
{
    // set PAINTCHILDREN for all parent windows till the first OverlapWindow
    lcl_PropagatePaintChildrenToOverlap(*this);

    // set Paint-Flags and update invalidate region
    mpInvalidation->invalidate(pRegion, nFlags);

    // Invalidate transparent regions on the first opaque parent
    lcl_InvalidateOpaqueParent(*this, *mpInvalidation, nFlags);

    if (!mpPlatformState->mpFrameData->maPaintIdle.IsActive())
        mpPlatformState->mpFrameData->maPaintIdle.Start();
}

void Window::ImplInvalidateOverlapFrameRegion(const vcl::Region& rRegion)
{
    vcl::Region aRegion = rRegion;

    vcl::clipping::clipBoundaries(*this, aRegion, true, true);
    if (!aRegion.IsEmpty())
        ImplInvalidateFrameRegion(&aRegion, InvalidateFlags::Children);

    // Invalidate the overlapping windows
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [&rRegion](vcl::Window& rOverlapWin) {
            if (rOverlapWin.IsVisible())
                rOverlapWin.ImplInvalidateOverlapFrameRegion(rRegion);
        });
    }
}

void Window::ImplInvalidateParentFrameRegion(const vcl::Region& rRegion)
{
    if (!mpClassification)
        return;

    if (mpClassification->mbOverlapWin)
    {
        if (vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr)
            pFrameWindow->ImplInvalidateOverlapFrameRegion(rRegion);
    }
    else if (vcl::Window* pParent = ImplGetParent())
    {
        pParent->ImplInvalidateFrameRegion(&rRegion, InvalidateFlags::Children);
    }
}

static vcl::Window* lcl_FindOpaqueTarget(vcl::Window& rWindow,
                                         const WindowInvalidation& rInvalidation,
                                         InvalidateFlags nFlags)
{
    const bool bNeedsTransparent = (rInvalidation.isPaintTransparent() && !(nFlags & InvalidateFlags::NoTransparent))
                                || (nFlags & InvalidateFlags::Transparent);

    if (!bNeedsTransparent)
        return &rWindow;

    vcl::Window* pTempWindow = rWindow.ImplGetParent();
    while (pTempWindow)
    {
        if (!pTempWindow->IsPaintTransparent())
            return pTempWindow;

        if (pTempWindow->ImplIsOverlapWindow())
            break;

        pTempWindow = pTempWindow->ImplGetParent();
    }

    return &rWindow;
}

static vcl::Region lcl_CalculateBaseInvalidateRegion(vcl::Window& rWindow, const vcl::Region* pRegion)
{
    vcl::Region aRegion(rWindow.GetOutputRectPixel());

    if (pRegion)
    {
        // RTL: remirror region before intersecting it
        const OutputDevice* pOutDev = rWindow.GetOutDev();
        if (pOutDev->ImplIsAntiparallel())
        {
            vcl::Region aRgn(*pRegion);
            pOutDev->ReMirror(aRgn);
            aRegion.Intersect(aRgn);
        }
        else
        {
            aRegion.Intersect(*pRegion);
        }
    }

    vcl::clipping::clipBoundaries(rWindow, aRegion, true, true);
    return aRegion;
}

static std::tuple<vcl::Region, InvalidateFlags> lcl_ClipInvalidateRegion(
    vcl::Window& rWindow,
    const vcl::Region* pRegion,
    InvalidateFlags nFlags,
    bool bExplicitNoChildren)
{
    vcl::Region aRegion = lcl_CalculateBaseInvalidateRegion(rWindow, pRegion);

    if (nFlags & InvalidateFlags::NoChildren)
    {
        nFlags &= ~InvalidateFlags::Children;

        if (!(nFlags & InvalidateFlags::NoClipChildren))
        {
            if (bExplicitNoChildren)
                vcl::clipping::clipAllChildren(rWindow, aRegion);
            else if (vcl::clipping::clipChildren(rWindow, aRegion))
                nFlags |= InvalidateFlags::Children;
        }
    }

    return { aRegion, nFlags };
}

void Window::ImplInvalidate( const vcl::Region* pRegion, InvalidateFlags nFlags )
{
    bool bInvalidateAll = !pRegion;

    // take Transparent-Invalidate into account
    vcl::Window* pOpaqueWindow = lcl_FindOpaqueTarget(*this, *mpInvalidation, nFlags);
    if (pOpaqueWindow != this)
    {
        nFlags |= InvalidateFlags::Children;
        bInvalidateAll = false;
    }

    // Capture caller's explicit intent before we apply window styles
    const bool bExplicitNoChildren = bool(nFlags & InvalidateFlags::NoChildren);

    // apply implicit flags if neither was specified
    if (!(nFlags & (InvalidateFlags::Children | InvalidateFlags::NoChildren)))
        nFlags |= (GetStyle() & WB_CLIPCHILDREN) ? InvalidateFlags::NoChildren : InvalidateFlags::Children;

    if ((nFlags & InvalidateFlags::NoChildren) && mpHierarchy->hasChildren())
        bInvalidateAll = false;

    if (bInvalidateAll)
    {
        ImplInvalidateFrameRegion(nullptr, nFlags);
    }
    else
    {
        auto [aRegion, nAdjustedFlags] = lcl_ClipInvalidateRegion(*this, pRegion, nFlags, bExplicitNoChildren);

        if (!aRegion.IsEmpty())
            ImplInvalidateFrameRegion(&aRegion, nAdjustedFlags); // transparency is handled here, pOpaqueWindow not required
    }

    if (nFlags & InvalidateFlags::Update)
        pOpaqueWindow->PaintImmediately();        // start painting at the opaque parent
}

void Window::ImplMoveInvalidateRegion( const tools::Rectangle& rRect,
                                       tools::Long nHorzScroll, tools::Long nVertScroll,
                                       bool bChildren )
{
    mpInvalidation->moveInvalidateRegion( rRect, nHorzScroll, nVertScroll );

    if (!bChildren || !mpInvalidation->shouldPaintChildren())
        return;

    vcl::Window* pWindow = mpHierarchy->getFirstChild();
    while ( pWindow )
    {
        pWindow->ImplMoveInvalidateRegion( rRect, nHorzScroll, nVertScroll, true );
        pWindow = pWindow->ImplGetWindowHierarchy()->getNextSibling();
    }
}

void Window::ImplMoveAllInvalidateRegions( const tools::Rectangle& rRect,
                                          tools::Long nHorzScroll, tools::Long nVertScroll,
                                          bool bChildren )
{
    // also shift Paint-Region when paints need processing
    ImplMoveInvalidateRegion(rRect, nHorzScroll, nVertScroll, bChildren);

    // Paint-Region should be shifted, as drawn by the parents
    if (ImplIsOverlapWindow())
        return;

    vcl::Region aPaintAllRegion;
    vcl::Window* pPaintAllWindow = this;
    do
    {
        pPaintAllWindow = pPaintAllWindow->ImplGetParent();
        const WindowInvalidation* pInvalidation = pPaintAllWindow->ImplGetWindowInvalidation();

        if (!pInvalidation->accumulatePaintAllRegion(aPaintAllRegion))
            break;
    }
    while (!pPaintAllWindow->ImplIsOverlapWindow());

    if ( !aPaintAllRegion.IsEmpty() )
    {
        aPaintAllRegion.Move( nHorzScroll, nVertScroll );
        InvalidateFlags nPaintFlags = InvalidateFlags::NONE;

        if ( bChildren )
            nPaintFlags |= InvalidateFlags::Children;

        ImplInvalidateFrameRegion( &aPaintAllRegion, nPaintFlags );
    }
}

void Window::ImplValidateFrameRegion( const vcl::Region* pRegion, ValidateFlags nFlags )
{
    if (pRegion && mpInvalidation->shouldPaintAllChildren() && mpHierarchy->hasChildren())
    {
        const vcl::Region aChildRegion = mpInvalidation->determineChildInvalidateRegion(GetOutputRectPixel());

        vcl::Window* pChild = mpHierarchy->getFirstChild();
        while (pChild)
        {
            pChild->Invalidate(aChildRegion, InvalidateFlags::Children | InvalidateFlags::NoTransparent);
            pChild = pChild->ImplGetWindowHierarchy()->getNextSibling();
        }
    }

    mpInvalidation->validateRegion(pRegion, GetOutputRectPixel());
    mpInvalidation->clearPaintAll();

    if (!(nFlags & ValidateFlags::Children))
        return;

    vcl::Window* pChild = mpHierarchy->getFirstChild();
    while (pChild)
    {
        pChild->ImplValidateFrameRegion(pRegion, nFlags);
        pChild = pChild->ImplGetWindowHierarchy()->getNextSibling();
    }
}

void Window::ImplValidate()
{
    // Assemble region
    ValidateFlags nFlags = (GetStyle() & WB_CLIPCHILDREN)
                               ? ValidateFlags::NoChildren
                               : ValidateFlags::Children;

    const bool bHasChildren = mpHierarchy && (mpHierarchy->getFirstChild() != nullptr);
    const bool bValidateAll = !(nFlags & ValidateFlags::NoChildren) || !bHasChildren;

    if (bValidateAll)
    {
        ImplValidateFrameRegion(nullptr, nFlags);
        return;
    }

    vcl::Region aRegion(GetOutputRectPixel());
    vcl::clipping::clipBoundaries(*this, aRegion, true, true);

    if (nFlags & ValidateFlags::NoChildren)
    {
        nFlags &= ~ValidateFlags::Children;
        if (vcl::clipping::clipChildren(*this, aRegion))
            nFlags |= ValidateFlags::Children;
    }

    if (!aRegion.IsEmpty())
        ImplValidateFrameRegion(&aRegion, nFlags);
}

void Window::ImplUpdateAll()
{
    if ( !mpVisibilityState || !mpVisibilityState->mbReallyVisible )
        return;

    bool bFlush = false;
    vcl::Window* pFrameWindow = mpHierarchy->getFrameWindow();
    if ( pFrameWindow && pFrameWindow->ImplGetWindowInvalidation()->isPaintFrame() )
    {
        const vcl::Region aRegion( tools::Rectangle( Point( 0, 0 ), GetOutputSizePixel() ) );
        ImplInvalidateOverlapFrameRegion( aRegion );

        const bool bBorderIsFrame = mpHierarchy->hasBorderWindow()
            && mpHierarchy->getBorderWindow()->ImplGetWindowClassification()->isFrame();

        if ( mpClassification->isFrame() || bBorderIsFrame )
            bFlush = true;
    }

    // an update changes the OverlapWindow, such that for later paints
    // not too much has to be drawn, if ALLCHILDREN etc. is set
    vcl::Window* pWindow = ImplGetFirstOverlapWindow();
    pWindow->ImplCallOverlapPaint();

    if ( bFlush )
        GetOutDev()->Flush();
}

void Window::PrePaint(vcl::RenderContext& /*rRenderContext*/)
{
}

void Window::PostPaint(vcl::RenderContext& /*rRenderContext*/)
{
}

void Window::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    auto aState = vcl::clipping::ClipStateBuilder::BuildFromWindow(*this);
    vcl::clipping::ApplyClipping(rRenderContext, aState);

    // VIRTUAL VERIFICATION:
    // Check if our new clipping result matches what VCL's legacy state expected.
    // If this hits, you have found a regression in your compiler logic!
    assert(rRenderContext.GetClipRegion() == vcl::clipping::ClipCompiler::Compile(aState).maFinalRegion);

    CallEventListeners(VclEventId::WindowPaint, const_cast<tools::Rectangle *>(&rRect));
}

void Window::SetPaintTransparent( bool bTransparent )
{
    // transparency is not useful for frames as the background would have to be provided by a different frame
    if ( bTransparent && mpClassification->isFrame() )
        return;

    if ( mpHierarchy->hasBorderWindow() )
        mpHierarchy->getBorderWindow()->SetPaintTransparent( bTransparent );

    mpInvalidation->setPaintTransparent( bTransparent );
}

void Window::SetWindowRegionPixel()
{
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
    {
        pBorderWindow->SetWindowRegionPixel();
        return;
    }

    if (!mpClassification || !mpClippingState)
        return;

    if (mpClassification->mbFrame)
    {
        mpClippingState->maWinRegion = vcl::Region(true);
        mpClippingState->mbWinRegion = false;
        if (mpPlatformState && mpPlatformState->mpFrame)
            mpPlatformState->mpFrame->ResetClipRegion();
        return;
    }

    if (!mpClippingState->mbWinRegion)
        return;

    mpClippingState->maWinRegion = vcl::Region(true);
    mpClippingState->mbWinRegion = false;
    vcl::clipping::setClipFlag(*this);

    if (!IsReallyVisible())
        return;

    vcl::Region aRegion(GetOutputRectPixel());
    ImplInvalidateParentFrameRegion(aRegion);
}

void Window::SetWindowRegionPixel(const vcl::Region& rRegion)
{
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
    {
        pBorderWindow->SetWindowRegionPixel(rRegion);
        return;
    }

    if (!mpClassification || !mpClippingState)
        return;

    if (mpClassification->mbFrame)
    {
        if (!rRegion.IsNull() && !rRegion.IsEmpty())
        {
            mpClippingState->maWinRegion = rRegion;
            mpClippingState->mbWinRegion = true;

            // Set/update ClipRegion
            SalFrame* pFrame = mpPlatformState ? mpPlatformState->mpFrame : nullptr;
            if (pFrame)
            {
                RectangleVector aRectangles;
                mpClippingState->maWinRegion.GetRegionRectangles(aRectangles);
                pFrame->BeginSetClipRegion(aRectangles.size());

                for (const auto& rRectangle : aRectangles)
                {
                    pFrame->UnionClipRegion(
                        rRectangle.Left(),
                        rRectangle.Top(),
                        rRectangle.GetWidth(),
                        rRectangle.GetHeight());
                }

                pFrame->EndSetClipRegion();
            }
        }
        else
        {
            SetWindowRegionPixel();
        }

        return;
    }

    if (rRegion.IsNull())
    {
        if (mpClippingState->mbWinRegion)
        {
            mpClippingState->maWinRegion = vcl::Region(true);
            mpClippingState->mbWinRegion = false;
            vcl::clipping::setClipFlag(*this);
        }
    }
    else
    {
        mpClippingState->maWinRegion = rRegion;
        mpClippingState->mbWinRegion = true;
        vcl::clipping::setClipFlag(*this);
    }

    if (!IsReallyVisible())
        return;

    vcl::Region aRegion(GetOutputRectPixel());
    ImplInvalidateParentFrameRegion(aRegion);
}

vcl::Region Window::GetPaintRegion() const
{
    if ( mpInvalidation->hasPaintRegion() )
    {
        vcl::Region aRegion = *mpInvalidation->getPaintRegion();
        aRegion.Move( -GetOutDev()->GetDeviceOriginX(), -GetOutDev()->GetDeviceOriginY() );
        return convertTo<vcl::LogicRegion>(vcl::WindowRegion(aRegion)).get();
    }

    return vcl::Region(true);
}

void Window::Invalidate( InvalidateFlags nFlags )
{
    if (!comphelper::LibreOfficeKit::isActive() && (!GetOutDev()->IsDeviceOutputNecessary() || !GetOutDev()->GetOutputWidthPixel() || !GetOutDev()->GetOutputHeightPixel()) )
        return;

    if (!mpClassification)
        return; // ImplInvalidate() would dereference mpClassification unconditionally.

    ImplInvalidate( nullptr, nFlags );
    LogicInvalidate(nullptr);
}

void Window::Invalidate( const tools::Rectangle& rRect, InvalidateFlags nFlags )
{
    if ( !comphelper::LibreOfficeKit::isActive() && (!GetOutDev()->IsDeviceOutputNecessary() || !GetOutDev()->GetOutputWidthPixel() || !GetOutDev()->GetOutputHeightPixel()) )
        return;

    OutputDevice *pOutDev = GetOutDev();
    tools::Rectangle aRect = pOutDev->GetMapper().LogicToDevicePixel(rRect, pOutDev->GetMappingPolicy());

    if (aRect.IsEmpty())
        return;

    vcl::Region aRegion( aRect );
    ImplInvalidate( &aRegion, nFlags );
    tools::Rectangle aLogicRectangle(rRect);
    LogicInvalidate(&aLogicRectangle);
}

void Window::Invalidate( const vcl::Region& rRegion, InvalidateFlags nFlags )
{
    if ( !comphelper::LibreOfficeKit::isActive() && (!GetOutDev()->IsDeviceOutputNecessary() || !GetOutDev()->GetOutputWidthPixel() || !GetOutDev()->GetOutputHeightPixel()) )
        return;

    if ( rRegion.IsNull() )
    {
        ImplInvalidate( nullptr, nFlags );
        LogicInvalidate(nullptr);
        return;
    }

    auto aRegion = convertTo<vcl::DeviceRegion>(vcl::LogicRegion(rRegion), GetMapMode());

    if (aRegion->IsEmpty())
        return;

    ImplInvalidate(&aRegion.get(), nFlags);
    tools::Rectangle aLogicRectangle = rRegion.GetBoundRect();
    LogicInvalidate(&aLogicRectangle);
}

void Window::LogicInvalidate(const tools::Rectangle* pRectangle)
{
    if (pRectangle)
    {
        const auto aRect = convertTo<vcl::DeviceRect>(vcl::LogicRect(*pRectangle), GetOutDev()->GetMapMode());
        PixelInvalidate(&aRect.get());
        return;
    }

    PixelInvalidate(nullptr);
}

bool Window::InvalidateByForeignEditView(EditView* )
{
    return false;
}

void Window::PixelInvalidate(const tools::Rectangle* pRectangle)
{
    if (comphelper::LibreOfficeKit::isDialogPainting() || !comphelper::LibreOfficeKit::isActive())
        return;

    Size aSize = GetSizePixel();
    if (aSize.IsEmpty())
        return;

    if (const vcl::ILibreOfficeKitNotifier* pNotifier = GetLOKNotifier())
    {
        // In case we are routing the window, notify the client
        std::vector<vcl::LOKPayloadItem> aPayload;
        tools::Rectangle aRect(Point(0, 0), aSize);
        if (pRectangle)
            aRect = *pRectangle;

        if (IsRTLEnabled() && GetOutDev() && !GetOutDev()->ImplIsAntiparallel())
            GetOutDev()->ReMirror(aRect);

        aPayload.emplace_back("rectangle", aRect.toString());

        pNotifier->notifyWindow(GetLOKWindowId(), u"invalidate"_ustr, aPayload);
    }
    else if (GetParent() && GetParent()->IsFormControl())
    {
        const VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier();
        if (pParent)
            pParent->GetLOKNotifier()->notifyInvalidation(pRectangle);
    }
    // Added for dialog items. Pass invalidation to the parent window.
    else if (VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier())
    {
        const tools::Rectangle aRect(Point(GetDeviceOriginX(), GetDeviceOriginY()), GetSizePixel());
        pParent->PixelInvalidate(&aRect);
    }
}

void Window::Validate()
{
    if ( !comphelper::LibreOfficeKit::isActive() && (!GetOutDev()->IsDeviceOutputNecessary() || !GetOutDev()->GetOutputWidthPixel() || !GetOutDev()->GetOutputHeightPixel()) )
        return;

    ImplValidate();
}

bool Window::HasPaintEvent() const
{
    if (!mpVisibilityState || !mpHierarchy || !mpInvalidation)
        return false;

    if (!mpVisibilityState->mbReallyVisible)
        return false;

    vcl::Window* pFrameWindow = mpHierarchy->getFrameWindow();
    if (pFrameWindow && pFrameWindow->ImplGetWindowInvalidation()->isPaintFrame())
        return true;

    if (mpInvalidation->isPaintNeeded())
        return true;

    if (ImplIsOverlapWindow())
        return false;

    const vcl::Window* pTempWindow = this;

    do
    {
        pTempWindow = pTempWindow->ImplGetParent();
        const WindowInvalidation* pInvalidation = pTempWindow->ImplGetWindowInvalidation();
        if (pInvalidation->shouldPaintAnyChildren())
            return true;
    }
    while (!pTempWindow->ImplIsOverlapWindow());

    return false;
}

void Window::PaintImmediately()
{
    if (!mpHierarchy || !mpVisibilityState)
        return;

    if ( mpHierarchy->hasBorderWindow() )
    {
        mpHierarchy->getBorderWindow()->PaintImmediately();
        return;
    }

    if ( !mpVisibilityState->mbReallyVisible )
        return;

    bool bFlush = false;
    vcl::Window* pFrameWindow = mpHierarchy->getFrameWindow();
    if ( pFrameWindow && pFrameWindow->ImplGetWindowInvalidation()->isPaintFrame() )
    {
        const vcl::Region aRegion( tools::Rectangle( Point( 0, 0 ), GetOutputSizePixel() ) );
        ImplInvalidateOverlapFrameRegion( aRegion );

        const bool bBorderIsFrame = mpHierarchy->hasBorderWindow()
            && mpHierarchy->getBorderWindow()->ImplGetWindowClassification()->isFrame();

        if ( mpClassification->isFrame() || bBorderIsFrame )
            bFlush = true;
    }

    // First we should skip all windows which are Paint-Transparent
    vcl::Window* pUpdateWindow = this;
    vcl::Window* pWindow = pUpdateWindow;
    while ( !pWindow->ImplIsOverlapWindow() )
    {
        if ( !pWindow->ImplGetWindowInvalidation()->isPaintTransparent() )
        {
            pUpdateWindow = pWindow;
            break;
        }
        pWindow = pWindow->ImplGetParent();
    }

    // In order to limit drawing, an update only draws the window which
    // has PAINTALLCHILDREN set
    pWindow = pUpdateWindow;
    do
    {
        if ( pWindow->ImplGetWindowInvalidation()->shouldPaintAllChildren() )
            pUpdateWindow = pWindow;
        if ( pWindow->ImplIsOverlapWindow() )
            break;
        pWindow = pWindow->ImplGetParent();
    }
    while ( pWindow );

    // if there is something to paint, trigger a Paint
    WindowInvalidation* pUpdateInvalidation = pUpdateWindow->ImplGetWindowInvalidation();
    if ( pUpdateInvalidation->hasPendingPaint() )
    {
        VclPtr<vcl::Window> xWindow(this);

        // trigger an update also for system windows on top of us,
        // otherwise holes would remain
        vcl::Window* pUpdateOverlapWindow = ImplGetFirstOverlapWindow();
        if (pUpdateOverlapWindow->ImplGetWindowClassification())
            pUpdateOverlapWindow = pUpdateOverlapWindow->ImplGetWindowHierarchy()->getFirstOverlap();
        else
            pUpdateOverlapWindow = nullptr;

        while ( pUpdateOverlapWindow )
        {
             pUpdateOverlapWindow->PaintImmediately();
             pUpdateOverlapWindow = pUpdateOverlapWindow->ImplGetWindowHierarchy()->getNextSibling();
        }

        pUpdateWindow->ImplCallPaint(nullptr, pUpdateInvalidation->getPaintFlags());

        if (comphelper::LibreOfficeKit::isActive() && pUpdateWindow->GetParentDialog())
            pUpdateWindow->LogicInvalidate(nullptr);

        if (xWindow->isDisposed())
           return;

        bFlush = true;
    }

    if ( bFlush )
        GetOutDev()->Flush();
}

void Window::ImplPaintToDevice(OutputDevice& rTargetOutDev, const Point& i_rPos)
{
    OutputDevice* pOutDev = GetOutDev();
    if (!pOutDev)
        return;

    SalFrame* pFrame = mpPlatformState ? mpPlatformState->mpFrame : nullptr;

    // Special drawing when called through LOKit
    // TODO: Move to its own method
    if (comphelper::LibreOfficeKit::isActive())
    {
        VclPtrInstance<VirtualDevice> pDevice(rTargetOutDev);
        pDevice->EnableRTL(IsRTLEnabled());

        Size aSize(GetOutputSizePixel());
        pDevice->SetOutputSizePixel(aSize);

        vcl::Font aCopyFont = GetFont();
        pDevice->SetFont(aCopyFont);

        pDevice->SetTextColor(GetTextColor());
        if (pOutDev->HasLineColor())
            pDevice->SetLineColor(pOutDev->GetLineColor());
        else
            pDevice->SetLineColor();

        if (pOutDev->HasFillColor())
            pDevice->SetFillColor(pOutDev->GetFillColor());
        else
            pDevice->SetFillColor();

        if (HasTextLineColor())
            pDevice->SetTextLineColor(GetTextLineColor());
        else
            pDevice->SetTextLineColor();

        if (HasOverlineColor())
            pDevice->SetOverlineColor(GetOverlineColor());
        else
            pDevice->SetOverlineColor();

        if (HasTextFillColor())
            pDevice->SetTextFillColor(GetTextFillColor());
        else
            pDevice->SetTextFillColor();

        pDevice->SetTextAlign(GetTextAlign());
        pDevice->SetRasterOp(pOutDev->GetRasterOp());

        tools::Rectangle aPaintRect(Point(), GetOutputSizePixel());

        vcl::Region aClipRegion(pOutDev->GetClipRegion());
        pDevice->SetClipRegion();
        aClipRegion.Intersect(aPaintRect);
        pDevice->SetClipRegion(aClipRegion);

        if (!IsPaintTransparent() && HasBackground() && !(vcl::clipping::getParentClipMode(*this) & ParentClipMode::NoClip))
            Erase(*pDevice);

        pDevice->SetMapMode(GetMapMode());

        Paint(*pDevice, tools::Rectangle(Point(), GetOutputSizePixel()));

        rTargetOutDev.DrawOutDev(i_rPos, aSize, Point(), pDevice->convertTo<vcl::LogicSize>(vcl::WindowSize(aSize)), *pDevice);

        const bool bHasMirroredGraphics = pDevice->HasMirroredGraphics();

        // Get rid of virtual device now so they don't pile up during recursive calls
        pDevice.disposeAndClear();

        if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
        {
            WindowHierarchy::forEachSibling(pFirstChild, [&](vcl::Window& rChild) {
                WindowPlatformState* pChildPlatform = rChild.ImplGetPlatformState();
                SalFrame* pChildFrame = pChildPlatform ? pChildPlatform->mpFrame : nullptr;

                if (pChildFrame == pFrame && rChild.IsVisible())
                {
                    OutputDevice* pChildOutDev = rChild.GetOutDev();
                    tools::Long nDeltaX = (pChildOutDev ? pChildOutDev->GetDeviceOriginX() : 0) - pOutDev->GetDeviceOriginX();
                    if (bHasMirroredGraphics)
                        nDeltaX = pOutDev->GetOutputWidthPixel() - nDeltaX - (pChildOutDev ? pChildOutDev->GetOutputWidthPixel() : 0);

                    tools::Long nDeltaY = rChild.GetDeviceOriginY() - GetDeviceOriginY();

                    Point aPos(i_rPos);
                    aPos += Point(nDeltaX, nDeltaY);

                    rChild.ImplPaintToDevice(rTargetOutDev, aPos);
                }
            });
        }
        return;
    }

    if (!mpVisibilityState)
        return;

    const bool bRVisible = mpVisibilityState->mbReallyVisible;
    mpVisibilityState->mbReallyVisible = mpVisibilityState->mbVisible;
    const bool bDevOutput = pOutDev->mbDevOutput;
    pOutDev->mbDevOutput = true;

    const tools::Long nOldDPIX = pOutDev->GetDPIX();
    const tools::Long nOldDPIY = pOutDev->GetDPIY();

    pOutDev->SetDPIX(rTargetOutDev.GetDPIX());
    pOutDev->SetDPIY(rTargetOutDev.GetDPIY());

    const bool bOutput = pOutDev->IsOutputEnabled();
    pOutDev->EnableOutput();

    SAL_WARN_IF(GetMapMode().GetMapUnit() != MapUnit::MapPixel, "vcl.window", "MapMode must be PIXEL based");
    if (GetMapMode().GetMapUnit() != MapUnit::MapPixel)
        return;

    // Preserve graphics state
    pOutDev->Push();
    vcl::Region aClipRegion(pOutDev->GetClipRegion());
    pOutDev->SetClipRegion();

    GDIMetaFile* pOldMtf = pOutDev->GetConnectMetaFile();
    GDIMetaFile aMtf;
    pOutDev->SetConnectMetaFile(&aMtf);

    // Put a push action to metafile
    pOutDev->Push();
    // Copy graphics state to metafile
    vcl::Font aCopyFont = GetFont();
    if (nOldDPIX != pOutDev->GetDPIX() || nOldDPIY != pOutDev->GetDPIY())
    {
        aCopyFont.SetFontHeight(aCopyFont.GetFontHeight() * pOutDev->GetDPIY() / nOldDPIY);
        aCopyFont.SetFontWidth(aCopyFont.GetFontWidth() * pOutDev->GetDPIX() / nOldDPIX);
    }
    SetFont(aCopyFont);
    SetTextColor(GetTextColor());

    if (pOutDev->HasLineColor())
        pOutDev->SetLineColor(pOutDev->GetLineColor());
    else
        pOutDev->SetLineColor();

    if (pOutDev->HasFillColor())
        pOutDev->SetFillColor(pOutDev->GetFillColor());
    else
        pOutDev->SetFillColor();

    if (HasTextLineColor())
        SetTextLineColor(GetTextLineColor());
    else
        SetTextLineColor();

    if (HasOverlineColor())
        SetOverlineColor(GetOverlineColor());
    else
        SetOverlineColor();

    if (HasTextFillColor())
        SetTextFillColor(GetTextFillColor());
    else
        SetTextFillColor();

    SetTextAlign(GetTextAlign());
    pOutDev->SetRasterOp(pOutDev->GetRasterOp());

    if (pOutDev->IsReferencePoint())
        pOutDev->SetReferencePoint(pOutDev->GetReferencePoint());
    else
        pOutDev->SetReferencePoint();

    pOutDev->SetLayoutMode(pOutDev->GetLayoutMode());

    pOutDev->SetDigitLanguage(pOutDev->GetDigitLanguage());
    tools::Rectangle aPaintRect(Point(0, 0), GetOutputSizePixel());
    aClipRegion.Intersect(aPaintRect);
    pOutDev->SetClipRegion(aClipRegion);

    // Do the actual paint

    // Background
    if (!IsPaintTransparent() && HasBackground() && !(vcl::clipping::getParentClipMode(*this) & ParentClipMode::NoClip))
    {
        Erase(*pOutDev);
    }
    // Foreground
    Paint(*pOutDev, aPaintRect);
    // Put a pop action to metafile
    pOutDev->Pop();

    pOutDev->SetConnectMetaFile(pOldMtf);
    pOutDev->EnableOutput(bOutput);
    mpVisibilityState->mbReallyVisible = bRVisible;

    // Paint metafile to VDev
    VclPtrInstance<VirtualDevice> pMaskedDevice(rTargetOutDev, DeviceFormat::WITH_ALPHA);

    pMaskedDevice->SetOutputSizePixel(GetOutputSizePixel(), true, true);
    pMaskedDevice->EnableRTL(IsRTLEnabled());
    aMtf.WindStart();
    aMtf.Play(*pMaskedDevice);
    Bitmap aBmpEx(pMaskedDevice->GetBitmap(Point(0, 0), aPaintRect.GetSize()));
    rTargetOutDev.DrawBitmap(i_rPos, aBmpEx);
    // Get rid of virtual device now so they don't pile up during recursive calls
    pMaskedDevice.disposeAndClear();

    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [&](vcl::Window& rChild) {
            WindowPlatformState* pChildPlatform = rChild.ImplGetPlatformState();
            SalFrame* pChildFrame = pChildPlatform ? pChildPlatform->mpFrame : nullptr;

            if (pChildFrame == pFrame && rChild.IsVisible())
            {
                OutputDevice* pChildOutDev = rChild.GetOutDev();
                tools::Long nDeltaX = (pChildOutDev ? pChildOutDev->GetDeviceOriginX() : 0) - pOutDev->GetDeviceOriginX();

                if (pOutDev->HasMirroredGraphics())
                    nDeltaX = pOutDev->GetOutputWidthPixel() - nDeltaX - (pChildOutDev ? pChildOutDev->GetOutputWidthPixel() : 0);
                tools::Long nDeltaY = rChild.GetDeviceOriginY() - GetDeviceOriginY();
                Point aPos(i_rPos);
                // tdf#165706 those delta values are in pixels, but aPos copied from
                // i_rPos *may* be in logical coordinates if a MapMode is set at
                // i_pTargetOutDev. To not mix values of different coordinate systems
                // it *needs* to be converted (which does nothing if no MapMode)
                Point aDelta = rTargetOutDev.convertTo<vcl::LogicPoint>(
                    vcl::WindowPoint(nDeltaX, nDeltaY)
                );
                aPos += aDelta;
                rChild.ImplPaintToDevice(rTargetOutDev, aPos);
            }
        });
    }

    // Restore graphics state
    pOutDev->Pop();

    pOutDev->EnableOutput(bOutput);
    mpVisibilityState->mbReallyVisible = bRVisible;
    pOutDev->mbDevOutput = bDevOutput;
    pOutDev->SetDPIX(nOldDPIX);
    pOutDev->SetDPIY(nOldDPIY);
}

void Window::PaintToDevice(OutputDevice& rDev, const Point& rPos)
{
    if (!mpClassification || !mpVisibilityState)
        return;

    SAL_WARN_IF(rDev.HasMirroredGraphics(), "vcl.window", "PaintToDevice to mirroring graphics");
    SAL_WARN_IF(rDev.IsRTLEnabled(), "vcl.window", "PaintToDevice to mirroring device");

    vcl::Window* pRealParent = nullptr;
    if (!mpVisibilityState->mbVisible)
    {
        vcl::Window* pTempParent = ImplGetDefaultWindow();
        if (pTempParent)
            pTempParent->EnableChildTransparentMode();
        pRealParent = GetParent();
        if (pTempParent)
            SetParent(pTempParent);
        // Trigger correct visibility flags for children
        Show();
        Hide();
    }

    const bool bVisible = mpVisibilityState->mbVisible;
    mpVisibilityState->mbVisible = true;

    vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr;
    if (pBorderWindow)
        pBorderWindow->ImplPaintToDevice(rDev, rPos);
    else
        ImplPaintToDevice(rDev, rPos);

    mpVisibilityState->mbVisible = bVisible;

    if (pRealParent)
        SetParent(pRealParent);
}

void Window::Erase(vcl::RenderContext& rRenderContext)
{
    if (!GetOutDev()->IsDeviceOutputNecessary() || GetOutDev()->ImplIsRecordLayout())
        return;

    bool bNativeOK = false;

    ControlPart aCtrlPart = ImplGetControlAppearance()->getNativeBackground();

    if (aCtrlPart == ControlPart::Entire && HasControlBackground())
    {
        // nothing to do here; background is drawn in corresponding drawNativeControl implementation
        bNativeOK = true;
    }
    else if (aCtrlPart != ControlPart::NONE && ! HasControlBackground())
    {
        tools::Rectangle aCtrlRegion(Point(), GetOutputSizePixel());
        ControlState nState = ControlState::NONE;

        if (IsEnabled())
            nState |= ControlState::ENABLED;

        bNativeOK = rRenderContext.DrawNativeControl(ControlType::WindowBackground, aCtrlPart, aCtrlRegion,
                                                     nState, ImplControlValue(), OUString());
    }

    if (GetOutDev()->mbBackground && !bNativeOK)
    {
        RasterOp eRasterOp = GetOutDev()->GetRasterOp();
        if (eRasterOp != RasterOp::OverPaint)
            GetOutDev()->SetRasterOp(RasterOp::OverPaint);
        rRenderContext.DrawWallpaper(0, 0, GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel(), GetOutDev()->maBackground);
        if (eRasterOp != RasterOp::OverPaint)
            rRenderContext.SetRasterOp(eRasterOp);
    }
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll, ScrollFlags nFlags )
{

    ImplScroll( GetOutputRectPixel(),
                nHorzScroll, nVertScroll, nFlags & ~ScrollFlags::Clip );
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll,
                     const tools::Rectangle& rRect, ScrollFlags nFlags )
{
    OutputDevice *pOutDev = GetOutDev();
    tools::Rectangle aRect = pOutDev->GetMapper().LogicToDevicePixel(rRect, pOutDev->GetMappingPolicy());
    aRect.Intersection( GetOutputRectPixel() );
    if ( !aRect.IsEmpty() )
        ImplScroll( aRect, nHorzScroll, nVertScroll, nFlags );
}

void Window::ImplScroll(const tools::Rectangle& rRect,
                        tools::Long nHorzScroll, tools::Long nVertScroll, ScrollFlags nFlags)
{
    OutputDevice* pOutDev = GetOutDev();
    if (!pOutDev || !pOutDev->IsDeviceOutputNecessary())
        return;

    nHorzScroll = pOutDev->LogicWidthToDevicePixel(nHorzScroll);
    nVertScroll = pOutDev->LogicHeightToDevicePixel(nVertScroll);

    if (!nHorzScroll && !nVertScroll)
        return;

    if (mpControlAppearance && mpControlAppearance->getCursor())
        mpControlAppearance->suspendCursor();

    ScrollFlags nEffectiveFlags = nFlags;
    if (!(nFlags & (ScrollFlags::Children | ScrollFlags::NoChildren)))
    {
        if (GetStyle() & WB_CLIPCHILDREN)
            nFlags |= ScrollFlags::NoChildren;
        else
            nFlags |= ScrollFlags::Children;
    }

    vcl::Region aInvalidateRegion;
    vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr;
    const bool bScrollChildren = (nFlags & ScrollFlags::Children) && (pFirstChild != nullptr);

    // RTL: check if this window requires special action
    const bool bReMirror = pOutDev->ImplIsAntiparallel();

    tools::Rectangle aRectMirror(rRect);
    if (bReMirror)
    {
        // Make sure the invalidate region of this window is
        // computed in the same coordinate space as the one from the overlap windows
        pOutDev->ReMirror(aRectMirror);
    }

    // Adapt paint areas
    ImplMoveAllInvalidateRegions(aRectMirror, nHorzScroll, nVertScroll, bScrollChildren);

    vcl::clipping::calcOverlapRegion(*this, aRectMirror, aInvalidateRegion, !bScrollChildren, false);

    // If the scrolling on the device is performed in the opposite direction
    // then move the overlaps in that direction to compute the invalidate region
    // on the correct side, i.e., revert nHorzScroll
    if (!aInvalidateRegion.IsEmpty())
    {
        aInvalidateRegion.Move(bReMirror ? -nHorzScroll : nHorzScroll, nVertScroll);
    }

    tools::Rectangle aDestRect(aRectMirror);
    aDestRect.Move(bReMirror ? -nHorzScroll : nHorzScroll, nVertScroll);
    vcl::Region aWinInvalidateRegion(aRectMirror);
    if (!SupportsDoubleBuffering())
    {
        // There will be no CopyArea() call below, so invalidate the
        // whole visible area, not only the smaller one that was just
        // scrolled in.
        aWinInvalidateRegion.Exclude(aDestRect);
    }

    aInvalidateRegion.Union(aWinInvalidateRegion);

    vcl::Region aRegion(GetOutputRectPixel());
    if (nFlags & ScrollFlags::Clip)
        aRegion.Intersect(rRect);
    if (mpClippingState && mpClippingState->mbWinRegion)
        aRegion.Intersect(pOutDev->GetMapper().ViewToDevice(mpClippingState->maWinRegion));

    aRegion.Exclude(aInvalidateRegion);

    vcl::clipping::clipBoundaries(*this, aRegion, false, true);
    if (!bScrollChildren)
    {
        if (nEffectiveFlags & ScrollFlags::NoChildren)
            vcl::clipping::clipAllChildren(*this, aRegion);
        else
            vcl::clipping::clipChildren(*this, aRegion);
    }

    const auto& rClipState = pOutDev->GetClipState();
    if (rClipState.mbHasCustomClip && (nFlags & ScrollFlags::UseClipRegion))
        aRegion.Intersect(rClipState.maRegion);

    if (!aRegion.IsEmpty())
    {
        if (mpWinData)
        {
            if (mpFocusState && mpFocusState->isFocusVisible() && mpWinData->mpFocusRect)
                ImplInvertFocus(*mpWinData->mpFocusRect);
            if (mpClassification && mpClassification->mbTrackVisible && (mpWinData->mnTrackFlags & ShowTrackFlags::TrackWindow) && mpWinData->mpTrackRect)
                InvertTracking(*mpWinData->mpTrackRect, mpWinData->mnTrackFlags);
        }
#ifndef IOS
        // This seems completely unnecessary with tiled rendering, and
        // causes the "AquaSalGraphics::copyArea() for non-layered
        // graphics" message. Presumably we should bypass this on all
        // platforms when dealing with a "window" that uses tiled
        // rendering at the moment. Unclear how to figure that out,
        // though. Also unclear whether we actually could just not
        // create a "frame window", whatever that exactly is, in the
        // tiled rendering case, or at least for platforms where tiles
        // rendering is all there is.

        SalGraphics* pGraphics = ImplGetFrameGraphics();
        // The invalidation area contains the area what would be copied here,
        // so avoid copying in case of double buffering.
        if (pGraphics && !SupportsDoubleBuffering())
        {
            if (bReMirror)
            {
                pOutDev->ReMirror(aRegion);
            }

            pOutDev->SelectClipRegion(aRegion, pGraphics);
            pGraphics->CopyArea(rRect.Left() + nHorzScroll, rRect.Top() + nVertScroll,
                                rRect.Left(), rRect.Top(),
                                rRect.GetWidth(), rRect.GetHeight(),
                                *pOutDev);
        }
#endif
        if (mpWinData)
        {
            if (mpFocusState && mpFocusState->isFocusVisible() && mpWinData->mpFocusRect)
                ImplInvertFocus(*mpWinData->mpFocusRect);
            if (mpClassification && mpClassification->mbTrackVisible && (mpWinData->mnTrackFlags & ShowTrackFlags::TrackWindow) && mpWinData->mpTrackRect)
                InvertTracking(*mpWinData->mpTrackRect, mpWinData->mnTrackFlags);
        }
    }

    if (!aInvalidateRegion.IsEmpty())
    {
        // RTL: the invalidate region for this window is already computed in frame coordinates
        // so it has to be re-mirrored before calling the Paint-handler
        if (mpInvalidation)
            mpInvalidation->setCheckRtl();

        if (!bScrollChildren)
        {
            if (nEffectiveFlags & ScrollFlags::NoChildren)
                vcl::clipping::clipAllChildren(*this, aInvalidateRegion);
            else
                vcl::clipping::clipChildren(*this, aInvalidateRegion);
        }

        ImplInvalidateFrameRegion(&aInvalidateRegion, InvalidateFlags::Children);
    }

    if (bScrollChildren && pFirstChild)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [nHorzScroll, nVertScroll](vcl::Window& rChild) {
            Point aPos = rChild.GetPosPixel();
            aPos += Point(nHorzScroll, nVertScroll);
            rChild.SetPosPixel(aPos);
        });
    }

    if (nFlags & ScrollFlags::Update)
        PaintImmediately();

    if (mpControlAppearance && mpControlAppearance->getCursor())
        mpControlAppearance->resumeCursor();
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
