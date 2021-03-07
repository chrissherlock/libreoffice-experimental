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

#include <tools/fontenum.hxx>
#include <i18nlangtag/lang.h>
#include <i18nlangtag/mslangid.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/font.hxx>

void RenderContext2::ImplGetEmphasisMark(tools::PolyPolygon& rPolyPoly, bool& rPolyLine,
                                         tools::Rectangle& rRect1, tools::Rectangle& rRect2,
                                         tools::Long& rYOff, tools::Long& rWidth,
                                         FontEmphasisMark eEmphasis, tools::Long nHeight)
{
    static const PolyFlags aAccentPolyFlags[24]
        = { PolyFlags::Normal,  PolyFlags::Control, PolyFlags::Control, PolyFlags::Normal,
            PolyFlags::Control, PolyFlags::Control, PolyFlags::Normal,  PolyFlags::Control,
            PolyFlags::Control, PolyFlags::Normal,  PolyFlags::Control, PolyFlags::Control,
            PolyFlags::Normal,  PolyFlags::Control, PolyFlags::Control, PolyFlags::Normal,
            PolyFlags::Control, PolyFlags::Control, PolyFlags::Normal,  PolyFlags::Normal,
            PolyFlags::Control, PolyFlags::Normal,  PolyFlags::Control, PolyFlags::Control };

    static const Point aAccentPos[24]
        = { { 78, 0 },    { 348, 79 },  { 599, 235 }, { 843, 469 }, { 938, 574 }, { 990, 669 },
            { 990, 773 }, { 990, 843 }, { 964, 895 }, { 921, 947 }, { 886, 982 }, { 860, 999 },
            { 825, 999 }, { 764, 999 }, { 721, 964 }, { 686, 895 }, { 625, 791 }, { 556, 660 },
            { 469, 504 }, { 400, 400 }, { 261, 252 }, { 61, 61 },   { 0, 27 },    { 9, 0 } };

    rWidth = 0;
    rYOff = 0;
    rPolyLine = false;

    if (!nHeight)
        return;

    FontEmphasisMark nEmphasisStyle = eEmphasis & FontEmphasisMark::Style;
    tools::Long nDotSize = 0;
    switch (nEmphasisStyle)
    {
        case FontEmphasisMark::Dot:
            // Dot has 55% of the height
            nDotSize = (nHeight * 550) / 1000;

            if (!nDotSize)
                nDotSize = 1;

            if (nDotSize <= 2)
            {
                rRect1 = tools::Rectangle(Point(), Size(nDotSize, nDotSize));
            }
            else
            {
                tools::Long nRad = nDotSize / 2;
                tools::Polygon aPoly(Point(nRad, nRad), nRad, nRad);
                rPolyPoly.Insert(aPoly);
            }

            rYOff = ((nHeight * 250) / 1000) / 2; // Center to the another EmphasisMarks
            rWidth = nDotSize;
            break;

        case FontEmphasisMark::Circle:
            // Dot has 80% of the height
            nDotSize = (nHeight * 800) / 1000;
            if (!nDotSize)
                nDotSize = 1;

            if (nDotSize <= 2)
            {
                rRect1 = tools::Rectangle(Point(), Size(nDotSize, nDotSize));
            }
            else
            {
                tools::Long nRad = nDotSize / 2;
                tools::Polygon aPoly(Point(nRad, nRad), nRad, nRad);
                rPolyPoly.Insert(aPoly);
                // BorderWidth is 15%
                tools::Long nBorder = (nDotSize * 150) / 1000;
                if (nBorder <= 1)
                {
                    rPolyLine = true;
                }
                else
                {
                    tools::Polygon aPoly2(Point(nRad, nRad), nRad - nBorder, nRad - nBorder);
                    rPolyPoly.Insert(aPoly2);
                }
            }
            rWidth = nDotSize;
            break;

        case FontEmphasisMark::Disc:
            // Dot has 80% of the height
            nDotSize = (nHeight * 800) / 1000;
            if (!nDotSize)
                nDotSize = 1;

            if (nDotSize <= 2)
            {
                rRect1 = tools::Rectangle(Point(), Size(nDotSize, nDotSize));
            }
            else
            {
                tools::Long nRad = nDotSize / 2;
                tools::Polygon aPoly(Point(nRad, nRad), nRad, nRad);
                rPolyPoly.Insert(aPoly);
            }

            rWidth = nDotSize;
            break;

        case FontEmphasisMark::Accent:
            // Dot has 80% of the height
            nDotSize = (nHeight * 800) / 1000;

            if (!nDotSize)
                nDotSize = 1;

            if (nDotSize <= 2)
            {
                if (nDotSize == 1)
                {
                    rRect1 = tools::Rectangle(Point(), Size(nDotSize, nDotSize));
                    rWidth = nDotSize;
                }
                else
                {
                    rRect1 = tools::Rectangle(Point(), Size(1, 1));
                    rRect2 = tools::Rectangle(Point(1, 1), Size(1, 1));
                }
            }
            else
            {
                tools::Polygon aPoly(SAL_N_ELEMENTS(aAccentPos), aAccentPos, aAccentPolyFlags);
                double dScale = static_cast<double>(nDotSize) / 1000.0;
                aPoly.Scale(dScale, dScale);
                tools::Polygon aTemp;
                aPoly.AdaptiveSubdivide(aTemp);
                tools::Rectangle aBoundRect = aTemp.GetBoundRect();
                rWidth = aBoundRect.GetWidth();
                nDotSize = aBoundRect.GetHeight();
                rPolyPoly.Insert(aTemp);
            }
            break;

        default:
            break;
    }

    // calculate position
    tools::Long nOffY = 1 + (GetDPIY() / 300); // one visible pixel space
    tools::Long nSpaceY = nHeight - nDotSize;

    if (nSpaceY >= nOffY * 2)
        rYOff += nOffY;

    if (!(eEmphasis & FontEmphasisMark::PosBelow))
        rYOff += nDotSize;
}

FontEmphasisMark RenderContext2::ImplGetEmphasisMarkStyle(vcl::Font const& rFont)
{
    FontEmphasisMark nEmphasisMark = rFont.GetEmphasisMark();

    // If no Position is set, then calculate the default position, which
    // depends on the language
    if (!(nEmphasisMark & (FontEmphasisMark::PosAbove | FontEmphasisMark::PosBelow)))
    {
        LanguageType eLang = rFont.GetLanguage();
        // In Chinese Simplified the EmphasisMarks are below/left
        if (MsLangId::isSimplifiedChinese(eLang))
        {
            nEmphasisMark |= FontEmphasisMark::PosBelow;
        }
        else
        {
            eLang = rFont.GetCJKContextLanguage();
            // In Chinese Simplified the EmphasisMarks are below/left
            if (MsLangId::isSimplifiedChinese(eLang))
                nEmphasisMark |= FontEmphasisMark::PosBelow;
            else
                nEmphasisMark |= FontEmphasisMark::PosAbove;
        }
    }

    return nEmphasisMark;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
