/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>

#include <salhelper/simplereferenceobject.hxx>
#include <tools/degree.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/outdev.hxx>
#include <vcl/font.hxx>

#include <font/FontController.hxx>
#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>
#include <GraphicsState.hxx>
#include <ImplLayoutArgs.hxx>

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
    void testCreateLayoutRequest_Simple();
    void testCreateLayoutRequest_DigitLocalization();
    void testCreateLayoutRequest_OrientationAndWidth();
    void testCreateLayoutRequest_OutOfBounds();
    void testFindFallbackFont_ForcedFallbackPriority();
    void testIdentifyMissingChars();
    void testJustifyLayout();
    void testSetAnchorPoint();
    void testApplyHorizontalOffset();

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testBiDiLayoutFlags);
    CPPUNIT_TEST(testCreateLayoutRequest_Simple);
    CPPUNIT_TEST(testCreateLayoutRequest_DigitLocalization);
    CPPUNIT_TEST(testCreateLayoutRequest_OrientationAndWidth);
    CPPUNIT_TEST(testCreateLayoutRequest_OutOfBounds);
    CPPUNIT_TEST(testFindFallbackFont_ForcedFallbackPriority);
    CPPUNIT_TEST(testIdentifyMissingChars);
    CPPUNIT_TEST(testJustifyLayout);
    CPPUNIT_TEST(testSetAnchorPoint);
    CPPUNIT_TEST(testApplyHorizontalOffset);
    CPPUNIT_TEST_SUITE_END();
};

void TextLayoutEngineTest::testBiDiLayoutFlags()
{
    // Case 1: Pure Latin Text (LTR)
    OUString aLatin = u"Hello World"_ustr;
    SalLayoutFlags nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aLatin, 0, aLatin.getLength());

    // Verify that the LTR optimization flag (BiDiStrong) is set.
    bool bHasStrong = bool(nFlags & SalLayoutFlags::BiDiStrong);
    CPPUNIT_ASSERT_MESSAGE("Pure LTR text should trigger BiDiStrong optimization", bHasStrong);

    // Case 2: Arabic Text (RTL)
    OUString aArabic = u"مرحبا"_ustr;
    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aArabic, 0, aArabic.getLength());

    // For mixed/RTL content without a forcing flag, it returns NONE
    CPPUNIT_ASSERT_EQUAL(SalLayoutFlags::NONE, nFlags);

    // Case 3: Forced RTL Mode
    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::BiDiRtl, aLatin, 0, aLatin.getLength());

    bool bHasRTL = bool(nFlags & SalLayoutFlags::BiDiRtl);
    CPPUNIT_ASSERT_MESSAGE("Forced RTL mode should set BiDiRtl flag", bHasRTL);
}

void TextLayoutEngineTest::testCreateLayoutRequest_Simple()
{
    // 1. Setup Mock State
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;

    // Setup basic font
    vcl::Font aFont("Arial", Size(0, 12));
    aState.maFont = aFont;
    aRealization.mxFont = nullptr;

    OUString aInput = u"Hello World"_ustr;
    sal_Int32 nMin = 0;
    sal_Int32 nLen = 5; // "Hello"

    // 2. Call the Engine
    vcl::text::ImplLayoutArgs aArgs
        = vcl::text::TextLayoutEngine::CreateLayoutRequest(aInput, nMin, nLen,
                                                           100.0, // Pixel Width
                                                           SalLayoutFlags::NONE,
                                                           nullptr, // Cache
                                                           aState, aRealization,
                                                           false // bRTL
        );

    // 3. Verify
    CPPUNIT_ASSERT_EQUAL(nMin, aArgs.mnMinCharPos);
    CPPUNIT_ASSERT_EQUAL(nMin + nLen, aArgs.mnEndCharPos);
    // Verify defaults
    CPPUNIT_ASSERT_EQUAL((0_deg10).get(), aArgs.mnOrientation.get());
}

void TextLayoutEngineTest::testCreateLayoutRequest_DigitLocalization()
{
    // 1. Setup State: Language set to Arabic (Saudi Arabia)
    vcl::GraphicsState aState;
    aState.meTextLanguage = LANGUAGE_ARABIC_SAUDI_ARABIA;

    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;

    // 2. Input: "Year 2024"
    // IMPORTANT: CreateLayoutRequest takes OUString& and modifies it in place!
    OUString aInput = u"Year 2024"_ustr;

    vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 0, aInput.getLength(), 100.0, SalLayoutFlags::NONE, nullptr, aState, aRealization,
        false);

    // 3. Verify the SIDE EFFECT (String modification)
    // 0x0660 is the zero digit for Arabic
    sal_Unicode cZero = 0x0660;
    // Construct expected string "Year ٢٠٢٤"
    OUStringBuffer aBuf;
    aBuf.append(u"Year ");
    aBuf.append(static_cast<sal_Unicode>(cZero + 2));
    aBuf.append(static_cast<sal_Unicode>(cZero + 0));
    aBuf.append(static_cast<sal_Unicode>(cZero + 2));
    aBuf.append(static_cast<sal_Unicode>(cZero + 4));
    OUString aExpected = aBuf.makeStringAndClear();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("String should contain localized Arabic digits", aExpected,
                                 aInput);
}

void TextLayoutEngineTest::testCreateLayoutRequest_OrientationAndWidth()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr; // Defaults to 0 orientation

    OUString aInput = u"CheckWidth"_ustr;
    double nTestWidth = 555.5;

    vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 0, aInput.getLength(), nTestWidth, SalLayoutFlags::NONE, nullptr, aState,
        aRealization, false);

    // Verify Width is correctly stored in the Request object
    // ImplLayoutArgs typically truncates double to integer for layout width
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(nTestWidth),
                         static_cast<sal_Int32>(aArgs.mnLayoutWidth));

    // Verify fallback orientation (nullptr font -> 0 degrees)
    CPPUNIT_ASSERT_EQUAL((0_deg10).get(), aArgs.mnOrientation.get());
}

void TextLayoutEngineTest::testCreateLayoutRequest_OutOfBounds()
{
    // Test the safety clamping logic when MinIndex > StringLength
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;

    OUString aInput = u"Hi"_ustr;
    // Request start at 10, length 5 (Way out of bounds)
    vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 10, 5, 100.0, SalLayoutFlags::NONE, nullptr, aState, aRealization, false);

    // Should clamp start/end to be safe (likely equal to MinIndex or StringLength)
    // Based on logic: nEndIndex becomes nMinIndex (10).
    // This ensures the engine doesn't crash on bad inputs.
    CPPUNIT_ASSERT_EQUAL(sal_Int32(10), aArgs.mnMinCharPos);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(10), aArgs.mnEndCharPos);
}

namespace
{
struct MockFontInstance : public salhelper::SimpleReferenceObject
{
    MockFontInstance() {}
};
}

void TextLayoutEngineTest::testFindFallbackFont_ForcedFallbackPriority()
{
    bool bHasUsedForcedFallback = false;

    // Create a dummy "Forced Font"
    // Use unsafe cast because we only need the pointer identity and ref-counting
    rtl::Reference<LogicalFontInstance> pForcedFont(
        reinterpret_cast<LogicalFontInstance*>(new MockFontInstance()));

    // Create dummy dependencies (Safe because the Forced path DOES NOT dereference them)
    // We pass 0xDEADBEEF to ensure that if it DOES try to access it, the test will crash hard.
    ImplFontCache* pDummyCache = reinterpret_cast<ImplFontCache*>(0xDEADBEEF);
    vcl::font::PhysicalFontCollection* pDummyCollection = nullptr;

    OUString aMissingCodes = "A";
    vcl::font::FontSelectPattern aPattern(vcl::Font("Arial", Size(0, 10)), u"Arial"_ustr,
                                          Size(0, 10), 0.0, false);

    // This verifies that the engine prioritizes the forced fallback before hitting the cache.
    // Construct criteria for test
    vcl::text::FontLookupCriteria aCriteria = {
        *pDummyCache, pDummyCollection,
        nullptr, // pReferenceFont (Base Font)
        pForcedFont // pPriorityFallback
    };

    rtl::Reference<LogicalFontInstance> pResult
        = vcl::text::TextLayoutEngine::FindFallbackFont(aCriteria,
                                                        1, // Level
                                                        aMissingCodes, bHasUsedForcedFallback,
                                                        nullptr // pGlyphsImpl
        );

    CPPUNIT_ASSERT_EQUAL(pForcedFont.get(), pResult.get());

    CPPUNIT_ASSERT_MESSAGE("Should mark forced fallback as used", bHasUsedForcedFallback);
}

void TextLayoutEngineTest::testIdentifyMissingChars()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;

    // Empty Case (No runs)
    {
        OUString aInput = u"NothingMissing"_ustr;
        vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
            aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
            false);

        // Ensure runs are empty initially
        aArgs.maRuns.Clear();

        OUString aResult = vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Should return empty string for empty runs", OUString(""),
                                     aResult);
    }

    // Single Run Case
    {
        OUString aInput = u"Hello World"_ustr;
        vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
            aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
            false);

        aArgs.maRuns.Clear();
        // Add run for "World" (Index 6 to 11)
        aArgs.maRuns.AddRun(6, 11, false);

        OUString aResult = vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Should extract 'World'", OUString("World"), aResult);
    }

    // Multiple Disjoint Runs Case
    {
        OUString aInput = u"Missing Glyphs Here"_ustr;
        vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
            aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
            false);

        aArgs.maRuns.Clear();
        // Run 1: "M" (0-1)
        aArgs.maRuns.AddRun(0, 1, false);
        // Run 2: "G" (8-9)
        aArgs.maRuns.AddRun(8, 9, false);
        // Run 3: "H" (15-16)
        aArgs.maRuns.AddRun(15, 16, false);

        OUString aResult = vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Should concatenate missing parts 'MGH'", OUString("MGH"),
                                     aResult);
    }

    // Adjacent/Split Runs Case
    {
        OUString aInput = u"SplitRun"_ustr;
        vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
            aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
            false);

        aArgs.maRuns.Clear();
        // "Split" (0-5)
        aArgs.maRuns.AddRun(0, 5, false);
        // "Run" (5-8)
        aArgs.maRuns.AddRun(5, 8, false);

        OUString aResult = vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Should stitch adjacent runs 'SplitRun'", OUString("SplitRun"),
                                     aResult);
    }

    // The function blindly extracts [Start, End), the RTL flag in the run shouldn't change *which* characters are pulled.
    {
        OUString aInput = u"RTLTest"_ustr;
        vcl::text::ImplLayoutArgs aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
            aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
            false);

        aArgs.maRuns.Clear();
        // "Test" (3-7) marked as RTL
        aArgs.maRuns.AddRun(3, 7, true);

        OUString aResult = vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Should extract text regardless of RTL flag", OUString("Test"),
                                     aResult);
    }
}

class MockSalLayout : public SalLayout
{
public:
    bool bAdjustCalled = false;

    // SalLayout overrides
    virtual void AdjustLayout(vcl::text::ImplLayoutArgs&) override { bAdjustCalled = true; }

    // Abstract methods that must be implemented with exact signatures
    virtual bool LayoutText(vcl::text::ImplLayoutArgs&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void DrawText(SalGraphics&) const override {}

    virtual double GetTextWidth() const override { return 100.0; }
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, int,
                                      int) const override
    {
        return 0;
    }
    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual bool HasFontKashidaPositions() const override { return false; }
    virtual bool IsKashidaPosValid(int, int) const override { return false; }

    // Fixed signature: the 4th parameter is const LogicalFontInstance**
    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance** ppFont) const override
    {
        (void)pGlyph;
        (void)rPos;
        (void)nStart;
        (void)ppFont;
        return false;
    }
};

void TextLayoutEngineTest::testJustifyLayout()
{
    MockSalLayout aLayout;
    vcl::text::ImplLayoutArgs aArgs(u"LibreOffice"_ustr, 0, 11, SalLayoutFlags::NONE,
                                    LanguageTag(LANGUAGE_ENGLISH_US), nullptr);

    vcl::text::TextLayoutEngine::JustifyLayout(aLayout, aArgs);
    CPPUNIT_ASSERT_MESSAGE("JustifyLayout should call AdjustLayout on SalLayout",
                           aLayout.bAdjustCalled);
}

void TextLayoutEngineTest::testSetAnchorPoint()
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutPositioning aPos;
    aPos.aDrawBase = basegfx::B2DPoint(123.4, 567.8);

    vcl::text::TextLayoutEngine::SetAnchorPoint(aLayout, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(123.4, aLayout.DrawBase().getX(), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(567.8, aLayout.DrawBase().getY(), 0.001);
}

void TextLayoutEngineTest::testApplyHorizontalOffset()
{
    MockSalLayout aLayout;
    vcl::text::ImplLayoutArgs aArgs(u"RTL_TEST"_ustr, 0, 8, SalLayoutFlags::RightAlign,
                                    LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = true;
    aPos.nEndGlyphCoord = 0;
    aArgs.mnLayoutWidth = 0;

    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-99.0, aLayout.DrawOffset().getX(), 0.001);

    aArgs.mnLayoutWidth = 250;
    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-249.0, aLayout.DrawOffset().getX(), 0.001);
}

} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
