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

#include <i18nlangtag/lang.h>

#include <vcl/ImplLayoutRuns.hxx>
#include <vcl/vclenum.hxx>

#include <unicode/uchar.h>

ImplLayoutRuns::ImplLayoutRuns()
{
    mnRunIndex = 0;
    maRuns.reserve(8);
}

void ImplLayoutRuns::Clear() { maRuns.clear(); }

bool ImplLayoutRuns::IsEmpty() const { return maRuns.empty(); }

void ImplLayoutRuns::ResetPos() { mnRunIndex = 0; }

void ImplLayoutRuns::NextRun() { mnRunIndex += 2; }

sal_UCS4 GetMirroredChar(sal_UCS4 nChar)
{
    nChar = u_charMirror(nChar);
    return nChar;
}

sal_UCS4 GetLocalizedChar(sal_UCS4 nChar, LanguageType eLang)
{
    // currently only conversion from ASCII digits is interesting
    if ((nChar < '0') || ('9' < nChar))
        return nChar;

    int nOffset;
    // eLang & LANGUAGE_MASK_PRIMARY catches language independent of region.
    // CAVEAT! To some like Mongolian MS assigned the same primary language
    // although the script type is different!
    LanguageType pri = primary(eLang);
    if (pri == primary(LANGUAGE_ARABIC_SAUDI_ARABIA))
        nOffset = 0x0660 - '0'; // arabic-indic digits
    else if (pri.anyOf(primary(LANGUAGE_FARSI), primary(LANGUAGE_URDU_PAKISTAN),
                       primary(LANGUAGE_PUNJABI), //???
                       primary(LANGUAGE_SINDHI)))
        nOffset = 0x06F0 - '0'; // eastern arabic-indic digits
    else if (pri == primary(LANGUAGE_BENGALI))
        nOffset = 0x09E6 - '0'; // bengali
    else if (pri == primary(LANGUAGE_HINDI))
        nOffset = 0x0966 - '0'; // devanagari
    else if (pri.anyOf(primary(LANGUAGE_AMHARIC_ETHIOPIA), primary(LANGUAGE_TIGRIGNA_ETHIOPIA)))
        // TODO case:
        nOffset = 0x1369 - '0'; // ethiopic
    else if (pri == primary(LANGUAGE_GUJARATI))
        nOffset = 0x0AE6 - '0'; // gujarati
#ifdef LANGUAGE_GURMUKHI // TODO case:
    else if (pri == primary(LANGUAGE_GURMUKHI))
        nOffset = 0x0A66 - '0'; // gurmukhi
#endif
    else if (pri == primary(LANGUAGE_KANNADA))
        nOffset = 0x0CE6 - '0'; // kannada
    else if (pri == primary(LANGUAGE_KHMER))
        nOffset = 0x17E0 - '0'; // khmer
    else if (pri == primary(LANGUAGE_LAO))
        nOffset = 0x0ED0 - '0'; // lao
    else if (pri == primary(LANGUAGE_MALAYALAM))
        nOffset = 0x0D66 - '0'; // malayalam
    else if (pri == primary(LANGUAGE_MONGOLIAN_MONGOLIAN_LSO))
    {
        if (eLang.anyOf(LANGUAGE_MONGOLIAN_MONGOLIAN_MONGOLIA, LANGUAGE_MONGOLIAN_MONGOLIAN_CHINA,
                        LANGUAGE_MONGOLIAN_MONGOLIAN_LSO))
            nOffset = 0x1810 - '0'; // mongolian
        else
            nOffset = 0; // mongolian cyrillic
    }
    else if (pri == primary(LANGUAGE_BURMESE))
        nOffset = 0x1040 - '0'; // myanmar
    else if (pri == primary(LANGUAGE_ODIA))
        nOffset = 0x0B66 - '0'; // odia
    else if (pri == primary(LANGUAGE_TAMIL))
        nOffset = 0x0BE7 - '0'; // tamil
    else if (pri == primary(LANGUAGE_TELUGU))
        nOffset = 0x0C66 - '0'; // telugu
    else if (pri == primary(LANGUAGE_THAI))
        nOffset = 0x0E50 - '0'; // thai
    else if (pri == primary(LANGUAGE_TIBETAN))
        nOffset = 0x0F20 - '0'; // tibetan
    else
    {
        nOffset = 0;
    }

    nChar += nOffset;
    return nChar;
}

void ImplLayoutRuns::AddPos(int nCharPos, bool bRTL)
{
    // check if charpos could extend current run
    int nIndex = maRuns.size();
    if (nIndex >= 2)
    {
        int nRunPos0 = maRuns[nIndex - 2];
        int nRunPos1 = maRuns[nIndex - 1];
        if (((nCharPos + int(bRTL)) == nRunPos1) && ((nRunPos0 > nRunPos1) == bRTL))
        {
            // extend current run by new charpos
            maRuns[nIndex - 1] = nCharPos + int(!bRTL);
            return;
        }
        // ignore new charpos when it is in current run
        if ((nRunPos0 <= nCharPos) && (nCharPos < nRunPos1))
            return;
        if ((nRunPos1 <= nCharPos) && (nCharPos < nRunPos0))
            return;
    }

    // else append a new run consisting of the new charpos
    maRuns.push_back(nCharPos + (bRTL ? 1 : 0));
    maRuns.push_back(nCharPos + (bRTL ? 0 : 1));
}

void ImplLayoutRuns::AddRun(int nCharPos0, int nCharPos1, bool bRTL)
{
    if (nCharPos0 == nCharPos1)
        return;

    // swap if needed
    if (bRTL == (nCharPos0 < nCharPos1))
    {
        int nTemp = nCharPos0;
        nCharPos0 = nCharPos1;
        nCharPos1 = nTemp;
    }

    if (maRuns.size() >= 2 && nCharPos0 == maRuns[maRuns.size() - 2]
        && nCharPos1 == maRuns[maRuns.size() - 1])
    {
        //this run is the same as the last
        return;
    }

    // append new run
    maRuns.push_back(nCharPos0);
    maRuns.push_back(nCharPos1);
}

bool ImplLayoutRuns::PosIsInRun(int nCharPos) const
{
    if (mnRunIndex >= static_cast<int>(maRuns.size()))
        return false;

    int nMinCharPos = maRuns[mnRunIndex + 0];
    int nEndCharPos = maRuns[mnRunIndex + 1];
    if (nMinCharPos > nEndCharPos) // reversed in RTL case
    {
        int nTemp = nMinCharPos;
        nMinCharPos = nEndCharPos;
        nEndCharPos = nTemp;
    }

    if (nCharPos < nMinCharPos)
        return false;
    if (nCharPos >= nEndCharPos)
        return false;
    return true;
}

bool ImplLayoutRuns::PosIsInAnyRun(int nCharPos) const
{
    bool bRet = false;
    int nRunIndex = mnRunIndex;

    ImplLayoutRuns* pThis = const_cast<ImplLayoutRuns*>(this);

    pThis->ResetPos();

    for (size_t i = 0; i < maRuns.size(); i += 2)
    {
        bRet = PosIsInRun(nCharPos);
        if (bRet)
            break;
        pThis->NextRun();
    }

    pThis->mnRunIndex = nRunIndex;
    return bRet;
}

bool ImplLayoutRuns::GetNextPos(int* nCharPos, bool* bRightToLeft)
{
    // negative nCharPos => reset to first run
    if (*nCharPos < 0)
        mnRunIndex = 0;

    // return false when all runs completed
    if (mnRunIndex >= static_cast<int>(maRuns.size()))
        return false;

    int nRunPos0 = maRuns[mnRunIndex + 0];
    int nRunPos1 = maRuns[mnRunIndex + 1];
    *bRightToLeft = (nRunPos0 > nRunPos1);

    if (*nCharPos < 0)
    {
        // get first valid nCharPos in run
        *nCharPos = nRunPos0;
    }
    else
    {
        // advance to next nCharPos for LTR case
        if (!*bRightToLeft)
            ++(*nCharPos);

        // advance to next run if current run is completed
        if (*nCharPos == nRunPos1)
        {
            if ((mnRunIndex += 2) >= static_cast<int>(maRuns.size()))
                return false;
            nRunPos0 = maRuns[mnRunIndex + 0];
            nRunPos1 = maRuns[mnRunIndex + 1];
            *bRightToLeft = (nRunPos0 > nRunPos1);
            *nCharPos = nRunPos0;
        }
    }

    // advance to next nCharPos for RTL case
    if (*bRightToLeft)
        --(*nCharPos);

    return true;
}

bool ImplLayoutRuns::GetRun(int* nMinRunPos, int* nEndRunPos, bool* bRightToLeft) const
{
    if (mnRunIndex >= static_cast<int>(maRuns.size()))
        return false;

    int nRunPos0 = maRuns[mnRunIndex + 0];
    int nRunPos1 = maRuns[mnRunIndex + 1];
    *bRightToLeft = (nRunPos1 < nRunPos0);
    if (!*bRightToLeft)
    {
        *nMinRunPos = nRunPos0;
        *nEndRunPos = nRunPos1;
    }
    else
    {
        *nMinRunPos = nRunPos1;
        *nEndRunPos = nRunPos0;
    }
    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
