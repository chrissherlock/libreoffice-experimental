/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>

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

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
