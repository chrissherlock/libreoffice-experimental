/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

struct WindowVisibilityState
{
    bool mbVisible : 1 = false;
    bool mbReallyVisible : 1 = false;
    bool mbReallyShown : 1 = false;
    bool mbInInitShow : 1 = false;
    bool mbOverlapVisible : 1 = false;

    WindowVisibilityState();
    ~WindowVisibilityState();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
