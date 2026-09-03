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

struct WindowEventHandlers
{
    std::vector<Link<VclWindowEvent&, void>> maEventListeners;
    int mnEventListenersIteratingCount = 0;
    std::set<Link<VclWindowEvent&, void>> maEventListenersDeleted;

    std::vector<Link<VclWindowEvent&, void>> maChildEventListeners;
    int mnChildEventListenersIteratingCount = 0;
    std::set<Link<VclWindowEvent&, void>> maChildEventListenersDeleted;

    Link<const CommandEvent&, bool> maCommandHdl;
    Link<vcl::Window&, bool> maHelpRequestHdl;
    Link<vcl::Window&, bool> maMnemonicActivateHdl;

    WindowEventHandlers() = default;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
