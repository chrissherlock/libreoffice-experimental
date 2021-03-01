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

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

RenderContext2::RenderContext2()
    : mpGraphics(nullptr)
    , mpAlphaVDev(nullptr)
    , mnDrawMode(DrawModeFlags::Default)
    , mbInitFont(true)
    , mbOutput(true)
{
    // #i84553 toop BiDi preference to RTL
    if (AllSettings::GetLayoutRTL())
        mnTextLayoutMode = ComplexTextLayoutFlags::BiDiRtl | ComplexTextLayoutFlags::TextOriginLeft;
    else
        mnTextLayoutMode = ComplexTextLayoutFlags::Default;
}

RenderContext2::~RenderContext2() { disposeOnce(); }

void RenderContext2::dispose()
{
    mpAlphaVDev.disposeAndClear();
    VclReferenceBase::dispose();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
