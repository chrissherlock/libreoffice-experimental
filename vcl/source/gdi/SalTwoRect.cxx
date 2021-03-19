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

#include <tools/helpers.hxx>

#include <vcl/salgtype.hxx>

BmpMirrorFlags AdjustTwoRect(SalTwoRect& rTwoRect, Size const& rSizePix)
{
    BmpMirrorFlags nMirrFlags = BmpMirrorFlags::NONE;

    if (rTwoRect.mnDestWidth < 0)
    {
        rTwoRect.mnSrcX = rSizePix.Width() - rTwoRect.mnSrcX - rTwoRect.mnSrcWidth;
        rTwoRect.mnDestWidth = -rTwoRect.mnDestWidth;
        rTwoRect.mnDestX -= rTwoRect.mnDestWidth - 1;
        nMirrFlags |= BmpMirrorFlags::Horizontal;
    }

    if (rTwoRect.mnDestHeight < 0)
    {
        rTwoRect.mnSrcY = rSizePix.Height() - rTwoRect.mnSrcY - rTwoRect.mnSrcHeight;
        rTwoRect.mnDestHeight = -rTwoRect.mnDestHeight;
        rTwoRect.mnDestY -= rTwoRect.mnDestHeight - 1;
        nMirrFlags |= BmpMirrorFlags::Vertical;
    }

    if ((rTwoRect.mnSrcX < 0) || (rTwoRect.mnSrcX >= rSizePix.Width()) || (rTwoRect.mnSrcY < 0)
        || (rTwoRect.mnSrcY >= rSizePix.Height())
        || ((rTwoRect.mnSrcX + rTwoRect.mnSrcWidth) > rSizePix.Width())
        || ((rTwoRect.mnSrcY + rTwoRect.mnSrcHeight) > rSizePix.Height()))
    {
        const tools::Rectangle aSourceRect(Point(rTwoRect.mnSrcX, rTwoRect.mnSrcY),
                                           Size(rTwoRect.mnSrcWidth, rTwoRect.mnSrcHeight));
        tools::Rectangle aCropRect(aSourceRect);

        aCropRect.Intersection(tools::Rectangle(Point(), rSizePix));

        if (aCropRect.IsEmpty())
        {
            rTwoRect.mnSrcWidth = rTwoRect.mnSrcHeight = rTwoRect.mnDestWidth
                = rTwoRect.mnDestHeight = 0;
        }
        else
        {
            const double fFactorX
                = (rTwoRect.mnSrcWidth > 1)
                      ? static_cast<double>(rTwoRect.mnDestWidth - 1) / (rTwoRect.mnSrcWidth - 1)
                      : 0.0;
            const double fFactorY
                = (rTwoRect.mnSrcHeight > 1)
                      ? static_cast<double>(rTwoRect.mnDestHeight - 1) / (rTwoRect.mnSrcHeight - 1)
                      : 0.0;

            const tools::Long nDstX1
                = rTwoRect.mnDestX + FRound(fFactorX * (aCropRect.Left() - rTwoRect.mnSrcX));
            const tools::Long nDstY1
                = rTwoRect.mnDestY + FRound(fFactorY * (aCropRect.Top() - rTwoRect.mnSrcY));
            const tools::Long nDstX2
                = rTwoRect.mnDestX + FRound(fFactorX * (aCropRect.Right() - rTwoRect.mnSrcX));
            const tools::Long nDstY2
                = rTwoRect.mnDestY + FRound(fFactorY * (aCropRect.Bottom() - rTwoRect.mnSrcY));

            rTwoRect.mnSrcX = aCropRect.Left();
            rTwoRect.mnSrcY = aCropRect.Top();
            rTwoRect.mnSrcWidth = aCropRect.GetWidth();
            rTwoRect.mnSrcHeight = aCropRect.GetHeight();
            rTwoRect.mnDestX = nDstX1;
            rTwoRect.mnDestY = nDstY1;
            rTwoRect.mnDestWidth = nDstX2 - nDstX1 + 1;
            rTwoRect.mnDestHeight = nDstY2 - nDstY1 + 1;
        }
    }

    return nMirrFlags;
}

void AdjustTwoRect(SalTwoRect& rTwoRect, tools::Rectangle const& rValidSrcRect)
{
    if (!((rTwoRect.mnSrcX < rValidSrcRect.Left()) || (rTwoRect.mnSrcX >= rValidSrcRect.Right())
          || (rTwoRect.mnSrcY < rValidSrcRect.Top()) || (rTwoRect.mnSrcY >= rValidSrcRect.Bottom())
          || ((rTwoRect.mnSrcX + rTwoRect.mnSrcWidth) > rValidSrcRect.Right())
          || ((rTwoRect.mnSrcY + rTwoRect.mnSrcHeight) > rValidSrcRect.Bottom())))
        return;

    const tools::Rectangle aSourceRect(Point(rTwoRect.mnSrcX, rTwoRect.mnSrcY),
                                       Size(rTwoRect.mnSrcWidth, rTwoRect.mnSrcHeight));
    tools::Rectangle aCropRect(aSourceRect);

    aCropRect.Intersection(rValidSrcRect);

    if (aCropRect.IsEmpty())
    {
        rTwoRect.mnSrcWidth = rTwoRect.mnSrcHeight = rTwoRect.mnDestWidth = rTwoRect.mnDestHeight
            = 0;
    }
    else
    {
        const double fFactorX
            = (rTwoRect.mnSrcWidth > 1)
                  ? static_cast<double>(rTwoRect.mnDestWidth - 1) / (rTwoRect.mnSrcWidth - 1)
                  : 0.0;
        const double fFactorY
            = (rTwoRect.mnSrcHeight > 1)
                  ? static_cast<double>(rTwoRect.mnDestHeight - 1) / (rTwoRect.mnSrcHeight - 1)
                  : 0.0;

        const tools::Long nDstX1
            = rTwoRect.mnDestX + FRound(fFactorX * (aCropRect.Left() - rTwoRect.mnSrcX));
        const tools::Long nDstY1
            = rTwoRect.mnDestY + FRound(fFactorY * (aCropRect.Top() - rTwoRect.mnSrcY));
        const tools::Long nDstX2
            = rTwoRect.mnDestX + FRound(fFactorX * (aCropRect.Right() - rTwoRect.mnSrcX));
        const tools::Long nDstY2
            = rTwoRect.mnDestY + FRound(fFactorY * (aCropRect.Bottom() - rTwoRect.mnSrcY));

        rTwoRect.mnSrcX = aCropRect.Left();
        rTwoRect.mnSrcY = aCropRect.Top();
        rTwoRect.mnSrcWidth = aCropRect.GetWidth();
        rTwoRect.mnSrcHeight = aCropRect.GetHeight();
        rTwoRect.mnDestX = nDstX1;
        rTwoRect.mnDestY = nDstY1;
        rTwoRect.mnDestWidth = nDstX2 - nDstX1 + 1;
        rTwoRect.mnDestHeight = nDstY2 - nDstY1 + 1;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
