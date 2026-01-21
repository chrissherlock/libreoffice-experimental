/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <rtl/string.hxx>
#include <rtl/ustring.hxx>
#include <rtl/ustrbuf.hxx>

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <i18nutil/digitlocalization.hxx>

using namespace i18nutil;
#include <cppunit/plugin/TestPlugIn.h>

namespace
{
CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testLocalDigits)
{
    // Test the case where there are no digits to convert
    OUString sNoDigits = "hello";
    sal_Int32 nLen = sNoDigits.getLength();
    // It should return the same string
    CPPUNIT_ASSERT_EQUAL(reinterpret_cast<sal_uIntPtr>(sNoDigits.getStr()),
                         reinterpret_cast<sal_uIntPtr>(
                             LocalizeDigitsInString(sNoDigits, LANGUAGE_FARSI, 0, nLen).getStr()));
    // The length should not change
    CPPUNIT_ASSERT_EQUAL(sNoDigits.getLength(), nLen);

    // Test the case where there are digits but they are already correct for the locale.
    OUString sDozen = "There are 12 eggs in a dozen.";
    nLen = sDozen.getLength();
    // It should return the same string
    CPPUNIT_ASSERT_EQUAL(reinterpret_cast<sal_uIntPtr>(sDozen.getStr()),
                         reinterpret_cast<sal_uIntPtr>(
                             LocalizeDigitsInString(sDozen, LANGUAGE_SYSTEM, 0, nLen).getStr()));
    // The length should not change
    CPPUNIT_ASSERT_EQUAL(sDozen.getLength(), nLen);

    // Test an actual conversion
    CPPUNIT_ASSERT_EQUAL(u"There are ۱۲ eggs in a dozen."_ustr,
                         LocalizeDigitsInString(sDozen, LANGUAGE_FARSI, 0, nLen));
    CPPUNIT_ASSERT_EQUAL(sDozen.getLength(), nLen);

    // Test converting a subrange
    nLen = 12;
    CPPUNIT_ASSERT_EQUAL(
        u"There are ۱۲ eggs in a dozen but 13 in a baker's dozen."_ustr,
        LocalizeDigitsInString("There are 12 eggs in a dozen but 13 in a baker's dozen.",
                               LANGUAGE_FARSI, 2, nLen));
    CPPUNIT_ASSERT_EQUAL(sal_Int32(12), nLen);

    // Test with characters outside of the bmp
    CPPUNIT_ASSERT_EQUAL(
        u"𐑞𐑺 𐑸 ۱۲ 𐑧𐑜𐑟 𐑦𐑯 𐑩 𐑚𐑱𐑒𐑼𐑟 𐑛𐑳𐑟𐑩𐑯."_ustr,
        LocalizeDigitsInString(u"𐑞𐑺 𐑸 12 𐑧𐑜𐑟 𐑦𐑯 𐑩 𐑚𐑱𐑒𐑼𐑟 𐑛𐑳𐑟𐑩𐑯."_ustr, LANGUAGE_FARSI));
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testAllLanguageOffsets)
{
    // Map of Language -> Expected Unicode Zero Digit
    std::vector<std::pair<LanguageType, sal_Unicode>> aTests
        = { { LANGUAGE_ENGLISH_US, '0' }, // No offset
            { LANGUAGE_ARABIC_SAUDI_ARABIA, 0x0660 },
            { LANGUAGE_FARSI, 0x06F0 },
            { LANGUAGE_URDU_PAKISTAN, 0x06F0 },
            { LANGUAGE_BENGALI, 0x09E6 },
            { LANGUAGE_HINDI, 0x0966 },
            { LANGUAGE_AMHARIC_ETHIOPIA, 0x1369 },
            { LANGUAGE_GUJARATI, 0x0AE6 },
            { LANGUAGE_KANNADA, 0x0CE6 },
            { LANGUAGE_KHMER, 0x17E0 },
            { LANGUAGE_LAO, 0x0ED0 },
            { LANGUAGE_MALAYALAM, 0x0D66 },
            { LANGUAGE_BURMESE, 0x1040 },
            { LANGUAGE_ODIA, 0x0B66 },
            { LANGUAGE_TAMIL, 0x0BE7 },
            { LANGUAGE_TELUGU, 0x0C66 },
            { LANGUAGE_THAI, 0x0E50 },
            { LANGUAGE_TIBETAN, 0x0F20 },

            // Mongolian Case 1: Traditional Script (Mongolia/China)
            { LANGUAGE_MONGOLIAN_MONGOLIAN_CHINA, 0x1810 },
            // Mongolian Case 2: Cyrillic (Russia/Buryat) - Should be standard digits
            { LANGUAGE_MONGOLIAN_CYRILLIC_MONGOLIA, '0' } };

    for (const auto& rTest : aTests)
    {
        int nExpectedOffset = rTest.second - '0';
        int nActualOffset = i18nutil::GetLocalizedDigitOffset(rTest.first);

        rtl::OString sMsg = "Failed for language ID: "
                            + rtl::OString::number(static_cast<sal_uInt16>(rTest.first));
        CPPUNIT_ASSERT_EQUAL_MESSAGE(sMsg.getStr(), nExpectedOffset, nActualOffset);

        // Also verify the string conversion works for this language
        rtl::OUString sInput = "0123";
        sal_Int32 nLen = 4;
        rtl::OUString sResult = i18nutil::LocalizeDigitsInString(sInput, rTest.first, 0, nLen);

        if (nExpectedOffset == 0)
        {
            CPPUNIT_ASSERT_EQUAL(sInput, sResult);
        }
        else
        {
            sal_Unicode cZero = rTest.second;
            OUStringBuffer sExp;
            sExp.append(cZero);
            sExp.append(static_cast<sal_Unicode>(cZero + 1));
            sExp.append(static_cast<sal_Unicode>(cZero + 2));
            sExp.append(static_cast<sal_Unicode>(cZero + 3));
            CPPUNIT_ASSERT_EQUAL(sExp.makeStringAndClear(), sResult);
        }
    }
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
