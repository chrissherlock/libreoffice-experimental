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

#include <tools/gen.hxx>
#include <tools/long.hxx>
#include <o3tl/deleter.hxx>
#include <o3tl/typed_flags_set.hxx>

#include <vcl/dllapi.h>
#include <vcl/settings.hxx>
#include <vcl/outdev.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/wall.hxx>

struct ImplFrameData;
namespace vcl
{
class Window;
}

namespace vcl
{
/// Sets up the buffer to have settings matching the window, and restores the original state in the dtor.
class VCL_DLLPUBLIC PaintBufferGuard
{
    ImplFrameData* mpFrameData;
    VclPtr<vcl::Window> m_pWindow;
    bool mbBackground;
    Wallpaper maBackground;
    AllSettings maSettings;
    tools::Long mnOutOffX;
    tools::Long mnOutOffY;
    tools::Rectangle m_aPaintRect;

public:
    PaintBufferGuard(ImplFrameData* pFrameData, vcl::Window* pWindow);
    ~PaintBufferGuard();
    /// If this is called, then the dtor will also copy rRectangle to the window from the buffer, before restoring the state.
    void SetPaintRect(const tools::Rectangle& rRectangle);
    /// Returns either the frame's buffer or the window, in case of no buffering.
    vcl::RenderContext* GetRenderContext();
};
typedef std::unique_ptr<PaintBufferGuard, o3tl::default_delete<PaintBufferGuard>>
    PaintBufferGuardPtr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
