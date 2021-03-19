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

#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>

#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawRect(tools::Rectangle const& rRect)
{
    assert(!is_double_buffered_window());

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaRectAction(rRect));

    if (ImplIsRecordLayout())
        return;

    RenderContext2::DrawRect(rRect);
}

void OutputDevice::DrawRect(tools::Rectangle const& rRect,
                             sal_uLong nHorzRound, sal_uLong nVertRound)
{
    assert(!is_double_buffered_window());

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaRoundRectAction(rRect, nHorzRound, nVertRound));

    if (ImplIsRecordLayout())
        return;

    RenderContext2::DrawRect(rRect, nHorzRound, nVertRound);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
