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

namespace vcl
{
class Window;
}

struct WindowHierarchy
{
    VclPtr<vcl::Window> mpParent; // Parent (includes BorderWindow)
    VclPtr<vcl::Window> mpRealParent; // Real parent (excludes BorderWindow)

    VclPtr<vcl::Window> mpFirstChild; // First child window
    VclPtr<vcl::Window> mpLastChild; // Last child window

    VclPtr<vcl::Window> mpFirstOverlap; // First overlap window child
    VclPtr<vcl::Window> mpLastOverlap; // Last overlap window child

    VclPtr<vcl::Window> mpPrev; // Previous sibling window
    VclPtr<vcl::Window> mpNext; // Next sibling window

    VclPtr<vcl::Window> mpPrevOverlap; // Previous overlap window of frame
    VclPtr<vcl::Window> mpNextOverlap; // Next overlap window of frame

    VclPtr<vcl::Window> mpFrameWindow; // window that is the frame for this window
    VclPtr<vcl::Window> mpOverlapWindow; // first overlap parent
    VclPtr<vcl::Window> mpBorderWindow; // Border-Window
    VclPtr<vcl::Window> mpClientWindow; // Client-Window of a FrameWindow

    vcl::Window* getFirstOverlap() const { return mpFirstOverlap; }
    vcl::Window* getNext() const { return mpNext; }

    bool hasChildren() const { return mpFirstChild != nullptr; }

    vcl::Window* getParent() const { return mpParent.get(); }
    vcl::Window* getRealParent() const { return mpRealParent.get(); }
    vcl::Window* getBorderWindow() const { return mpBorderWindow.get(); }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
