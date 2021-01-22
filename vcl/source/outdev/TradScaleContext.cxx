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

#include <tools/gen.hxx>

#include "TradScaleContext.hxx"

TradScaleContext::TradScaleContext(tools::Rectangle const& aDstRect,
                                   tools::Rectangle const& aBitmapRect, Size const& aOutSize,
                                   tools::Long nOffX, tools::Long nOffY)
    : mpMapX(new tools::Long[aDstRect.GetWidth()])
    , mpMapY(new tools::Long[aDstRect.GetHeight()])
{
    const tools::Long nSrcWidth = aBitmapRect.GetWidth();
    const tools::Long nSrcHeight = aBitmapRect.GetHeight();

    const bool bHMirr = aOutSize.Width() < 0;
    const bool bVMirr = aOutSize.Height() < 0;

    generateSimpleMap(nSrcWidth, aDstRect.GetWidth(), aBitmapRect.Left(), aOutSize.Width(), nOffX,
                      bHMirr, mpMapX.get());

    generateSimpleMap(nSrcHeight, aDstRect.GetHeight(), aBitmapRect.Top(), aOutSize.Height(), nOffY,
                      bVMirr, mpMapY.get());
}

void TradScaleContext::generateSimpleMap(tools::Long nSrcDimension, tools::Long nDstDimension,
                                         tools::Long nDstLocation, tools::Long nOutDimension,
                                         tools::Long nOffset, bool bMirror, tools::Long* pMap)
{
    tools::Long nMirrorOffset = 0;

    if (bMirror)
        nMirrorOffset = (nDstLocation << 1) + nSrcDimension - 1;

    for (tools::Long i = 0; i < nDstDimension; ++i, ++nOffset)
    {
        pMap[i] = nDstLocation + nOffset * nSrcDimension / nOutDimension;
        if (bMirror)
            pMap[i] = nMirrorOffset - pMap[i];
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
