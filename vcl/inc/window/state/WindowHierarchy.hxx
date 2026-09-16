/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#pragma once

#include <vcl/vclptr.hxx>

#include <type_traits>
#include <utility>

namespace vcl
{
class Window;
}

class WindowHierarchy
{
public:
    // Getters
    vcl::Window* getParent() const { return mpParent.get(); }
    vcl::Window* getRealParent() const { return mpRealParent.get(); }
    vcl::Window* getFirstChild() const { return mpFirstChild.get(); }
    vcl::Window* getLastChild() const { return mpLastChild.get(); }
    vcl::Window* getFirstOverlap() const { return mpFirstOverlap.get(); }
    vcl::Window* getLastOverlap() const { return mpLastOverlap.get(); }
    vcl::Window* getPrevSibling() const { return mpPrev.get(); }
    vcl::Window* getNextSibling() const { return mpNext.get(); }
    vcl::Window* getOverlapWindow() const { return mpOverlapWindow.get(); }
    vcl::Window* getFrameWindow() const { return mpFrameWindow.get(); }
    vcl::Window* getBorderWindow() const { return mpBorderWindow.get(); }
    vcl::Window* getClientWindow() const { return mpClientWindow.get(); }

    bool hasChildren() const { return mpFirstChild != nullptr; }
    bool hasBorderWindow() const { return mpBorderWindow != nullptr; }

    // Mutators
    void setParent(vcl::Window* p) { mpParent = p; }
    void setRealParent(vcl::Window* p) { mpRealParent = p; }
    void setFrameWindow(vcl::Window* p) { mpFrameWindow = p; }
    void setOverlapWindow(vcl::Window* p) { mpOverlapWindow = p; }
    void setBorderWindow(vcl::Window* p) { mpBorderWindow = p; }
    void setClientWindow(vcl::Window* p) { mpClientWindow = p; }

    // Structural operations
    vcl::Window* getTopmostFrameWindow() const;

    void insertOverlapWindow(vcl::Window* pWindow, vcl::Window* pParent, ImplFrameData& rFrameData);
    void linkIntoOverlapChain(vcl::Window* pWindow, vcl::Window* pOverlapParent);
    void linkIntoChildChain(vcl::Window* pWindow, vcl::Window* pParent);

    void unlinkOverlapWindow(ImplFrameData& rFrameData);
    void unlinkFromChildChain();

    void reorderWithinParent(vcl::Window* pWindow, sal_uInt16 nNewPosition);
    bool reorderRelativeTo(vcl::Window* pWindow, vcl::Window* pRefWindow, ZOrderFlags nFlags,
                           bool bOverlap);

    void toBottomChild(vcl::Window* pWindow);
    bool toTopOverlap(vcl::Window* pWindow, bool bOnTop);

    vcl::Window* getBoundarySibling(ZOrderFlags nFlags, bool bOverlap) const;
    static vcl::Window* findOutermostBorderWindow(vcl::Window* pWindow);

    // Visitors
    template <typename Func> static void forEachSibling(vcl::Window* pStart, Func&& rVisitor)
    {
        vcl::Window* pCurrent = pStart;
        while (pCurrent)
        {
            WindowHierarchy* pHierarchy = pCurrent->ImplGetWindowHierarchy();
            vcl::Window* pNext = pHierarchy ? pHierarchy->mpNext.get() : nullptr;

            if constexpr (std::is_same_v<std::invoke_result_t<Func, vcl::Window&>, void>)
            {
                rVisitor(*pCurrent);
            }
            else
            {
                if (!rVisitor(*pCurrent))
                    break;
            }

            pCurrent = pNext;
        }
    }

    template <typename Func> void forEachSibling(bool bOverlap, Func&& rVisitor) const
    {
        forEachSibling(getBoundarySibling(ZOrderFlags::First, bOverlap),
                       std::forward<Func>(rVisitor));
    }

    template <typename Func> static void forEachAncestor(vcl::Window* pStart, Func&& rVisitor)
    {
        vcl::Window* pCurrent = pStart;
        while (pCurrent)
        {
            WindowHierarchy* pHierarchy = pCurrent->ImplGetWindowHierarchy();
            vcl::Window* pNext = pHierarchy ? pHierarchy->getParent() : nullptr;

            if constexpr (std::is_same_v<std::invoke_result_t<Func, vcl::Window&>, void>)
            {
                rVisitor(*pCurrent);
            }
            else
            {
                if (!rVisitor(*pCurrent))
                    break;
            }

            pCurrent = pNext;
        }
    }

    template <typename Func> void forEachAncestor(Func&& rVisitor) const
    {
        forEachAncestor(mpParent, std::forward<Func>(rVisitor));
    }

    template <typename Func> static bool forEachDescendant(vcl::Window* pRoot, Func&& rVisitor)
    {
        if (!pRoot)
            return true;

        WindowHierarchy* pHierarchy = pRoot->ImplGetWindowHierarchy();
        if (!pHierarchy)
            return true;

        bool bContinue = true;
        forEachSibling(pHierarchy->mpFirstChild, [&](vcl::Window& rChild) -> bool {
            if constexpr (std::is_same_v<std::invoke_result_t<Func, vcl::Window&>, void>)
            {
                rVisitor(rChild);
                forEachDescendant(&rChild, rVisitor);
                return true;
            }
            else
            {
                if (!rVisitor(rChild) || !forEachDescendant(&rChild, rVisitor))
                {
                    bContinue = false;
                    return false;
                }
                return true;
            }
        });

        return bContinue;
    }

    template <typename Func> static void forEachBorderWindow(vcl::Window* pStart, Func&& rVisitor)
    {
        vcl::Window* pCurrent = pStart;
        while (pCurrent)
        {
            WindowHierarchy* pHierarchy = pCurrent->ImplGetWindowHierarchy();
            vcl::Window* pBorder = pHierarchy ? pHierarchy->mpBorderWindow.get() : nullptr;
            if (!pBorder)
                break;

            if constexpr (std::is_same_v<std::invoke_result_t<Func, vcl::Window&>, void>)
                rVisitor(*pBorder);
            else if (!rVisitor(*pBorder))
                break;

            pCurrent = pBorder;
        }
    }

    template <typename Func>
    static void forEachPreviousSibling(vcl::Window* pStart, Func&& aFunc)
    {
        while (pStart)
        {
            WindowHierarchy* pHierarchy = pStart->ImplGetWindowHierarchy();
            vcl::Window* pPrev = pHierarchy ? pHierarchy->getPrevSibling() : nullptr;
            aFunc(*pStart);
            pStart = pPrev;
        }
    }

    template <typename Func>
    static void forEachPreviousSibling(const vcl::Window* pStart, Func&& aFunc)
    {
        while (pStart)
        {
            const WindowHierarchy* pHierarchy = pStart->ImplGetWindowHierarchy();
            const vcl::Window* pPrev = pHierarchy ? pHierarchy->getPrevSibling() : nullptr;
            aFunc(*pStart);
            pStart = pPrev;
        }
    }

    template <typename Func>
    static void forEachOverlap(vcl::Window* pStart, Func&& aFunc)
    {
        while (pStart)
        {
            WindowHierarchy* pHierarchy = pStart->ImplGetWindowHierarchy();
            vcl::Window* pNext = pHierarchy ? pHierarchy->mpNextOverlap : nullptr;
            aFunc(*pStart);
            pStart = pNext;
        }
    }

    template <typename Func>
    static void forEachOverlap(const vcl::Window* pStart, Func&& aFunc)
    {
        while (pStart)
        {
            const WindowHierarchy* pHierarchy = pStart->ImplGetWindowHierarchy();
            const vcl::Window* pNext = pHierarchy ? pHierarchy->mpNextOverlap : nullptr;
            aFunc(*pStart);
            pStart = pNext;
        }
    }

    void disposeBorderWindow()
    {
        mpBorderWindow.disposeAndClear();
    }

private:
    VclPtr<vcl::Window> mpParent;
    VclPtr<vcl::Window> mpRealParent;

    VclPtr<vcl::Window> mpFirstChild;
    VclPtr<vcl::Window> mpLastChild;

    VclPtr<vcl::Window> mpFirstOverlap;
    VclPtr<vcl::Window> mpLastOverlap;

    VclPtr<vcl::Window> mpPrev;
    VclPtr<vcl::Window> mpNext;

    VclPtr<vcl::Window> mpPrevOverlap;
    VclPtr<vcl::Window> mpNextOverlap;

    VclPtr<vcl::Window> mpFrameWindow;
    VclPtr<vcl::Window> mpOverlapWindow;
    VclPtr<vcl::Window> mpBorderWindow;
    VclPtr<vcl::Window> mpClientWindow;

    void unlinkFromGlobalOverlapChain(ImplFrameData& rFrameData);
    void unlinkFromOverlapChain();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
