/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/MultiLineEngine.hxx>
#include <algorithm>

namespace vcl::text
{
OUString MultiLineEngine::GetEllipsisString(
    const OUString& rStr, tools::Long nMaxWidth, DrawTextFlags nStyle,
    const std::function<tools::Long(const OUString&)>& rfnGetTextWidth)
{
    if (rStr.isEmpty() || rfnGetTextWidth(rStr) <= nMaxWidth)
        return rStr;

    const OUString aEllipsisStr = "...";
    const bool bClipText = bool(nStyle & DrawTextFlags::Clip);
    sal_Int32 nLen = rStr.getLength();

    // Center Ellipsis (Balanced Truncation)
    if (nStyle & DrawTextFlags::CenterEllipsis)
    {
        sal_Int32 nIndex = nLen / 2;
        sal_Int32 nEraseChars = std::max<sal_Int32>(4, nLen - (nIndex * 4) / 3);

        while (nEraseChars < nLen)
        {
            sal_Int32 i = (nLen - nEraseChars) / 2;
            OUString aTmpStr = rStr.copy(0, i) + aEllipsisStr + rStr.copy(i + nEraseChars);

            if (rfnGetTextWidth(aTmpStr) <= nMaxWidth)
                return aTmpStr;

            nEraseChars++;
        }
        return aEllipsisStr;
    }

    // Path Ellipsis
    if (nStyle & DrawTextFlags::PathEllipsis)
    {
        sal_Int32 nLastSep = rStr.lastIndexOf('/');
        if (nLastSep == -1)
            nLastSep = rStr.lastIndexOf('\\');

        if (nLastSep != -1 && nLastSep > 0)
        {
            for (sal_Int32 i = 1; i < nLastSep; ++i)
            {
                OUString aTest = rStr.copy(0, i) + aEllipsisStr + rStr.copy(nLastSep);

                if (rfnGetTextWidth(aTest) > nMaxWidth)
                {
                    return (i > 1)
                               ? OUString(rStr.copy(0, i - 1) + aEllipsisStr + rStr.copy(nLastSep))
                               : OUString(aEllipsisStr + rStr.copy(nLastSep));
                }
            }
        }
    }

    // News Ellipsis (Structure Preserving)
    if (nStyle & DrawTextFlags::NewsEllipsis)
    {
        OUString aCurrentStr = rStr;
        auto lcl_IsSep = [](sal_Unicode c) { return c == '.'; };
        sal_Int32 nLastContent = nLen;

        while (nLastContent > 0)
        {
            nLastContent--;
            if (lcl_IsSep(aCurrentStr[nLastContent]))
                break;
        }

        while (nLastContent > 0 && lcl_IsSep(aCurrentStr[nLastContent - 1]))
        {
            nLastContent--;
        }

        OUString aLastStr = aCurrentStr.copy(nLastContent);

        if (rfnGetTextWidth(aEllipsisStr + aLastStr) <= nMaxWidth)
        {
            sal_Int32 nFirstContent = 0;

            while (nFirstContent < nLastContent)
            {
                nFirstContent++;
                if (lcl_IsSep(aCurrentStr[nFirstContent]))
                    break;
            }

            while (nFirstContent < nLastContent && lcl_IsSep(aCurrentStr[nFirstContent]))
            {
                nFirstContent++;
            }

            if (nFirstContent < nLastContent)
            {
                if (nFirstContent > 4)
                    nFirstContent = 4;

                OUString aFirstStr = aCurrentStr.copy(0, nFirstContent) + aEllipsisStr;
                OUString aTempStr = aFirstStr + aLastStr;

                if (rfnGetTextWidth(aTempStr) <= nMaxWidth)
                {
                    do
                    {
                        aCurrentStr = aTempStr;
                        if (nLastContent > aCurrentStr.getLength())
                            nLastContent = aCurrentStr.getLength();

                        while (nFirstContent < nLastContent)
                        {
                            nLastContent--;
                            if (lcl_IsSep(aCurrentStr[nLastContent]))
                                break;
                        }

                        while (nFirstContent < nLastContent
                               && lcl_IsSep(aCurrentStr[nLastContent - 1]))
                        {
                            nLastContent--;
                        }

                        if (nFirstContent < nLastContent)
                        {
                            aTempStr = aFirstStr + aCurrentStr.copy(nLastContent);

                            if (rfnGetTextWidth(aTempStr) > nMaxWidth)
                                break;
                        }
                    } while (nFirstContent < nLastContent);

                    return aCurrentStr;
                }
            }
        }
    }

    // End Ellipsis (Priority: Text+Dots > Text(1) > Dots)
    // Iterate from full length down to 1 char.
    for (sal_Int32 i = nLen - 1; i >= 1; --i)
    {
        // Case A: Try fitting "Text..."
        // We only try this if i > 1.
        // Parity Rule: If i == 1 ("a"), we NEVER add dots, we just return "a" if it fits.
        if (i > 1)
        {
            OUString aWithDots = rStr.copy(0, i) + aEllipsisStr;
            if (rfnGetTextWidth(aWithDots) <= nMaxWidth)
                return aWithDots;
        }
        else
        {
            // Case B: i == 1. Check if "a" fits without dots.
            OUString aFirstChar = rStr.copy(0, 1);
            if (rfnGetTextWidth(aFirstChar) <= nMaxWidth)
                return aFirstChar;
        }
    }

    // Ultimate Fallback
    // If nothing above fit, and Clip is requested, return "a" (even if it's too wide).
    if (bClipText)
        return rStr.copy(0, 1);

    // Otherwise, return "..."
    return aEllipsisStr;
}

} // namespace vcl::text
