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

#include <text.hxx>
#include <textlayout.hxx>

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
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
