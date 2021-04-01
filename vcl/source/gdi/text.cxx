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

#include <sal/log.hxx>
#include <osl/file.h>
#include <rtl/ustrbuf.hxx>
#include <tools/gen.hxx>
#include <comphelper/processfactory.hxx>
#include <i18nlangtag/languagetag.hxx>

#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/unohelp.hxx>

#include <ImplMultiTextLineInfo.hxx>
#include <text.hxx>
#include <textlayout.hxx>
#include <textlineinfo.hxx>

#include <com/sun/star/i18n/WordType.hpp>
#include <com/sun/star/i18n/XBreakIterator.hpp>
#include <com/sun/star/linguistic2/LinguServiceManager.hpp>
#include <com/sun/star/linguistic2/XHyphenator.hpp>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/XComponentContext.hpp>

tools::Long ImplGetTextLines(ImplMultiTextLineInfo& rLineInfo, tools::Long nWidth,
                             OUString const& rStr, DrawTextFlags nStyle,
                             vcl::ITextLayout const& _rLayout)
{
    SAL_WARN_IF(nWidth <= 0, "vcl", "ImplGetTextLines: nWidth <= 0!");

    if (nWidth <= 0)
        nWidth = 1;

    tools::Long nMaxLineWidth = 0;
    rLineInfo.Clear();
    if (!rStr.isEmpty())
    {
        const bool bHyphenate
            = (nStyle & DrawTextFlags::WordBreakHyphenation) == DrawTextFlags::WordBreakHyphenation;
        css::uno::Reference<css::linguistic2::XHyphenator> xHyph;
        if (bHyphenate)
        {
            // get service provider
            css::uno::Reference<css::uno::XComponentContext> xContext(
                comphelper::getProcessComponentContext());
            css::uno::Reference<css::linguistic2::XLinguServiceManager2> xLinguMgr
                = css::linguistic2::LinguServiceManager::create(xContext);
            xHyph = xLinguMgr->getHyphenator();
        }

        css::uno::Reference<css::i18n::XBreakIterator> xBI;
        sal_Int32 nPos = 0;
        sal_Int32 nLen = rStr.getLength();
        while (nPos < nLen)
        {
            sal_Int32 nBreakPos = nPos;

            while ((nBreakPos < nLen) && (rStr[nBreakPos] != '\r') && (rStr[nBreakPos] != '\n'))
                nBreakPos++;

            tools::Long nLineWidth = _rLayout.GetTextWidth(rStr, nPos, nBreakPos - nPos);
            if ((nLineWidth > nWidth) && (nStyle & DrawTextFlags::WordBreak))
            {
                if (!xBI.is())
                    xBI = vcl::unohelper::CreateBreakIterator();

                if (xBI.is())
                {
                    const css::lang::Locale& rDefLocale(
                        Application::GetSettings().GetUILanguageTag().getLocale());
                    sal_Int32 nSoftBreak
                        = _rLayout.GetTextBreak(rStr, nWidth, nPos, nBreakPos - nPos);
                    if (nSoftBreak == -1)
                    {
                        nSoftBreak = nPos;
                    }
                    SAL_WARN_IF(nSoftBreak >= nBreakPos, "vcl", "Break?!");
                    css::i18n::LineBreakHyphenationOptions aHyphOptions(
                        xHyph, css::uno::Sequence<css::beans::PropertyValue>(), 1);
                    css::i18n::LineBreakUserOptions aUserOptions;
                    css::i18n::LineBreakResults aLBR = xBI->getLineBreak(
                        rStr, nSoftBreak, rDefLocale, nPos, aHyphOptions, aUserOptions);
                    nBreakPos = aLBR.breakIndex;
                    if (nBreakPos <= nPos)
                        nBreakPos = nSoftBreak;
                    if (bHyphenate)
                    {
                        // Whether hyphen or not: Put the word after the hyphen through
                        // word boundary.

                        // nMaxBreakPos the last char that fits into the line
                        // nBreakPos is the word's start

                        // We run into a problem if the doc is so narrow, that a word
                        // is broken into more than two lines ...
                        if (xHyph.is())
                        {
                            css::i18n::Boundary aBoundary
                                = xBI->getWordBoundary(rStr, nBreakPos, rDefLocale,
                                                       css::i18n::WordType::DICTIONARY_WORD, true);
                            sal_Int32 nWordStart = nPos;
                            sal_Int32 nWordEnd = aBoundary.endPos;
                            SAL_WARN_IF(nWordEnd <= nWordStart, "vcl",
                                        "ImpBreakLine: Start >= End?");

                            sal_Int32 nWordLen = nWordEnd - nWordStart;
                            if ((nWordEnd >= nSoftBreak) && (nWordLen > 3))
                            {
                                // #104415# May happen, because getLineBreak may differ from getWordBoundary with DICTIONARY_WORD
                                // SAL_WARN_IF( nWordEnd < nMaxBreakPos, "vcl", "Hyph: Break?" );
                                OUString aWord = rStr.copy(nWordStart, nWordLen);
                                sal_Int32 nMinTrail
                                    = nWordEnd - nSoftBreak + 1; //+1: Before the "broken off" char
                                css::uno::Reference<css::linguistic2::XHyphenatedWord> xHyphWord;
                                if (xHyph.is())
                                    xHyphWord = xHyph->hyphenate(
                                        aWord, rDefLocale, aWord.getLength() - nMinTrail,
                                        css::uno::Sequence<css::beans::PropertyValue>());
                                if (xHyphWord.is())
                                {
                                    bool bAlternate = xHyphWord->isAlternativeSpelling();
                                    sal_Int32 _nWordLen = 1 + xHyphWord->getHyphenPos();

                                    if ((_nWordLen >= 2) && ((nWordStart + _nWordLen) >= 2))
                                    {
                                        if (!bAlternate)
                                        {
                                            nBreakPos = nWordStart + _nWordLen;
                                        }
                                        else
                                        {
                                            OUString aAlt(xHyphWord->getHyphenatedWord());

                                            // We can have two cases:
                                            // 1) "packen" turns into "pak-ken"
                                            // 2) "Schiffahrt" turns into "Schiff-fahrt"

                                            // In case 1 we need to replace a char
                                            // In case 2 we add a char

                                            // Correct recognition is made harder by words such as
                                            // "Schiffahrtsbrennesseln", as the Hyphenator splits all
                                            // positions of the word and comes up with "Schifffahrtsbrennnesseln"
                                            // Thus, we cannot infer the aWord from the AlternativeWord's
                                            // index.
                                            // TODO: The whole junk will be made easier by a function in
                                            // the Hyphenator, as soon as AMA adds it.
                                            sal_Int32 nAltStart = _nWordLen - 1;
                                            sal_Int32 nTxtStart
                                                = nAltStart
                                                  - (aAlt.getLength() - aWord.getLength());
                                            sal_Int32 nTxtEnd = nTxtStart;
                                            sal_Int32 nAltEnd = nAltStart;

                                            // The area between nStart and nEnd is the difference
                                            // between AlternativeString and OriginalString
                                            while (nTxtEnd < aWord.getLength()
                                                   && nAltEnd < aAlt.getLength()
                                                   && aWord[nTxtEnd] != aAlt[nAltEnd])
                                            {
                                                ++nTxtEnd;
                                                ++nAltEnd;
                                            }

                                            // If a char was added, we notice it now:
                                            if (nAltEnd > nTxtEnd && nAltStart == nAltEnd
                                                && aWord[nTxtEnd] == aAlt[nAltEnd])
                                            {
                                                ++nAltEnd;
                                                ++nTxtStart;
                                                ++nTxtEnd;
                                            }

                                            SAL_WARN_IF((nAltEnd - nAltStart) != 1, "vcl",
                                                        "Alternate: Wrong assumption!");

                                            sal_Unicode cAlternateReplChar = 0;
                                            if (nTxtEnd > nTxtStart)
                                                cAlternateReplChar = aAlt[nAltStart];

                                            nBreakPos = nWordStart + nTxtStart;
                                            if (cAlternateReplChar)
                                                nBreakPos++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    nLineWidth = _rLayout.GetTextWidth(rStr, nPos, nBreakPos - nPos);
                }
                else
                {
                    // fallback to something really simple
                    sal_Int32 nSpacePos = rStr.getLength();
                    tools::Long nW = 0;
                    do
                    {
                        nSpacePos = rStr.lastIndexOf(' ', nSpacePos);
                        if (nSpacePos != -1)
                        {
                            if (nSpacePos > nPos)
                                nSpacePos--;
                            nW = _rLayout.GetTextWidth(rStr, nPos, nSpacePos - nPos);
                        }
                    } while (nW > nWidth);

                    if (nSpacePos != -1)
                    {
                        nBreakPos = nSpacePos;
                        nLineWidth = _rLayout.GetTextWidth(rStr, nPos, nBreakPos - nPos);
                        if (nBreakPos < rStr.getLength() - 1)
                            nBreakPos++;
                    }
                }
            }

            if (nLineWidth > nMaxLineWidth)
                nMaxLineWidth = nLineWidth;

            rLineInfo.AddLine(new ImplTextLineInfo(nLineWidth, nPos, nBreakPos - nPos));

            if (nBreakPos == nPos)
                nBreakPos++;
            nPos = nBreakPos;

            if (nPos < nLen && ((rStr[nPos] == '\r') || (rStr[nPos] == '\n')))
            {
                nPos++;
                // CR/LF?
                if ((nPos < nLen) && (rStr[nPos] == '\n') && (rStr[nPos - 1] == '\r'))
                    nPos++;
            }
        }
    }
#ifdef DBG_UTIL
    for (sal_Int32 nL = 0; nL < rLineInfo.Count(); nL++)
    {
        ImplTextLineInfo* pLine = rLineInfo.GetLine(nL);
        OUString aLine = rStr.copy(pLine->GetIndex(), pLine->GetLen());
        SAL_WARN_IF(aLine.indexOf('\r') != -1, "vcl", "ImplGetTextLines - Found CR!");
        SAL_WARN_IF(aLine.indexOf('\n') != -1, "vcl", "ImplGetTextLines - Found LF!");
    }
#endif

    return nMaxLineWidth;
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

OUString ImplGetEllipsisString(OUString const& rOrigStr, tools::Long nMaxWidth,
                               DrawTextFlags nStyle, vcl::ITextLayout const& _rLayout)
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
                aStr = ImplGetEllipsisString(aStr, nMaxWidth, nStyle | DrawTextFlags::EndEllipsis,
                                             _rLayout);
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
                    aStr = ImplGetEllipsisString(aStr, nMaxWidth,
                                                 nStyle | DrawTextFlags::EndEllipsis, _rLayout);
                }
                else
                {
                    if (nFirstContent > 4)
                        nFirstContent = 4;

                    OUString aFirstStr = OUString::Concat(aStr.subView(0, nFirstContent)) + "...";
                    OUString aTempStr = aFirstStr + aLastStr;

                    if (_rLayout.GetTextWidth(aTempStr, 0, aTempStr.getLength()) > nMaxWidth)
                    {
                        aStr = ImplGetEllipsisString(aStr, nMaxWidth,
                                                     nStyle | DrawTextFlags::EndEllipsis, _rLayout);
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
