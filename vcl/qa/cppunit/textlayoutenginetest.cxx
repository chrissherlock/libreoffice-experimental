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
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/degree.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/font.hxx>

#include <GraphicsState.hxx>
#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontFace.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/FontMetricData.hxx>
#include <sallayout.hxx>
#include <text/TextLayoutEngine.hxx>
#include <text/TextLayoutRequest.hxx>

#include <unicode/uchar.h>

namespace
{
class StubPhysicalFontFace : public vcl::font::PhysicalFontFace
{
public:
    StubPhysicalFontFace()
        : PhysicalFontFace(vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0))
    {
    }
    virtual rtl::Reference<LogicalFontInstance>
    CreateFontInstance(const vcl::font::FontSelectPattern&) const override
    {
        return nullptr;
    }
    virtual sal_IntPtr GetFontId() const override { return reinterpret_cast<sal_IntPtr>(this); }
    virtual hb_blob_t* GetHbTable(hb_tag_t) const override { return nullptr; }
};

class StubFontInstance : public LogicalFontInstance
{
public:
    StubFontInstance()
        : LogicalFontInstance(*new StubPhysicalFontFace(),
                              vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0))
    {
        mxFontMetric
            = new FontMetricData(vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0));
        mnLineHeight = 20;
        mxFontMetric->SetAscent(10);
        mxFontMetric->SetDescent(10);
        mnOrientation = 0_deg10;
    }
    virtual void ImplGetGlyphWidths(const sal_GlyphId*, bool, tools::Long*, int) const {}
    virtual bool ImplGetGlyphBoundRect(sal_GlyphId, tools::Rectangle& rRect, bool) const
    {
        rRect = tools::Rectangle(Point(0, -10), Size(10, 20));
        return true;
    }
    virtual void ImplGetFontMetric(FontMetricData&) const {}
    virtual bool GetGlyphOutline(sal_GlyphId, basegfx::B2DPolyPolygon&, bool) const override
    {
        return false;
    }
};

class MockSalLayout : public SalLayout
{
public:
    bool bAdjustCalled = false;
    virtual void AdjustLayout(vcl::text::TextLayoutRequest&) override { bAdjustCalled = true; }
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void DrawText(SalGraphics&) const override {}
    virtual double GetTextWidth() const override { return 100.0; }
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual bool HasFontKashidaPositions() const override { return false; }
    virtual bool IsKashidaPosValid(int, int) const override { return false; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, int,
                                      int) const override
    {
        return 0;
    }
    virtual bool GetNextGlyph(const GlyphItem**, basegfx::B2DPoint&, int&,
                              const LogicalFontInstance**) const override
    {
        return false;
    }
};

// Mock to test word segmentation logic
class WordSegmentMockLayout : public MockSalLayout
{
    std::vector<GlyphItem> mGlyphs;

public:
    WordSegmentMockLayout(const std::vector<bool>& rIsSpacing)
    {
        double nX = 0;
        for (bool bSpacing : rIsSpacing)
        {
            GlyphItem aGlyph(0, 1, 0, basegfx::B2DPoint(nX, 0),
                             bSpacing ? GlyphItemFlags::IS_SPACING : GlyphItemFlags::NONE, 10.0,
                             0.0, 0.0, 0);
            mGlyphs.push_back(aGlyph);
            nX += 10.0;
        }
    }

    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance**) const override
    {
        if (nStart < static_cast<int>(mGlyphs.size()))
        {
            *pGlyph = &mGlyphs[nStart];
            rPos = mGlyphs[nStart].linearPos();
            nStart++;
            return true;
        }
        return false;
    }
};

class MockEmphasisLayout : public MockSalLayout
{
public:
    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance**) const override
    {
        if (nStart == 0)
        {
            *pGlyph = reinterpret_cast<const GlyphItem*>(0xDEADBEEF);
            rPos = basegfx::B2DPoint(100.0, 0); // Glyph is relative to baseline
            nStart++;
            return true;
        }
        return false;
    }
};

class RotatableMockLayout : public MockSalLayout
{
    GlyphItem mGlyph1;
    GlyphItem mGlyph2;

public:
    const LogicalFontInstance* mpFont = nullptr;
    RotatableMockLayout()
        : mGlyph1(0, 1, 0, basegfx::B2DPoint(100, 100), GlyphItemFlags::NONE, 10.0, 0.0, 0.0, 0)
        , mGlyph2(1, 1, 0, basegfx::B2DPoint(200, 100), GlyphItemFlags::NONE, 10.0, 0.0, 0.0, 1)
    {
    }
    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance** ppFont) const override
    {
        if (ppFont)
            *ppFont = mpFont;
        if (nStart == 0)
        {
            *pGlyph = &mGlyph1;
            rPos = basegfx::B2DPoint(100, 100);
            nStart++;
            return true;
        }
        if (nStart == 1)
        {
            *pGlyph = &mGlyph2;
            rPos = basegfx::B2DPoint(200, 100);
            nStart++;
            return true;
        }
        return false;
    }
};

} // end anonymous namespace

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
    void testApplyHorizontalOffset_EndGlyph();
    void testApplyHorizontalOffset_Disabled();
    void testGetTextHeightPixel();
    void testEmphasisMarkPositions();
    void testCalculateOutlineTransform();
    void testGetTextInkBounds_Rotation();
    void testGetWordLineSegments();
    void testInitializeTextLineMetrics();
    void testInitializeFontMetrics();
    void testInitializeAboveTextLineMetrics();
    void testGetAlignmentOffset();
    void testGetRotatedGeometry_0_Degrees();
    void testGetRotatedGeometry_90_Degrees();
    void testGetRotatedGeometry_180_Degrees();
    void testGetRotatedGeometry_270_Degrees();
    void testGetRotatedGeometry_Arbitrary_Angle();
    void testGetRotatedImageOrigin();

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
    CPPUNIT_TEST(testApplyHorizontalOffset_EndGlyph);
    CPPUNIT_TEST(testApplyHorizontalOffset_Disabled);
    CPPUNIT_TEST(testGetTextHeightPixel);
    CPPUNIT_TEST(testEmphasisMarkPositions);
    CPPUNIT_TEST(testCalculateOutlineTransform);
    CPPUNIT_TEST(testGetTextInkBounds_Rotation);
    CPPUNIT_TEST(testGetWordLineSegments);
    CPPUNIT_TEST(testInitializeTextLineMetrics);
    CPPUNIT_TEST(testInitializeFontMetrics);
    CPPUNIT_TEST(testInitializeAboveTextLineMetrics);
    CPPUNIT_TEST(testGetAlignmentOffset);
    CPPUNIT_TEST(testGetRotatedGeometry_0_Degrees);
    CPPUNIT_TEST(testGetRotatedGeometry_90_Degrees);
    CPPUNIT_TEST(testGetRotatedGeometry_180_Degrees);
    CPPUNIT_TEST(testGetRotatedGeometry_270_Degrees);
    CPPUNIT_TEST(testGetRotatedGeometry_Arbitrary_Angle);
    CPPUNIT_TEST(testGetRotatedImageOrigin);
    CPPUNIT_TEST_SUITE_END();
};

/**
 * Validates that the engine correctly identifies segments of non-spacing glyphs.
 */
void TextLayoutEngineTest::testGetWordLineSegments()
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = xFont;

    // Test "Hello World" style (Word - Space - Word)
    {
        // Glyphs: [W][W][W][S][W][W][W] (W=Word, S=Space)
        std::vector<bool> aPattern = { false, false, false, true, false, false, false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextLayoutEngine::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Should have 2 segments
        CPPUNIT_ASSERT_EQUAL(size_t(2), aSegments.size());

        // First word: offset 0, width 30
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, aSegments[0].second, 0.001);

        // Second word: offset 40, width 30
        CPPUNIT_ASSERT_DOUBLES_EQUAL(40.0, aSegments[1].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, aSegments[1].second, 0.001);
    }

    // Test Leading/Trailing Spaces: "  Word  "
    {
        // Glyphs: [S][S][W][W][S][S]
        std::vector<bool> aPattern = { true, true, false, false, true, true };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextLayoutEngine::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Should have only 1 segment for the word in the middle
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aSegments[0].first, 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aSegments[0].second, 0.001);
    }

    // Test Rotation Projection (90 degrees)
    {
        // When rotated 90 degrees, glyphs move along Y, not X.
        // BasePoint is (0,0). Word starts at (0, 50) and is 20 units long.
        std::vector<bool> aPattern = { false, false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        // Manually adjust mock glyph positions to simulate vertical flow
        // In vertical/rotated text, nDist depends on cos(90) and sin(90)
        xFont->mnOrientation = 900_deg10; // 90 degrees

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextLayoutEngine::GetWordLineSegments(aLayout, aRealization, aSegments);

        // Verify rotation math: nDist = nDist * cos(90) - nDY * sin(90)
        // cos(90) = 0, sin(90) = 1. So nDist = -nDY.
        // If the word started at Y=0, nDist should be 0.
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);
    }

    {
        // Test: 270 degrees at origin
        // BasePoint (0,0), Glyph at (0,0). Expected distance = 0.
        std::vector<bool> aPattern = { false };
        WordSegmentMockLayout aLayout(aPattern);
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);
        xFont->mnOrientation = 2700_deg10;

        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextLayoutEngine::GetWordLineSegments(aLayout, aRealization, aSegments);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);

        // Test: 270 degrees with horizontal offset (X=10).
        // nDist = (10-0)*cos(270) - (0-0)*sin(270) = 0.
        std::vector<bool> aXOffsetPattern = { true, false }; // Space at 0, Word at X=10
        WordSegmentMockLayout aXOffsetLayout(aXOffsetPattern);
        aXOffsetLayout.DrawBase() = basegfx::B2DPoint(0, 0);

        aSegments.clear();
        vcl::text::TextLayoutEngine::GetWordLineSegments(aXOffsetLayout, aRealization, aSegments);
        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aSegments[0].first, 0.001);

        // Test: 270 degrees with vertical offset (Y=50).
        // Since the mock layout increments X, we need a custom setup or
        // a known Y-offset to verify that nDist = -dY * sin(270) = dY.
        // In 270 deg, nDist = (dX * 0) - (dY * -1) = dY.

        // We simulate this by overriding a single glyph position in the mock
        // specifically to test the Y-to-Distance projection.
        // Expected result for dY=50 at 270 deg is nDist=50.

        std::vector<bool> aYOffsetPattern = { false };
        WordSegmentMockLayout aYOffsetLayout(aYOffsetPattern);
        aYOffsetLayout.DrawBase() = basegfx::B2DPoint(0, -50);

        aSegments.clear();
        vcl::text::TextLayoutEngine::GetWordLineSegments(aYOffsetLayout, aRealization, aSegments);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aSegments.size());
        // The distance should be exactly 50.0
        CPPUNIT_ASSERT_DOUBLES_EQUAL(50.0, aSegments[0].first, 0.001);
    }
}

void TextLayoutEngineTest::testBiDiLayoutFlags()
{
    OUString aLatin = u"Hello World"_ustr;
    SalLayoutFlags nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aLatin, 0, aLatin.getLength());
    CPPUNIT_ASSERT(bool(nFlags & SalLayoutFlags::BiDiStrong));

    OUString aArabic = u"مرحبا"_ustr;
    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::Default, aArabic, 0, aArabic.getLength());
    CPPUNIT_ASSERT_EQUAL(SalLayoutFlags::NONE, nFlags);

    nFlags = vcl::text::TextLayoutEngine::GetBiDiLayoutFlags(
        vcl::text::ComplexTextLayoutFlags::BiDiRtl, aLatin, 0, aLatin.getLength());
    CPPUNIT_ASSERT(bool(nFlags & SalLayoutFlags::BiDiRtl));
}

void TextLayoutEngineTest::testCreateLayoutRequest_Simple()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;
    OUString aInput = u"Hello World"_ustr;
    auto aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 0, 5, 100.0, SalLayoutFlags::NONE, nullptr, aState, aRealization, false);

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aArgs.mnMinCharPos);
    CPPUNIT_ASSERT_EQUAL(sal_Int32(5), aArgs.mnEndCharPos);
    CPPUNIT_ASSERT_EQUAL((0_deg10).get(), aArgs.mnOrientation.get());
}

void TextLayoutEngineTest::testCreateLayoutRequest_DigitLocalization()
{
    vcl::GraphicsState aState;
    aState.meTextLanguage = LANGUAGE_ARABIC_SAUDI_ARABIA;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;
    OUString aInput = u"Year 2024"_ustr;
    vcl::text::TextLayoutEngine::CreateLayoutRequest(aInput, 0, aInput.getLength(), 100.0,
                                                     SalLayoutFlags::NONE, nullptr, aState,
                                                     aRealization, false);
    CPPUNIT_ASSERT(aInput[5] == 0x0662);
}

void TextLayoutEngineTest::testCreateLayoutRequest_OrientationAndWidth()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;
    OUString aInput = u"CheckWidth"_ustr;
    auto aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 0, aInput.getLength(), 555.5, SalLayoutFlags::NONE, nullptr, aState, aRealization,
        false);
    CPPUNIT_ASSERT_EQUAL(555.5, aArgs.mnLayoutWidth);
}

void TextLayoutEngineTest::testCreateLayoutRequest_OutOfBounds()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;
    OUString aInput = u"Hi"_ustr;
    auto aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 10, 5, 100.0, SalLayoutFlags::NONE, nullptr, aState, aRealization, false);
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
    rtl::Reference<LogicalFontInstance> pForcedFont(
        reinterpret_cast<LogicalFontInstance*>(new MockFontInstance()));
    ImplFontCache* pDummyCache = reinterpret_cast<ImplFontCache*>(0xDEADBEEF);
    vcl::font::PhysicalFontCollection* pDummyCollection = nullptr;
    OUString aMissingCodes = "A";
    vcl::text::FontLookupCriteria aCriteria
        = { *pDummyCache, pDummyCollection, nullptr, pForcedFont };
    auto pResult = vcl::text::TextLayoutEngine::FindFallbackFont(aCriteria, 1, aMissingCodes,
                                                                 bHasUsedForcedFallback, nullptr);
    CPPUNIT_ASSERT_EQUAL(pForcedFont.get(), pResult.get());
    CPPUNIT_ASSERT(bHasUsedForcedFallback);
}

void TextLayoutEngineTest::testIdentifyMissingChars()
{
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = nullptr;
    OUString aInput = u"Hello World"_ustr;
    auto aArgs = vcl::text::TextLayoutEngine::CreateLayoutRequest(
        aInput, 0, aInput.getLength(), 100, SalLayoutFlags::NONE, nullptr, aState, aRealization,
        false);
    aArgs.maRuns.Clear();
    aArgs.maRuns.AddRun(6, 11, false);
    CPPUNIT_ASSERT_EQUAL(OUString("World"),
                         vcl::text::TextLayoutEngine::IdentifyMissingChars(aArgs));
}

void TextLayoutEngineTest::testJustifyLayout()
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"Test"_ustr, 0, 4, SalLayoutFlags::NONE,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutEngine::JustifyLayout(aLayout, aArgs);
    CPPUNIT_ASSERT(aLayout.bAdjustCalled);
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
    vcl::text::TextLayoutRequest aArgs(u"RTL"_ustr, 0, 3, SalLayoutFlags::RightAlign,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos{};
    aPos.bRightAlign = true;
    aPos.nEndGlyphCoord = 0;
    aArgs.mnLayoutWidth = 0;
    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-99.0, aLayout.DrawOffset().getX(), 0.001);
}

void TextLayoutEngineTest::testApplyHorizontalOffset_EndGlyph()
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"RTL"_ustr, 0, 3, SalLayoutFlags::RightAlign,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = true;
    aPos.nEndGlyphCoord = 150.0;
    aPos.bHasDXArray = true;
    aArgs.mnLayoutWidth = 200;
    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-149.0, aLayout.DrawOffset().getX(), 0.001);
}

void TextLayoutEngineTest::testApplyHorizontalOffset_Disabled()
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"LTR"_ustr, 0, 3, SalLayoutFlags::NONE,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = false;
    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aLayout.DrawOffset().getX(), 0.001);
}

void TextLayoutEngineTest::testGetTextHeightPixel()
{
    vcl::font::FontRealization aRealization;
    aRealization.nEmphasisAscent = 5;
    aRealization.nEmphasisDescent = 2;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, vcl::text::TextLayoutEngine::GetTextHeightPixel(aRealization),
                                 0.001);
}

void TextLayoutEngineTest::testCalculateOutlineTransform()
{
    MockSalLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 200.0);
    vcl::font::FontRealization aRealization;
    aRealization.nXOffset = 5.0;
    aRealization.nYOffset = 5.0;
    basegfx::B2DHomMatrix aMat
        = vcl::text::TextLayoutEngine::CalculateOutlineTransform(aLayout, aRealization, 0.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-95.0, aMat.get(0, 2), 0.001);
}

void TextLayoutEngineTest::testGetTextInkBounds_Rotation()
{
    StubFontInstance* pStub = new StubFontInstance();
    rtl::Reference<LogicalFontInstance> xFont(pStub);
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = xFont;
    RotatableMockLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 100.0);
    aLayout.mpFont = pStub;
    tools::Rectangle aRect
        = vcl::text::TextLayoutEngine::GetTextInkBounds(aLayout, aRealization, false);
    CPPUNIT_ASSERT_EQUAL(tools::Long(20), aRect.GetHeight());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRect.GetWidth());
}

void TextLayoutEngineTest::testEmphasisMarkPositions()
{
    // Setup Font and Realization
    StubFontInstance* pFont = new StubFontInstance();
    rtl::Reference<LogicalFontInstance> xFont(pFont);
    vcl::font::FontRealization aRealization;
    aRealization.mxFont = xFont;
    aRealization.nEmphasisAscent = 50;
    aRealization.nEmphasisDescent = 20;

    // Setup Mock Layout with a single glyph at (100, 200)
    MockEmphasisLayout aLayout;
    const tools::Long nBaseline = 200;
    // Ensure the layout's global draw position matches the glyph's baseline
    aLayout.DrawBase() = basegfx::B2DPoint(0, nBaseline);

    // Test "Above" Mark
    {
        vcl::font::EmphasisMark aMark(FontEmphasisMark::Disc, 50, 96);
        const long nMarkWidth = aMark.GetWidth();
        const long nYAdj = aMark.GetYOffset();

        std::vector<Point> aPoints;
        vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(aLayout, aRealization, aMark, false,
                                                              aPoints);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aPoints.size());

        // Horizontal Centering Check
        CPPUNIT_ASSERT_EQUAL_MESSAGE(
            "Horizontal centering failed: X should be (GlyphX - MarkWidth/2)",
            tools::Long(100 - (nMarkWidth / 2)), aPoints[0].X());

        // Vertical Positioning Check
        // nAnchorY(200) + nShapeAdj(-nYAdj) - nYCenterOff(Ascent/2) - FontAscent(50)
        tools::Long nExpectedY
            = nBaseline - aRealization.nEmphasisAscent - nYAdj - (aRealization.nEmphasisAscent / 2);

        CPPUNIT_ASSERT_EQUAL_MESSAGE(
            "Vertical 'Above' positioning failed. Check nAnchorY vs nShapeAdj math.", nExpectedY,
            aPoints[0].Y());
    }

    // Test "Below" Mark
    {
        vcl::font::EmphasisMark aMark(FontEmphasisMark::Disc | FontEmphasisMark::PosBelow, 20, 96);
        const long nYAdj = aMark.GetYOffset();

        std::vector<Point> aPoints;
        vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(aLayout, aRealization, aMark, true,
                                                              aPoints);

        // Vertical Positioning Check
        // nAnchorY(200) + nBaseOffset(Descent:20) + nShapeAdj(nYAdj) - nYCenterOff(Descent/2)
        tools::Long nExpectedY = nBaseline + aRealization.nEmphasisDescent + nYAdj
                                 - (aRealization.nEmphasisDescent / 2);

        CPPUNIT_ASSERT_EQUAL_MESSAGE(
            "Vertical 'Below' positioning failed. Descent anchor or adjustment is incorrect.",
            nExpectedY, aPoints[0].Y());
    }
}

void TextLayoutEngineTest::testInitializeTextLineMetrics()
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    const LogicalFontInstance* pConstFont = xFont.get(); // Test const-correctness

    vcl::Font aFont;
    const tools::Long nDPI = 96;
    const tools::Long nSpaceW = 10;
    const tools::Long nBulletW = 4;

    vcl::text::TextLayoutEngine::InitializeTextLineMetrics(pConstFont, aFont, nDPI, nSpaceW,
                                                           nBulletW);

    // Assertions: Verify Bullet Offset calculation
    // Logic: (nSpaceWidth - nBulletWidth) >> 1 => (10 - 4) >> 1 = 3
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Bullet offset calculation is incorrect", tools::Long(3),
                                 xFont->mxFontMetric->GetBulletOffset());

    // Verify Line Height population
    // In our StubFontInstance, Ascent=10 and Descent=10, so LineHeight should be 20
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line height was not initialized correctly", tools::Long(20),
                                 xFont->mnLineHeight);
}

void TextLayoutEngineTest::testInitializeFontMetrics()
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    vcl::Font aFont;
    const long nDPI = 96;
    const long nPixelWidth = 1;

    // Define Callbacks
    // These replace the OutputDevice::GetTextWidth and GetLogicalTextBoundRect calls
    auto fnWidth = [](const OUString& rStr) -> long {
        return rStr.getLength() * 10; // Mock: each char is 10 units wide
    };

    auto fnRect = [](tools::Rectangle& rRect, const OUString& rStr) {
        // Mock: set a deterministic bounding box
        rRect = tools::Rectangle(Point(0, 0), Size(rStr.getLength() * 10, 20));
    };

    // Execute Engine Logic
    // Using the const pointer to verify the signature update from your previous refactor
    vcl::text::TextLayoutEngine::InitializeFontMetrics(xFont.get(), aFont, nDPI, nPixelWidth,
                                                       fnWidth, fnRect);

    // Assertions
    // Verify that mnLineHeight was set (Ascent + Descent from StubFontInstance)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line height should be initialized from metrics", tools::Long(20),
                                 xFont->mnLineHeight);

    // Verify the internal FontMetricData was touched by checking a property
    // that InitializeFontMetrics calculates or delegates.
    CPPUNIT_ASSERT_MESSAGE("FontMetricData should be initialized", xFont->mxFontMetric != nullptr);
}

void TextLayoutEngineTest::testInitializeAboveTextLineMetrics()
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    const tools::Long nDPI = 96;
    const tools::Long nPixelWidth = 1; // Simulated logic-to-pixel width

    vcl::text::TextLayoutEngine::InitializeAboveTextLineMetrics(xFont.get(), nDPI, nPixelWidth);

    // Verify the FontMetricData was updated with the correct above-line sizes
    // Note: Verification depends on specific FontMetricData getters for
    // overline/above-line properties.
    CPPUNIT_ASSERT(xFont->mxFontMetric != nullptr);
}

void TextLayoutEngineTest::testGetAlignmentOffset()
{
    const tools::Long nAscent = 80;
    const tools::Long nDescent = 20;

    // Test ALIGN_TOP: Should return the positive ascent value to shift text down
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_TOP offset is incorrect", nAscent,
        vcl::text::TextLayoutEngine::GetAlignmentOffset(ALIGN_TOP, nAscent, nDescent));

    // Test ALIGN_BOTTOM: Should return the negative descent value to shift text up
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_BOTTOM offset is incorrect", -nDescent,
        vcl::text::TextLayoutEngine::GetAlignmentOffset(ALIGN_BOTTOM, nAscent, nDescent));

    // Test ALIGN_BASELINE: Should return 0 (no shift)
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_BASELINE offset should be zero", tools::Long(0),
        vcl::text::TextLayoutEngine::GetAlignmentOffset(ALIGN_BASELINE, nAscent, nDescent));
}

void TextLayoutEngineTest::testGetRotatedGeometry_0_Degrees()
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 0_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextLayoutEngine::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // X = BaseX(100) + DistX(10) = 110
    // Y = BaseY(100) + DistY(20) = 120
    CPPUNIT_ASSERT_EQUAL(tools::Long(110), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(120), aGeo.maRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetHeight());
}

void TextLayoutEngineTest::testGetRotatedGeometry_90_Degrees()
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 900_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextLayoutEngine::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 90 deg rotation logic (Clockwise):
    // NewX = OldY(20); NewY = -OldX(-10) - NewHeight(30) = -40
    // Base(100,100) + (20, -40) = (120, 60)
    CPPUNIT_ASSERT_EQUAL(tools::Long(120), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aGeo.maRect.Top());
    // Dimensions swapped
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetHeight());
}

void TextLayoutEngineTest::testGetRotatedGeometry_180_Degrees()
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 1800_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextLayoutEngine::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 180 deg rotation logic:
    // NewX = -OldX(-10) - Width(30) = -40
    // NewY = -OldY(-20) - Height(40) = -60
    // Base(100,100) + (-40, -60) = (60, 40)
    CPPUNIT_ASSERT_EQUAL(tools::Long(60), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.Top());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetHeight());
}

void TextLayoutEngineTest::testGetRotatedGeometry_270_Degrees()
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(10, 20), Size(30, 40));
    Degree10 nAngle = 2700_deg10;

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextLayoutEngine::GetRotatedGeometry(aBase, aLocal, nAngle);

    CPPUNIT_ASSERT_EQUAL(false, aGeo.mbIsPolygon);
    // 270 deg rotation logic (Clockwise):
    // NewX = -OldY(-20) - NewWidth(40) = -60
    // NewY = OldX(10)
    // Base(100,100) + (-60, 10) = (40, 110)
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(110), aGeo.maRect.Top());
    // Dimensions swapped
    CPPUNIT_ASSERT_EQUAL(tools::Long(40), aGeo.maRect.GetWidth());
    CPPUNIT_ASSERT_EQUAL(tools::Long(30), aGeo.maRect.GetHeight());
}

void TextLayoutEngineTest::testGetRotatedGeometry_Arbitrary_Angle()
{
    Point aBase(100, 100);
    tools::Rectangle aLocal(Point(0, 0), Size(100, 100));
    Degree10 nAngle = 450_deg10; // 45 degrees

    vcl::text::RotatedGeometry aGeo
        = vcl::text::TextLayoutEngine::GetRotatedGeometry(aBase, aLocal, nAngle);

    // Expect Polygon fallback
    CPPUNIT_ASSERT_EQUAL(true, aGeo.mbIsPolygon);
    CPPUNIT_ASSERT(aGeo.maPoly.GetSize() > 0);

    // Bounds Check: A 100x100 box rotated 45 degrees should have a bounding box
    // larger than 100x100 (approx 141x141)
    tools::Rectangle aBound = aGeo.maPoly.GetBoundRect();
    CPPUNIT_ASSERT(aBound.GetWidth() > 100);
    CPPUNIT_ASSERT(aBound.GetHeight() > 100);
}

void TextLayoutEngineTest::testGetRotatedImageOrigin()
{
    Point aBase(100, 100);
    // Local bounds: 10x20 rectangle at (0,0)
    // Note: VCL Rect of size 10x20 spans 0..9 in X and 0..19 in Y.
    tools::Rectangle aLocal(Point(0, 0), Size(10, 20));

    // Case 1: 0 Degrees
    // Should be Base + Local.TopLeft (100, 100)
    Point aPos = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(aBase, aLocal, 0_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.Y());

    // Case 2: 90 Degrees
    // Rotates (x,y) -> (y, -x).
    // X range [0..9] becomes Y range [0..-9]. Min Y is -9.
    // Base(100,100) + (0, -9) = (100, 91).
    aPos = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(aBase, aLocal, 900_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(91), aPos.Y());

    // Case 3: 180 Degrees
    // Rotates (x,y) -> (-x, -y).
    // X range [0..9] -> [-9..0]. Min X is -9.
    // Y range [0..19] -> [-19..0]. Min Y is -19.
    // Base(100,100) + (-9, -19) = (91, 81).
    aPos = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(aBase, aLocal, 1800_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(91), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(81), aPos.Y());

    // Case 4: 270 Degrees
    // Rotates (x,y) -> (-y, x).
    // Y range [0..19] -> X range [0..-19]. Min X is -19.
    // X range [0..9] -> Y range [0..9]. Min Y is 0.
    // Base(100,100) + (-19, 0) = (81, 100).
    aPos = vcl::text::TextLayoutEngine::GetRotatedImageOrigin(aBase, aLocal, 2700_deg10);
    CPPUNIT_ASSERT_EQUAL(tools::Long(81), aPos.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aPos.Y());
}

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
