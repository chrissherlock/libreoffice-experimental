
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/window.hxx>

#include "ClipState.hxx"

namespace vcl::clipping
{
class VCL_DLLPUBLIC ClipStateBuilder
{
public:
    // This is the factory method that performs the conversion
    static ClipState BuildFromWindow(const vcl::Window& rWindow);

    static ClipState Build(vcl::Window& rWindow);

private:
    // Helper to flatten linked-list pointers into ClipNode vectors
    static void CollectSiblings(const vcl::Window& rWindow, std::vector<ClipNode>& rOut);
    static void CollectChildren(const vcl::Window& rWindow, std::vector<ClipNode>& rOut);
};

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
