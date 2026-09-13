/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/link.hxx>

#include <vector>
#include <set>

class CommandEvent;
class VclWindowEvent;

namespace vcl
{
class Window;
}

class WindowEventHandlers
{
public:
    WindowEventHandlers() = default;

    void notifyEventListeners(VclWindowEvent& rEvent, const VclPtr<vcl::Window>& xWindow,
                              bool bIgnoreDisposed);

    void notifyChildEventListeners(VclWindowEvent& rEvent, const VclPtr<vcl::Window>& xWindow,
                                   bool bIgnoreDisposed);

    bool hasEventListeners() const { return !maEventListeners.empty(); }
    void addEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    void removeEventListener(const Link<VclWindowEvent&, void>& rEventListener);

    void addChildEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    void removeChildEventListener(const Link<VclWindowEvent&, void>& rEventListener);

    void setMnemonicActivateHdl(const Link<vcl::Window&, bool>& rLink)
    {
        maMnemonicActivateHdl = rLink;
    }

    bool callMnemonicActivateHdl(vcl::Window& rWindow)
    {
        return maMnemonicActivateHdl.Call(rWindow);
    }

    void setCommandHdl(const Link<const CommandEvent&, bool>& rLink) { maCommandHdl = rLink; }

    bool callCommandHdl(const CommandEvent& rCEvt) { return maCommandHdl.Call(rCEvt); }

    void setHelpRequestHdl(const Link<vcl::Window&, bool>& rLink) { maHelpRequestHdl = rLink; }

    bool callHelpRequestHdl(vcl::Window& rWindow)
    {
        return !maHelpRequestHdl.IsSet() || maHelpRequestHdl.Call(rWindow);
    }

private:
    std::vector<Link<VclWindowEvent&, void>> maEventListeners;
    int mnEventListenersIteratingCount = 0;
    std::set<Link<VclWindowEvent&, void>> maEventListenersDeleted;

    std::vector<Link<VclWindowEvent&, void>> maChildEventListeners;
    int mnChildEventListenersIteratingCount = 0;
    std::set<Link<VclWindowEvent&, void>> maChildEventListenersDeleted;

    Link<const CommandEvent&, bool> maCommandHdl;
    Link<vcl::Window&, bool> maHelpRequestHdl;
    Link<vcl::Window&, bool> maMnemonicActivateHdl;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
