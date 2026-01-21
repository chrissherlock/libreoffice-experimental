/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_I18NUTIL_DIGITLOCALIZATION_HXX
#define INCLUDED_I18NUTIL_DIGITLOCALIZATION_HXX

#include <i18nutil/i18nutildllapi.h>
#include <rtl/ustring.hxx>
#include <i18nlangtag/lang.h>

namespace i18nutil
{
/** Returns the zero-digit offset for the given language.
    (e.g. 0x0660 - '0' for Arabic-Indic).
    Returns 0 if no substitution is required.
*/
I18NUTIL_DLLPUBLIC int GetLocalizedDigitOffset(LanguageType eLang);

I18NUTIL_DLLPUBLIC OUString LocalizeDigitsInString(const OUString& sStr,
                                                   LanguageType eTextLanguage);

/** Replaces ASCII digits (0-9) in the substring with the native digits
    corresponding to the given LanguageType.
*/
I18NUTIL_DLLPUBLIC OUString LocalizeDigitsInString(const OUString& rString, LanguageType eLang,
                                                   sal_Int32 nStart, sal_Int32& rLen);
}

#endif
