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

#pragma once

#include <tools/gen.hxx>
#include <tools/long.hxx>

#include <vcl/region.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/window.hxx>

#include <window.h>

#include <memory>

class PaintHelper
{
private:
    VclPtr<vcl::Window> m_pWindow;
    std::unique_ptr<vcl::Region> m_pChildRegion;
    tools::Rectangle m_aSelectionRect;
    tools::Rectangle m_aPaintRect;
    vcl::Region m_aPaintRegion;
    ImplPaintFlags m_nPaintFlags;
    bool m_bPop : 1;
    bool m_bRestoreCursor : 1;
    bool
        m_bStartedBufferedPaint : 1; ///< This PaintHelper started a buffered paint, and should paint it on the screen when being destructed.
public:
    PaintHelper(vcl::Window* pWindow, ImplPaintFlags nPaintFlags);
    void SetPop() { m_bPop = true; }
    void SetPaintRect(const tools::Rectangle& rRect) { m_aPaintRect = rRect; }
    void SetSelectionRect(const tools::Rectangle& rRect) { m_aSelectionRect = rRect; }
    void SetRestoreCursor(bool bRestoreCursor) { m_bRestoreCursor = bRestoreCursor; }
    bool GetRestoreCursor() const { return m_bRestoreCursor; }
    ImplPaintFlags GetPaintFlags() const { return m_nPaintFlags; }
    vcl::Region& GetPaintRegion() { return m_aPaintRegion; }
    void DoPaint(const vcl::Region* pRegion);

    /// Start buffered paint: set it up to have the same settings as m_pWindow.
    void StartBufferedPaint();

    /// Paint the content of the buffer to the current m_pWindow.
    void PaintBuffer();

    ~PaintHelper();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
