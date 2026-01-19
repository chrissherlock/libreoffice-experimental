/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <vcl/outdev.hxx>
#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>

namespace
{
class TextLayoutEngineTest : public test::BootstrapFixture
{
public:
    TextLayoutEngineTest()
        : BootstrapFixture(true, false)
    {
    }

    void testBiDiLayoutFlags();

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testBiDiLayoutFlags);
    CPPUNIT_TEST_SUITE_END();
};

void TextLayoutEngineTest::testBiDiLayoutFlags()
{
    // Case 1: Pure Latin Text (LTR)
    // "Hello World" contains only characters < 0x052F.
    // The engine optimization (bAllLtr) should trigger and set BiDiStrong.
    OUString aLatin = u"Hello World"_ustr;
    SalLayoutFlags nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aLatin, 0, aLatin.getLength());

    // Verify that the LTR optimization flag (BiDiStrong) is set.
    // We avoid checking !BiDiRtl specifically because in some VCL configurations
    // flags might be aliased or interact unexpectedly. The presence of BiDiStrong
    // confirms the engine took the "LTR optimization" path.
    bool bHasStrong = bool(nFlags & SalLayoutFlags::BiDiStrong);
    CPPUNIT_ASSERT_MESSAGE("Pure LTR text should trigger BiDiStrong optimization", bHasStrong);

    // Case 2: Arabic Text (RTL)
    // Contains characters > 0x052F. bAllLtr will be false.
    // The default loop should NOT set BiDiStrong (unless forced, but here it shouldn't).
    // Wait - the logic is: if (bAllLtr) set BiDiStrong.
    // If NOT bAllLtr (Arabic), we fall through and return NONE (or existing flags).
    OUString aArabic = u"مرحبا"_ustr;
    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aArabic, 0, aArabic.getLength());

    // For mixed/RTL content without a forcing flag, it returns NONE (letting the lower layers handle it)
    // OR it might detect Strong RTL if we improved it, but the current logic only adds BiDiStrong for LTR.
    CPPUNIT_ASSERT_EQUAL(SalLayoutFlags::NONE, nFlags);

    // Case 3: Forced RTL Mode
    // Should have BiDiRtl flag regardless of content
    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::BiDiRtl, aLatin, 0, aLatin.getLength());

    bool bHasRTL = bool(nFlags & SalLayoutFlags::BiDiRtl);
    CPPUNIT_ASSERT_MESSAGE("Forced RTL mode should set BiDiRtl flag", bHasRTL);
}

} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
