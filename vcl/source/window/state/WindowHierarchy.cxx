/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>

#include <window/state/ImplFrameData.hxx>
#include <window/state/WindowHierarchy.hxx>

void WindowHierarchy::linkIntoOverlapChain(vcl::Window* pWindow, vcl::Window* pOverlapParent)
{
    mpOverlapWindow = pOverlapParent;

    WindowHierarchy* pParentHierarchy = pOverlapParent->ImplGetWindowHierarchy();

    // Overlap windows default to the head of the chain (uppermost Z-order)
    mpNext = pParentHierarchy->mpFirstOverlap;
    pParentHierarchy->mpFirstOverlap = pWindow;

    if (!pParentHierarchy->mpLastOverlap)
        pParentHierarchy->mpLastOverlap = pWindow;
    else
        mpNext->ImplGetWindowHierarchy()->mpPrev = pWindow;
}

void WindowHierarchy::linkIntoChildChain(vcl::Window* pWindow, vcl::Window* pParent)
{
    if (pParent->ImplIsOverlapWindow())
        mpOverlapWindow = pParent;
    else
        mpOverlapWindow = pParent->ImplGetWindowHierarchy()->mpOverlapWindow;

    // Standard child windows append to the tail of the child chain
    mpPrev = pParent->ImplGetWindowHierarchy()->mpLastChild;
    pParent->ImplGetWindowHierarchy()->mpLastChild = pWindow;

    if (!pParent->ImplGetWindowHierarchy()->mpFirstChild)
        pParent->ImplGetWindowHierarchy()->mpFirstChild = pWindow;
    else
        mpPrev->ImplGetWindowHierarchy()->mpNext = pWindow;
}

void WindowHierarchy::unlinkOverlapWindow(ImplFrameData& rFrameData)
{
    unlinkFromGlobalOverlapChain(rFrameData);
    unlinkFromOverlapChain();
}

void WindowHierarchy::unlinkFromGlobalOverlapChain(ImplFrameData& rFrameData)
{
    // True O(1) extraction utilizing mpPrevOverlap pointer
    if (mpPrevOverlap)
        mpPrevOverlap->ImplGetWindowHierarchy()->mpNextOverlap = mpNextOverlap;
    else
        rFrameData.mpFirstOverlap = mpNextOverlap;

    if (mpNextOverlap)
        mpNextOverlap->ImplGetWindowHierarchy()->mpPrevOverlap = mpPrevOverlap;

    mpPrevOverlap = nullptr;
    mpNextOverlap = nullptr;
}

void WindowHierarchy::unlinkFromOverlapChain()
{
    WindowHierarchy* pOverlapHierarchy
        = mpOverlapWindow ? mpOverlapWindow->ImplGetWindowHierarchy() : nullptr;

    // Unlink from the forward sibling chain (adjust head)
    if (mpPrev)
        mpPrev->ImplGetWindowHierarchy()->mpNext = mpNext;
    else if (pOverlapHierarchy)
        pOverlapHierarchy->mpFirstOverlap = mpNext;

    // Unlink from the backward sibling chain (adjust tail)
    if (mpNext)
        mpNext->ImplGetWindowHierarchy()->mpPrev = mpPrev;
    else if (pOverlapHierarchy)
        pOverlapHierarchy->mpLastOverlap = mpPrev;

    mpPrev = nullptr;
    mpNext = nullptr;
}

void WindowHierarchy::unlinkFromChildChain()
{
    vcl::Window* pParent = getParent();
    WindowHierarchy* pParentHierarchy = pParent ? pParent->ImplGetWindowHierarchy() : nullptr;

    // Unlink from the forward sibling chain (adjust head)
    if (mpPrev)
        mpPrev->ImplGetWindowHierarchy()->mpNext = mpNext;
    else if (pParentHierarchy)
        pParentHierarchy->mpFirstChild = mpNext;

    // Unlink from the backward sibling chain (adjust tail)
    if (mpNext)
        mpNext->ImplGetWindowHierarchy()->mpPrev = mpPrev;
    else if (pParentHierarchy)
        pParentHierarchy->mpLastChild = mpPrev;

    mpPrev = nullptr;
    mpNext = nullptr;
}

vcl::Window* WindowHierarchy::getTopmostFrameWindow() const
{
    const WindowHierarchy* pHierarchy = this;
    while (vcl::Window* pParent = pHierarchy->getParent())
    {
        WindowHierarchy* pParentHierarchy = pParent->ImplGetWindowHierarchy();
        if (!pParentHierarchy)
            break;
        pHierarchy = pParentHierarchy;
    }

    return pHierarchy->mpFrameWindow;
}

void WindowHierarchy::insertOverlapWindow(vcl::Window* pWindow, vcl::Window* pParent,
                                          ImplFrameData& rFrameData)
{
    vcl::Window* pFirstOverlapParent = pParent;
    while (!pFirstOverlapParent->ImplIsOverlapWindow())
    {
        pFirstOverlapParent = pFirstOverlapParent->ImplGetParent();
    }

    // Link into global frame overlap list
    mpNextOverlap = rFrameData.mpFirstOverlap;
    if (mpNextOverlap)
        mpNextOverlap->ImplGetWindowHierarchy()->mpPrevOverlap = pWindow;

    rFrameData.mpFirstOverlap = pWindow;
    mpPrevOverlap = nullptr;

    linkIntoOverlapChain(pWindow, pFirstOverlapParent);
}

void WindowHierarchy::reorderWithinParent(vcl::Window* pWindow, sal_uInt16 nNewPosition)
{
    WindowHierarchy* pParentHierarchy = mpParent ? mpParent->ImplGetWindowHierarchy() : nullptr;
    if (!pParentHierarchy)
        return;

    sal_uInt16 nChildCount = 0;
    vcl::Window* pSource = pParentHierarchy->mpFirstChild;

    while (pSource)
    {
        if (nChildCount == nNewPosition)
            break;
        pSource = pSource->ImplGetWindowHierarchy()->mpNext;
        nChildCount++;
    }

    if (pSource == pWindow) // already at the right place
        return;

    unlinkFromChildChain();

    if (pSource)
    {
        mpNext = pSource;
        mpPrev = pSource->ImplGetWindowHierarchy()->mpPrev;
        pSource->ImplGetWindowHierarchy()->mpPrev = pWindow;
    }
    else
    {
        // Appending to the end: link after the current tail
        mpPrev = pParentHierarchy->mpLastChild;
        pParentHierarchy->mpLastChild = pWindow;
    }

    if (mpPrev)
        mpPrev->ImplGetWindowHierarchy()->mpNext = pWindow;
    else
        pParentHierarchy->mpFirstChild = pWindow;
}

void WindowHierarchy::toBottomChild(vcl::Window* pWindow)
{
    WindowHierarchy* pParentHierarchy = mpParent ? mpParent->ImplGetWindowHierarchy() : nullptr;
    if (!pParentHierarchy || pParentHierarchy->mpLastChild.get() == pWindow)
        return;

    unlinkFromChildChain();

    // Reattach to the tail of the parent's child chain
    mpPrev = pParentHierarchy->mpLastChild;
    pParentHierarchy->mpLastChild = pWindow;

    if (mpPrev)
        mpPrev->ImplGetWindowHierarchy()->mpNext = pWindow;
    else
        pParentHierarchy->mpFirstChild = pWindow;

    mpNext = nullptr;
}

bool WindowHierarchy::toTopOverlap(vcl::Window* pWindow, bool bOnTop)
{
    WindowHierarchy* pOverlapHierarchy
        = mpOverlapWindow ? mpOverlapWindow->ImplGetWindowHierarchy() : nullptr;
    if (!pOverlapHierarchy || pOverlapHierarchy->mpFirstOverlap.get() == pWindow)
        return false;

    // Remove window from the overlap chain
    unlinkFromOverlapChain();

    // Respect AlwaysOnTop: if not on top, skip past existing AlwaysOnTop windows
    vcl::Window* pNextWin = pOverlapHierarchy->mpFirstOverlap;
    if (!bOnTop)
    {
        while (pNextWin)
        {
            if (!pNextWin->IsAlwaysOnTopEnabled())
                break;
            pNextWin = pNextWin->ImplGetWindowHierarchy()->mpNext;
        }
    }

    // Reinsert window before pNextWin
    mpNext = pNextWin;
    if (pNextWin)
    {
        mpPrev = pNextWin->ImplGetWindowHierarchy()->mpPrev;
        pNextWin->ImplGetWindowHierarchy()->mpPrev = pWindow;
    }
    else
    {
        mpPrev = pOverlapHierarchy->mpLastOverlap;
        pOverlapHierarchy->mpLastOverlap = pWindow;
    }

    if (mpPrev)
        mpPrev->ImplGetWindowHierarchy()->mpNext = pWindow;
    else
        pOverlapHierarchy->mpFirstOverlap = pWindow;

    return true;
}

bool WindowHierarchy::reorderRelativeTo(vcl::Window* pWindow, vcl::Window* pRefWindow,
                                        ZOrderFlags nFlags, bool bOverlap)
{
    WindowHierarchy* pRefHierarchy = pRefWindow->ImplGetWindowHierarchy();
    if (!pRefHierarchy)
        return false;

    if (nFlags & ZOrderFlags::Before)
    {
        if (pRefHierarchy->mpPrev.get() == pWindow)
            return false;

        if (bOverlap)
            unlinkFromOverlapChain();
        else
            unlinkFromChildChain();

        mpPrev = pRefHierarchy->mpPrev;
        mpNext = pRefWindow;

        if (mpPrev)
            mpPrev->ImplGetWindowHierarchy()->mpNext = pWindow;
        else if (bOverlap)
            mpOverlapWindow->ImplGetWindowHierarchy()->mpFirstOverlap = pWindow;
        else
            mpParent->ImplGetWindowHierarchy()->mpFirstChild = pWindow;

        pRefHierarchy->mpPrev = pWindow;
        return true;
    }
    else if (nFlags & ZOrderFlags::Behind)
    {
        if (pRefHierarchy->mpNext.get() == pWindow)
            return false;

        if (bOverlap)
            unlinkFromOverlapChain();
        else
            unlinkFromChildChain();

        mpPrev = pRefWindow;
        mpNext = pRefHierarchy->mpNext;

        if (mpNext)
            mpNext->ImplGetWindowHierarchy()->mpPrev = pWindow;
        else if (bOverlap)
            mpOverlapWindow->ImplGetWindowHierarchy()->mpLastOverlap = pWindow;
        else
            mpParent->ImplGetWindowHierarchy()->mpLastChild = pWindow;

        pRefHierarchy->mpNext = pWindow;
        return true;
    }

    return false;
}

vcl::Window* WindowHierarchy::getBoundarySibling(ZOrderFlags nFlags, bool bOverlap) const
{
    WindowHierarchy* pContainer
        = bOverlap ? (mpOverlapWindow ? mpOverlapWindow->ImplGetWindowHierarchy() : nullptr)
                   : (mpParent ? mpParent->ImplGetWindowHierarchy() : nullptr);

    if (!pContainer)
        return nullptr;

    if (nFlags & ZOrderFlags::First)
        return bOverlap ? pContainer->mpFirstOverlap : pContainer->mpFirstChild;

    if (nFlags & ZOrderFlags::Last)
        return bOverlap ? pContainer->mpLastOverlap : pContainer->mpLastChild;

    return nullptr;
}

vcl::Window* WindowHierarchy::findOutermostBorderWindow(vcl::Window* pWindow)
{
    while (pWindow)
    {
        WindowHierarchy* pHierarchy = pWindow->ImplGetWindowHierarchy();
        if (!pHierarchy || !pHierarchy->mpBorderWindow)
            break;
        pWindow = pHierarchy->mpBorderWindow;
    }

    return pWindow;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
