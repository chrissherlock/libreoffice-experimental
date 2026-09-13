/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <comphelper/scopeguard.hxx>

#include <vcl/event.hxx>
#include <vcl/window.hxx>

#include <WindowEventHandlers.hxx>

void WindowEventHandlers::notifyEventListeners(VclWindowEvent& rEvent,
                                               const VclPtr<vcl::Window>& xWindow,
                                               bool bIgnoreDisposed)
{
    if (maEventListeners.empty())
        return;

    // Copy the list, because this can be destroyed when calling a Link...
    std::vector<Link<VclWindowEvent&, void>> aCopy(maEventListeners);

    // we use an iterating counter/flag and a set of deleted Link's to avoid O(n^2) behaviour
    mnEventListenersIteratingCount++;

    comphelper::ScopeGuard aGuard([this, &xWindow, bIgnoreDisposed]() {
        if (bIgnoreDisposed || !xWindow->isDisposed())
        {
            mnEventListenersIteratingCount--;
            if (mnEventListenersIteratingCount == 0)
                maEventListenersDeleted.clear();
        }
    });

    for (const Link<VclWindowEvent&, void>& rLink : aCopy)
    {
        if (!bIgnoreDisposed && xWindow->isDisposed())
            break;

        // check this hasn't been removed in some re-entrancy scenario fdo#47368
        if (maEventListenersDeleted.find(rLink) == maEventListenersDeleted.end())
        {
            rLink.Call(rEvent);
        }
    }
}

void WindowEventHandlers::notifyChildEventListeners(VclWindowEvent& rEvent,
                                                    const VclPtr<vcl::Window>& xWindow,
                                                    bool bIgnoreDisposed)
{
    if (maChildEventListeners.empty())
        return;

    // Copy the list, because this can be destroyed when calling a Link...
    std::vector<Link<VclWindowEvent&, void>> aCopy(maChildEventListeners);

    // we use an iterating counter/flag and a set of deleted Link's to avoid O(n^2) behaviour
    mnChildEventListenersIteratingCount++;

    comphelper::ScopeGuard aGuard([this, &xWindow, bIgnoreDisposed]() {
        if (bIgnoreDisposed || !xWindow->isDisposed())
        {
            mnChildEventListenersIteratingCount--;
            if (mnChildEventListenersIteratingCount == 0)
                maChildEventListenersDeleted.clear();
        }
    });

    for (const Link<VclWindowEvent&, void>& rLink : aCopy)
    {
        if (!bIgnoreDisposed && xWindow->isDisposed())
            return;

        // Check this hasn't been removed in some re-entrancy scenario fdo#47368
        if (maChildEventListenersDeleted.find(rLink) == maChildEventListenersDeleted.end())
        {
            rLink.Call(rEvent);
        }
    }
}

void WindowEventHandlers::addEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    maEventListeners.push_back(rEventListener);
}

void WindowEventHandlers::removeEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    std::erase(maEventListeners, rEventListener);
    if (mnEventListenersIteratingCount)
        maEventListenersDeleted.insert(rEventListener);
}

void WindowEventHandlers::addChildEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    maChildEventListeners.push_back(rEventListener);
}

void WindowEventHandlers::removeChildEventListener(
    const Link<VclWindowEvent&, void>& rEventListener)
{
    std::erase(maChildEventListeners, rEventListener);
    if (mnChildEventListenersIteratingCount)
        maChildEventListenersDeleted.insert(rEventListener);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
