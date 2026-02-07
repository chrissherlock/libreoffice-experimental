/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <text/TextAnalyzer.hxx>
#include <vcl/mnemonic.hxx>

using namespace vcl::text;

namespace
{
class TextAnalyzerTest : public CppUnit::TestFixture
{
};

CPPUNIT_TEST_FIXTURE(TextAnalyzerTest, testGetNormalizedLength)
{
    OUString aStr = "Hello World";
    // Normal case
    CPPUNIT_ASSERT_EQUAL(sal_Int32(5), TextAnalyzer::GetNormalizedLength(aStr, 0, 5));

    // Auto-calculate length (-1)
    CPPUNIT_ASSERT_EQUAL(sal_Int32(11), TextAnalyzer::GetNormalizedLength(aStr, 0, -1));

    // Clamping (Request 100 chars starting at 6)
    CPPUNIT_ASSERT_EQUAL(sal_Int32(5), TextAnalyzer::GetNormalizedLength(aStr, 6, 100));

    // Invalid Index (Out of bounds)
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), TextAnalyzer::GetNormalizedLength(aStr, 20, 5));
}

CPPUNIT_TEST_FIXTURE(TextAnalyzerTest, testPrepareMnemonicText)
{
    // Case 1: Standard Mnemonic "~File"
    OUString aInput = "~File";
    MnemonicText aRes = TextAnalyzer::PrepareMnemonicText(aInput, 0, aInput.getLength());

    CPPUNIT_ASSERT_EQUAL(OUString("File"), aRes.aText);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aRes.nMnemonicPos);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(4), aRes.nLen);

    // Case 2: Mid-string Mnemonic "Fi~le"
    aInput = "Fi~le";
    aRes = TextAnalyzer::PrepareMnemonicText(aInput, 0, aInput.getLength());
    CPPUNIT_ASSERT_EQUAL(OUString("File"), aRes.aText);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aRes.nMnemonicPos);

    // Case 3: No Mnemonic
    aInput = "File";
    aRes = TextAnalyzer::PrepareMnemonicText(aInput, 0, aInput.getLength());
    CPPUNIT_ASSERT_EQUAL(OUString("File"), aRes.aText);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(-1), aRes.nMnemonicPos);

    // Case 4: Mnemonic outside the requested range
    // "He~llo World". Mnemonic is '~' (index 2).
    // We are processing "World" (Original Index 7, length 5).
    aInput = "He~llo World";
    aRes = TextAnalyzer::PrepareMnemonicText(aInput, 7, 5);

    // 1. The tilde is removed. "He~llo" -> "Hello". Mnemonic pos is 2 ('l').
    CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aRes.nMnemonicPos);

    // 2. The start index shifts left by 1 because the removal occurred before it.
    // 7 -> 6.
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aRes.nIndex);

    // 3. We verify that the system correctly identifies this as "Out of Range"
    CPPUNIT_ASSERT_EQUAL(
        false, TextAnalyzer::IsMnemonicInRange(aRes.nMnemonicPos, aRes.nIndex, aRes.nLen));
}

CPPUNIT_TEST_FIXTURE(TextAnalyzerTest, testIsMnemonicInRange)
{
    // Mnemonic at 5. Range [0, 10) -> True
    CPPUNIT_ASSERT(TextAnalyzer::IsMnemonicInRange(5, 0, 10));

    // Mnemonic at 5. Range [6, 10) -> False
    CPPUNIT_ASSERT(!TextAnalyzer::IsMnemonicInRange(5, 6, 10));

    // Mnemonic at 5. Range [0, 5) -> False (Exclusive end)
    CPPUNIT_ASSERT(!TextAnalyzer::IsMnemonicInRange(5, 0, 5));
}

CPPUNIT_TEST_FIXTURE(TextAnalyzerTest, testBiDiLayoutFlags)
{
    OUString aLatin = u"Hello World"_ustr;
    // Note: Change 'TextLayoutEngine' to 'TextAnalyzer' here after pasting!
    SalLayoutFlags nFlags = vcl::text::TextAnalyzer::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aLatin, 0, aLatin.getLength());
    CPPUNIT_ASSERT(bool(nFlags & SalLayoutFlags::BiDiStrong));

    OUString aArabic = u"مرحبا"_ustr;
    nFlags = vcl::text::TextAnalyzer::GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags::Default,
                                                         aArabic, 0, aArabic.getLength());
    CPPUNIT_ASSERT_EQUAL(SalLayoutFlags::NONE, nFlags);

    nFlags = vcl::text::TextAnalyzer::GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags::BiDiRtl,
                                                         aLatin, 0, aLatin.getLength());
    CPPUNIT_ASSERT(bool(nFlags & SalLayoutFlags::BiDiRtl));
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
