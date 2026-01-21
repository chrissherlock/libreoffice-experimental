/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <i18nutil/digitlocalization.hxx>
#include <i18nlangtag/languagetag.hxx>
#include <rtl/ustrbuf.hxx>

namespace i18nutil
{
int GetLocalizedDigitOffset(LanguageType eLang)
{
    // eLang & LANGUAGE_MASK_PRIMARY catches language independent of region.
    LanguageType pri = primary(eLang);
    if (pri == primary(LANGUAGE_ARABIC_SAUDI_ARABIA))
        return 0x0660 - '0'; // arabic-indic digits
    else if (pri.anyOf(primary(LANGUAGE_FARSI), primary(LANGUAGE_URDU_PAKISTAN),
                       primary(LANGUAGE_PUNJABI), primary(LANGUAGE_SINDHI)))
        return 0x06F0 - '0'; // eastern arabic-indic digits
    else if (pri == primary(LANGUAGE_BENGALI))
        return 0x09E6 - '0'; // bengali
    else if (pri == primary(LANGUAGE_HINDI))
        return 0x0966 - '0'; // devanagari
    else if (pri.anyOf(primary(LANGUAGE_AMHARIC_ETHIOPIA), primary(LANGUAGE_TIGRIGNA_ETHIOPIA)))
        return 0x1369 - '0'; // ethiopic
    else if (pri == primary(LANGUAGE_GUJARATI))
        return 0x0AE6 - '0'; // gujarati
#ifdef LANGUAGE_GURMUKHI
    else if (pri == primary(LANGUAGE_GURMUKHI))
        return 0x0A66 - '0'; // gurmukhi
#endif
    else if (pri == primary(LANGUAGE_KANNADA))
        return 0x0CE6 - '0'; // kannada
    else if (pri == primary(LANGUAGE_KHMER))
        return 0x17E0 - '0'; // khmer
    else if (pri == primary(LANGUAGE_LAO))
        return 0x0ED0 - '0'; // lao
    else if (pri == primary(LANGUAGE_MALAYALAM))
        return 0x0D66 - '0'; // malayalam
    else if (pri == primary(LANGUAGE_MONGOLIAN_MONGOLIAN_LSO))
    {
        if (eLang.anyOf(LANGUAGE_MONGOLIAN_MONGOLIAN_MONGOLIA, LANGUAGE_MONGOLIAN_MONGOLIAN_CHINA,
                        LANGUAGE_MONGOLIAN_MONGOLIAN_LSO))
            return 0x1810 - '0'; // mongolian
        else
            return 0; // mongolian cyrillic
    }
    else if (pri == primary(LANGUAGE_BURMESE))
        return 0x1040 - '0'; // myanmar
    else if (pri == primary(LANGUAGE_ODIA))
        return 0x0B66 - '0'; // odia
    else if (pri == primary(LANGUAGE_TAMIL))
        return 0x0BE7 - '0'; // tamil
    else if (pri == primary(LANGUAGE_TELUGU))
        return 0x0C66 - '0'; // telugu
    else if (pri == primary(LANGUAGE_THAI))
        return 0x0E50 - '0'; // thai
    else if (pri == primary(LANGUAGE_TIBETAN))
        return 0x0F20 - '0'; // tibetan
    else
        return 0;
}

OUString LocalizeDigitsInString(const OUString& sStr, LanguageType eTextLanguage)
{
    sal_Int32 nLen = sStr.getLength();
    return LocalizeDigitsInString(sStr, eTextLanguage, 0, nLen);
}

OUString LocalizeDigitsInString(const OUString& sStr, LanguageType eTextLanguage, sal_Int32 nStart,
                                sal_Int32& nLen)
{
    int digitOffset = GetLocalizedDigitOffset(eTextLanguage);

    if (digitOffset == 0)
        return sStr;

    sal_Int32 nEnd = nStart + nLen;

    for (sal_Int32 i = nStart; i < nEnd; ++i)
    {
        sal_Unicode nChar = sStr[i];

        if (nChar >= '0' && nChar <= '9')
        {
            OUStringBuffer xTmpStr(sStr.getLength());
            xTmpStr.append(sStr.subView(0, i));

            for (; i < nEnd; ++i)
            {
                nChar = sStr[i];
                if (nChar >= '0' && nChar <= '9')
                    xTmpStr.appendUtf32(nChar + digitOffset);
                else
                    xTmpStr.append(nChar);
            }

            xTmpStr.append(sStr.subView(nEnd));
            nLen += xTmpStr.getLength() - sStr.getLength();

            return xTmpStr.makeStringAndClear();
        }
    }
    return sStr;
}

} // namespace i18nutil
