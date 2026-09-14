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

#include <vcl/vclptr.hxx>

#include <svdata.hxx>

namespace vcl
{
class Window;
}

class HandleGestureEventBase
{
protected:
    ImplSVData* m_pSVData;
    VclPtr<vcl::Window> m_pWindow;
    Point m_aMousePos;

public:
    HandleGestureEventBase(vcl::Window* pWindow, const Point& rMousePos)
        : m_pSVData(ImplGetSVData())
        , m_pWindow(pWindow)
        , m_aMousePos(rMousePos)
    {
    }
    bool Setup();
    vcl::Window* FindTarget();
    vcl::Window* Dispatch(vcl::Window* pTarget);
    virtual bool CallCommand(vcl::Window* pWindow, const Point& rMousePos) = 0;
    virtual ~HandleGestureEventBase() {}

    static bool IsAcceptableWheelScrollTarget(const vcl::Window* pMouseWindow);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
