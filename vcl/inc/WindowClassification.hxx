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

#include <vcl/wintypes.hxx>

class WindowClassification
{
private:
    WindowClassification(const WindowClassification&) = delete;
    WindowClassification& operator=(const WindowClassification&) = delete;

public:
    WindowClassification(WindowType);
    ~WindowClassification();

    WindowType meType;
    bool mbFrame : 1 = false, mbBorderWin : 1 = false, mbOverlapWin : 1 = false,
                   mbSysWin : 1 = false, mbDialog : 1 = false, mbDockWin : 1 = false,
                   mbFloatWin : 1 = false, mbPushButton : 1 = false, mbTrackVisible : 1 = false,
                   mbAlwaysOnTop : 1 = false, mbInDispose : 1 = false,
                   mbCreatedWithToolkit : 1 = false, mbToolBox : 1 = false, mbSplitter : 1 = false,
                   mbMenuFloatingWindow : 1 = false, mbIsFormControl : 1 = false;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
