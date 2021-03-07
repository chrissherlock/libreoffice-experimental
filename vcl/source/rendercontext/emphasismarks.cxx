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

#include <osl/file.h>
#include <rtl/ustrbuf.hxx>
#include <tools/fontenum.hxx>
#include <i18nlangtag/lang.h>
#include <i18nlangtag/mslangid.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/font.hxx>

#include <textlayout.hxx>

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

static bool ImplIsCharIn(sal_Unicode c, char const* pStr)
{
    while (*pStr)
    {
        if (*pStr == c)
            return true;

        pStr++;
    }

    return false;
}

OUString RenderContext2::ImplGetEllipsisString(const RenderContext2& rTargetDevice,
                                               const OUString& rOrigStr, tools::Long nMaxWidth,
                                               DrawTextFlags nStyle,
                                               const vcl::ITextLayout& _rLayout)
{
    OUString aStr = rOrigStr;
    sal_Int32 nIndex = _rLayout.GetTextBreak(aStr, nMaxWidth, 0, aStr.getLength());

    if (nIndex != -1)
    {
        if ((nStyle & DrawTextFlags::CenterEllipsis) == DrawTextFlags::CenterEllipsis)
        {
            OUStringBuffer aTmpStr(aStr);
            // speed it up by removing all but 1.33x as many as the break pos.
            sal_Int32 nEraseChars = std::max<sal_Int32>(4, aStr.getLength() - (nIndex * 4) / 3);

            while (nEraseChars < aStr.getLength()
                   && _rLayout.GetTextWidth(aTmpStr.toString(), 0, aTmpStr.getLength()) > nMaxWidth)
            {
                aTmpStr = aStr;
                sal_Int32 i = (aTmpStr.getLength() - nEraseChars) / 2;
                aTmpStr.remove(i, nEraseChars++);
                aTmpStr.insert(i, "...");
            }

            aStr = aTmpStr.makeStringAndClear();
        }
        else if (nStyle & DrawTextFlags::EndEllipsis)
        {
            aStr = aStr.copy(0, nIndex);

            if (nIndex > 1)
            {
                aStr += "...";

                while (!aStr.isEmpty()
                       && (_rLayout.GetTextWidth(aStr, 0, aStr.getLength()) > nMaxWidth))
                {
                    if ((nIndex > 1) || (nIndex == aStr.getLength()))
                        nIndex--;

                    aStr = aStr.replaceAt(nIndex, 1, "");
                }
            }

            if (aStr.isEmpty() && (nStyle & DrawTextFlags::Clip))
                aStr += OUStringChar(rOrigStr[0]);
        }
        else if (nStyle & DrawTextFlags::PathEllipsis)
        {
            OUString aPath(rOrigStr);
            OUString aAbbreviatedPath;
            osl_abbreviateSystemPath(aPath.pData, &aAbbreviatedPath.pData, nIndex, nullptr);
            aStr = aAbbreviatedPath;
        }
        else if (nStyle & DrawTextFlags::NewsEllipsis)
        {
            static char const pSepChars[] = ".";

            // Determine last section
            sal_Int32 nLastContent = aStr.getLength();

            while (nLastContent)
            {
                nLastContent--;
                if (ImplIsCharIn(aStr[nLastContent], pSepChars))
                    break;
            }

            while (nLastContent && ImplIsCharIn(aStr[nLastContent - 1], pSepChars))
            {
                nLastContent--;
            }

            OUString aLastStr = aStr.copy(nLastContent);
            OUString aTempLastStr1 = "..." + aLastStr;

            if (_rLayout.GetTextWidth(aTempLastStr1, 0, aTempLastStr1.getLength()) > nMaxWidth)
            {
                aStr = RenderContext2::ImplGetEllipsisString(
                    rTargetDevice, aStr, nMaxWidth, nStyle | DrawTextFlags::EndEllipsis, _rLayout);
            }
            else
            {
                sal_Int32 nFirstContent = 0;
                while (nFirstContent < nLastContent)
                {
                    nFirstContent++;
                    if (ImplIsCharIn(aStr[nFirstContent], pSepChars))
                        break;
                }

                while ((nFirstContent < nLastContent)
                       && ImplIsCharIn(aStr[nFirstContent], pSepChars))
                {
                    nFirstContent++;
                }

                // MEM continue here
                if (nFirstContent >= nLastContent)
                {
                    aStr = RenderContext2::ImplGetEllipsisString(
                        rTargetDevice, aStr, nMaxWidth, nStyle | DrawTextFlags::EndEllipsis,
                        _rLayout);
                }
                else
                {
                    if (nFirstContent > 4)
                        nFirstContent = 4;

                    OUString aFirstStr = OUString::Concat(aStr.subView(0, nFirstContent)) + "...";
                    OUString aTempStr = aFirstStr + aLastStr;

                    if (_rLayout.GetTextWidth(aTempStr, 0, aTempStr.getLength()) > nMaxWidth)
                    {
                        aStr = RenderContext2::ImplGetEllipsisString(
                            rTargetDevice, aStr, nMaxWidth, nStyle | DrawTextFlags::EndEllipsis,
                            _rLayout);
                    }
                    else
                    {
                        do
                        {
                            aStr = aTempStr;

                            if (nLastContent > aStr.getLength())
                                nLastContent = aStr.getLength();

                            while (nFirstContent < nLastContent)
                            {
                                nLastContent--;
                                if (ImplIsCharIn(aStr[nLastContent], pSepChars))
                                    break;
                            }

                            while ((nFirstContent < nLastContent)
                                   && ImplIsCharIn(aStr[nLastContent - 1], pSepChars))
                            {
                                nLastContent--;
                            }

                            if (nFirstContent < nLastContent)
                            {
                                OUString aTempLastStr = aStr.copy(nLastContent);
                                aTempStr = aFirstStr + aTempLastStr;

                                if (_rLayout.GetTextWidth(aTempStr, 0, aTempStr.getLength())
                                    > nMaxWidth)
                                    break;
                            }
                        } while (nFirstContent < nLastContent);
                    }
                }
            }
        }
    }

    return aStr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
