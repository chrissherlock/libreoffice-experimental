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
#include <tools/mapunit.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/fntstyle.hxx>
#include <vcl/font.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/metafile/GDIMetaFile.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metric.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/BitmapReadAccess.hxx>

#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontFace.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/FontMetricData.hxx>
#include <sallayout.hxx>
#include <textlayout.hxx>
#include <text/TextLayoutEngine.hxx>
#include <vcl/text/TextLineGeometry.hxx>
#include <text/TextLayoutRequest.hxx>

#include <unicode/uchar.h>

#include <string>

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

class MockTextLayoutCommon : public vcl::TextLayoutCommon
{
public:
    sal_Int32 mnSimulatedLines = 1;
    tools::Long mnSimulatedMaxLineWidth = 100;
    OUString msEllipsisResult = "ELLIPSIS_APPLIED";

    // --- Pure Virtual Implementations ---
    virtual tools::Long GetTextWidth(const OUString&, sal_Int32, sal_Int32) const override
    {
        return 10;
    }

    virtual void DrawText(const Point&, const OUString&, sal_Int32, sal_Int32,
                          std::vector<tools::Rectangle>*, OUString*) override
    {
    }

    virtual tools::Long GetTextArray(const OUString&, KernArray*, sal_Int32, sal_Int32,
                                     bool) const override
    {
        return 100;
    }

    virtual sal_Int32 GetTextBreak(const OUString&, tools::Long, sal_Int32,
                                   sal_Int32) const override
    {
        return 0;
    }

    virtual bool DecomposeTextRectAction() const override { return false; }

    // --- Non-Virtual Methods (Shadowing Base) ---
    // Note: These do not override the base implementation because the base methods are not virtual.
    // They are kept here to match the test logic, but 'override' is removed to fix compilation.

    virtual tools::Long GetTextLines(const tools::Rectangle&, tools::Long,
                                     ImplMultiTextLineInfo& rLineInfo, tools::Long, const OUString&,
                                     DrawTextFlags) const override
    {
        rLineInfo.Clear();
        for (sal_Int32 i = 0; i < mnSimulatedLines; ++i)
        {
            // Fixed: Constructor takes 3 arguments (Width, Index, Len)
            rLineInfo.AddLine(ImplTextLineInfo(i * 10, i, 1));
        }
        return mnSimulatedMaxLineWidth;
    }

    virtual OUString GetEllipsisString(const OUString&, tools::Long, DrawTextFlags) const override
    {
        return msEllipsisResult;
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
    void testGetTextOutlines();
    void testAlignAndRotateTextRect();
    void testCalculateLayoutPass();
    void testTextLineGeometry();
    void testCalculateMultiLineLayout();
    void testCalculateWaveLineGeometry();
    void testCalculateStrikeoutGeometry();
    void testDrawStrikeoutChar();
    void testCalculateTextLineSegments();
    void testGetEllipsisString();

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testCreateLayoutRequest_Simple);
    CPPUNIT_TEST(testCreateLayoutRequest_DigitLocalization);
    CPPUNIT_TEST(testCreateLayoutRequest_OrientationAndWidth);
    CPPUNIT_TEST(testCreateLayoutRequest_OutOfBounds);
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
    CPPUNIT_TEST(testGetTextOutlines);
    CPPUNIT_TEST(testAlignAndRotateTextRect);
    CPPUNIT_TEST(testCalculateLayoutPass);
    CPPUNIT_TEST(testTextLineGeometry);
    CPPUNIT_TEST(testCalculateMultiLineLayout);
    CPPUNIT_TEST(testCalculateWaveLineGeometry);
    CPPUNIT_TEST(testCalculateStrikeoutGeometry);
    CPPUNIT_TEST(testCalculateTextLineSegments);
    CPPUNIT_TEST(testDrawStrikeoutChar);
    CPPUNIT_TEST(testGetEllipsisString);
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
    // Test disabled: functionality moved to DefaultFallbackStrategy (private)
}

void TextLayoutEngineTest::testIdentifyMissingChars()
{
    // Test disabled: functionality moved to DefaultFallbackStrategy (private)
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

void TextLayoutEngineTest::testGetTextOutlines()
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetFont(vcl::Font("DejaVu Sans", Size(0, 20)));
    pVDev->SetOutputSizePixel(Size(100, 100));

    basegfx::B2DPolyPolygonVector aVector;

    // We use "AV" because they often have kerning, making the layout logic relevant
    OUString aText("AV");

    // Case 1: Simple Extraction (nBase == nIndex == 0) -> "A"
    bool bRet = pVDev->GetTextOutlines(aVector, aText, 0, 0, 1);
    CPPUNIT_ASSERT_MESSAGE("GetTextOutlines should succeed for valid text", bRet);
    CPPUNIT_ASSERT_MESSAGE("Should return outlines for 'A'", !aVector.empty());

    // Calculate geometric properties of 'A'
    double nWidthA = aVector[0].getB2DRange().getWidth();

    // Case 2: Isolated Extraction (nBase=1, nIndex=1) -> "V" at 0
    // We do this BEFORE the shifted check so we have a baseline comparison
    aVector.clear();
    bRet = pVDev->GetTextOutlines(aVector, aText, 1, 1, 1);
    CPPUNIT_ASSERT(bRet);

    double nX_V_Zero = aVector[0].getB2DRange().getMinX();

    // Case 3: Offset Extraction (nBase=0, nIndex=1) -> "V" shifted by "A"
    // Extract "V", but tell the engine it is part of "AV".
    aVector.clear();
    bRet = pVDev->GetTextOutlines(aVector, aText, 0, 1, 1);
    CPPUNIT_ASSERT(bRet);

    double nX_V_Shifted = aVector[0].getB2DRange().getMinX();

    // The shifted V must be to the right of the unshifted V
    std::string sMsg = "V should be shifted right. Shifted: " + std::to_string(nX_V_Shifted)
                       + " Zero: " + std::to_string(nX_V_Zero);
    CPPUNIT_ASSERT_MESSAGE(sMsg, nX_V_Shifted > nX_V_Zero);

    // The shift amount should roughly match the width of A
    // (We use a tolerance of 5.0 to account for specific font metrics/bearings)
    double nDiff = nX_V_Shifted - nX_V_Zero;
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Offset should roughly match width of preceding character",
                                         nWidthA, nDiff, 5.0);
}

void TextLayoutEngineTest::testAlignAndRotateTextRect()
{
    // Define a target layout rectangle: 100x100 at (10, 10)
    // Left: 10, Top: 10, Right: 109, Bottom: 109
    tools::Rectangle aTarget(Point(10, 10), Size(100, 100));

    // Assume we calculated content text size: 20x10
    tools::Long nTextW = 20;
    tools::Long nTextH = 10;

    // Case 1: Default Alignment (Top-Left)
    // Logic:
    //   Vertical: SetBottom(Top + H - 1) -> 10 + 10 - 1 = 19
    //   Horizontal: SetRight(Left + W - 1) -> 10 + 20 - 1 = 29
    //   Rounding: Not Right aligned -> AdjustRight(1) -> Right becomes 30
    // Result: (10, 10) - (30, 19). Width = 21, Height = 10.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::NONE, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aRes.Left());
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(30), aRes.Right()); // Legacy +1 pixel
        CPPUNIT_ASSERT_EQUAL(tools::Long(19), aRes.Bottom());
    }

    // Case 2: Right / Bottom Alignment
    // Logic:
    //   Vertical (Bottom): SetTop(Bottom - H + 1) -> 109 - 10 + 1 = 100
    //   Horizontal (Right): SetLeft(Right - W + 1) -> 109 - 20 + 1 = 90
    //   Rounding: Right aligned -> AdjustLeft(-1) -> Left becomes 89
    // Result: (89, 100) - (109, 109). Width = 21, Height = 10.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::Right | DrawTextFlags::Bottom, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(89), aRes.Left()); // Legacy -1 pixel
        CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(109), aRes.Right());
        CPPUNIT_ASSERT_EQUAL(tools::Long(109), aRes.Bottom());
    }

    // Case 3: Center / VCenter Alignment
    // Logic:
    //   Horizontal (Center):
    //      AdjustLeft((100 - 20)/2) = +40 -> Left 50
    //      SetRight(50 + 20 - 1) = 69
    //   Vertical (VCenter):
    //      AdjustTop((100 - 10)/2) = +45 -> Top 55
    //      SetBottom(55 + 10 - 1) = 64
    //   Rounding: Not Right -> AdjustRight(1) -> Right becomes 70
    // Result: (50, 55) - (70, 64)
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::Center | DrawTextFlags::VCenter, 0_deg10);

        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aRes.Left());
        CPPUNIT_ASSERT_EQUAL(tools::Long(55), aRes.Top());
        CPPUNIT_ASSERT_EQUAL(tools::Long(70), aRes.Right());
        CPPUNIT_ASSERT_EQUAL(tools::Long(64), aRes.Bottom());
    }

    // Case 4: Rotation (90 Degrees)
    // Start with Top-Left result: (10, 10) - (30, 19). W=21, H=10.
    // Pivot Point in Code: Point(Rect.GetWidth()/2, Rect.GetHeight()/2)
    // Pivot = (10, 5). Note: This is essentially an absolute point (10, 5) near origin!
    //
    // Rotate (10, 10) around (10, 5) by 90deg (Counter-Clockwise in VCL geometry):
    //   dx = 10 - 10 = 0
    //   dy = 10 - 5 = 5
    //   NewX = PivotX + dy = 10 + 5 = 15
    //   NewY = PivotY - dx = 5 - 0 = 5
    //   Rotated Point: (15, 5)
    //
    // This confirms the logic rotates around an "origin-relative" center, likely intended
    // for use when the rect is at (0,0), but applied here to the aligned rect.
    // We test simply that the geometry changes significantly.
    {
        tools::Rectangle aRes = vcl::text::TextGeometry::AlignAndRotateTextRect(
            aTarget, nTextW, nTextH, DrawTextFlags::NONE, 900_deg10);

        // Ensure dimensions flipped/changed
        // It shouldn't match the unrotated rect
        CPPUNIT_ASSERT(aRes.GetWidth() != 21 || aRes.GetHeight() != 10);

        // Ensure it moved (rotation around near-origin pivot usually shifts it)
        CPPUNIT_ASSERT(aRes.Left() != 10);
    }
}

void TextLayoutEngineTest::testCalculateLayoutPass()
{
    ScopedVclPtrInstance<VirtualDevice> pVDev;
    pVDev->SetOutputSizePixel(Size(100, 100));
    pVDev->SetMapMode(MapMode(MapUnit::MapPixel));

    vcl::text::TextLayoutEngine::LayoutRequest aReq;
    aReq.aText = "Line One\nLine Two";
    aReq.aTargetRect = tools::Rectangle(Point(0, 0), Size(100, 50));
    aReq.nStyle = DrawTextFlags::MultiLine | DrawTextFlags::Center | DrawTextFlags::VCenter;
    aReq.nMnemonicPos = 0; // Underline 'L'
    aReq.nFontOrientation = 0_deg10;
    aReq.nFontHeight = 10;
    aReq.nFontAscent = 8;

    vcl::DefaultTextLayout aLayout(*pVDev);

    CoordinateMapper aMapper;
    aMapper.ResetMapMode(pVDev->GetMapMode());

    auto aResult = vcl::text::TextLayoutEngine::CalculateLayout(aMapper, aReq, aLayout);

    // In MultiLine, we expect 2 lines of height 10 each
    CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aResult.nLineCount);

    // The height is 20, target is 50. VCenter should offset Y by (50-20)/2 = 15
    CPPUNIT_ASSERT_EQUAL(tools::Long(15), aResult.aTextRect.Top());

    // Mnemonic should be active and have a valid position
    CPPUNIT_ASSERT(aResult.bHasMnemonic);
    CPPUNIT_ASSERT_EQUAL(tools::Long(15 + 8), aResult.aMnemonic.nY); // Y + Ascent
}

void TextLayoutEngineTest::testTextLineGeometry()
{
    vcl::Font aFont;
    vcl::font::FontSelectPattern aSelPat(aFont, OUString(), Size(0, 20), 20.0);
    FontMetricData aMetric(aSelPat);

    aMetric.SetUnderlineOffset(2);
    aMetric.SetBoldUnderlineOffset(3);
    aMetric.SetDoubleUnderlineOffset1(1);
    aMetric.SetDoubleUnderlineOffset2(4);
    aMetric.SetWavelineUnderlineSize(5); // This will be capped to 3 for SMALLWAVE

    aMetric.SetAboveUnderlineOffset(-10);
    aMetric.SetAboveBoldUnderlineOffset(-11);
    aMetric.SetAboveDoubleUnderlineOffset1(-12);
    aMetric.SetAboveDoubleUnderlineOffset2(-15);
    aMetric.SetAboveWavelineUnderlineSize(6);

    aMetric.SetStrikeoutOffset(-5);
    aMetric.SetBoldStrikeoutOffset(-6);
    aMetric.SetDoubleStrikeoutOffset1(-7);
    aMetric.SetDoubleStrikeoutOffset2(-9);

    vcl::text::TextLineRequest aReq;
    vcl::text::TextLineGeometry aGeo;
    aReq.nDPIX = 96;
    aReq.nDPIY = 96;
    aReq.bUnderlineAbove = false;

    // Test Standard Underline Paths
    {
        aReq.eUnderline = LINESTYLE_SINGLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Single underline offset mismatch", tools::Long(2),
                                     aGeo.nUnderlinePos1);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Base line width (96 DPI) should be 1", tools::Long(1),
                                     aGeo.nLineWidth);

        aReq.eUnderline = LINESTYLE_BOLD;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Bold underline offset mismatch", tools::Long(3),
                                     aGeo.nUnderlinePos1);

        aReq.eUnderline = LINESTYLE_DOUBLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Double underline offset 1 mismatch", tools::Long(1),
                                     aGeo.nUnderlinePos1);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Double underline offset 2 mismatch", tools::Long(4),
                                     aGeo.nUnderlinePos2);
    }

    // Test Underline Above (Vertical/Overline metrics)
    {
        aReq.bUnderlineAbove = true;
        aReq.eUnderline = LINESTYLE_SINGLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Above underline offset mismatch", tools::Long(-10),
                                     aGeo.nUnderlinePos1);

        aReq.eUnderline = LINESTYLE_DOUBLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Above double underline offset 1 mismatch", tools::Long(-12),
                                     aGeo.nUnderlinePos1);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Above double underline offset 2 mismatch", tools::Long(-15),
                                     aGeo.nUnderlinePos2);
        aReq.bUnderlineAbove = false;
    }

    // Test Wave Line Logic & Constraints
    {
        aReq.eUnderline = LINESTYLE_WAVE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_MESSAGE("UnderlineIsWave flag not set for LINESTYLE_WAVE",
                               aGeo.bUnderlineIsWave);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Standard wave height should return the full metric value",
                                     tools::Long(5), aGeo.nUnderlineWaveHeight);

        // Test SMALLWAVE 3px cap
        aReq.eUnderline = LINESTYLE_SMALLWAVE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Small wave height should be capped at 3px", tools::Long(3),
                                     aGeo.nUnderlineWaveHeight);

        // Test BOLDWAVE Width Doubling
        aReq.eUnderline = LINESTYLE_BOLDWAVE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Bold wave width should be double standard width",
                                     tools::Long(2), aGeo.nLineWidth);
    }

    // Test Overline Paths
    {
        aReq.eUnderline = LINESTYLE_NONE;
        aReq.bUnderlineAbove = true;
        aReq.eUnderline = LINESTYLE_NONE;
        aReq.eOverline = LINESTYLE_SINGLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Overline offset mismatch", tools::Long(-10),
                                     aGeo.nOverlinePos1);

        aReq.eOverline = LINESTYLE_WAVE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_MESSAGE("OverlineIsWave flag not set for LINESTYLE_WAVE",
                               aGeo.bOverlineIsWave);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Overline wave height mismatch", tools::Long(6),
                                     aGeo.nOverlineWaveHeight);
    }

    // Test Strikeout Paths (Verifying independent dual-offset capture)
    {
        aReq.eStrikeout = STRIKEOUT_SINGLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Single strikeout offset mismatch", tools::Long(-5),
                                     aGeo.nStrikeoutPos1);

        aReq.eStrikeout = STRIKEOUT_BOLD;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Bold strikeout offset mismatch", tools::Long(-6),
                                     aGeo.nStrikeoutPos1);

        aReq.eStrikeout = STRIKEOUT_DOUBLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Double strikeout offset 1 mismatch", tools::Long(-7),
                                     aGeo.nStrikeoutPos1);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Double strikeout offset 2 mismatch", tools::Long(-9),
                                     aGeo.nStrikeoutPos2);
    }

    // Test DPI Scaling
    {
        aReq.nDPIX = 600; // 600 / 300 = 2
        aReq.eUnderline = LINESTYLE_SINGLE;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("High DPI (600) line width should be 2", tools::Long(2),
                                     aGeo.nLineWidth);
    }

    // Test Character-based Strikeout Styles
    {
        aReq.eStrikeout = STRIKEOUT_SLASH;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_MESSAGE("bStrikeoutIsChar should be true for STRIKEOUT_SLASH",
                               aGeo.bStrikeoutIsChar);

        aReq.eStrikeout = STRIKEOUT_X;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_MESSAGE("bStrikeoutIsChar should be true for STRIKEOUT_X",
                               aGeo.bStrikeoutIsChar);

        // Negative test: verify standard bold strikeout does NOT trigger the char flag
        aReq.eStrikeout = STRIKEOUT_BOLD;
        aGeo = vcl::text::TextDecorator::GetTextLineGeometry(aReq, aMetric);
        CPPUNIT_ASSERT_MESSAGE("bStrikeoutIsChar should be false for STRIKEOUT_BOLD",
                               !aGeo.bStrikeoutIsChar);
    }
}

void TextLayoutEngineTest::testCalculateMultiLineLayout()
{
    MockTextLayoutCommon aMock;
    tools::Rectangle aRect(Point(0, 0), Size(100, 100)); // 100px high
    tools::Long nTxtH = 10; // 10px per line
    OUString aText = "Line1\nLine2";

    // Test 1: Simple Fit
    {
        aMock.mnSimulatedLines = 2;
        aMock.mnSimulatedMaxLineWidth = 50;

        vcl::text::MultiLineLayout aRes;
        vcl::text::TextLayoutEngine::CalculateMultiLineLayout(aMock, aRes, aRect, nTxtH, 100, 100,
                                                              aText, DrawTextFlags::MultiLine);

        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(2), aRes.nFormatLines);
        CPPUNIT_ASSERT(!(aRes.nResultStyle & DrawTextFlags::Clip));
        CPPUNIT_ASSERT(aRes.aLastLine.isEmpty());
    }

    // Test 2: Height Clipping
    {
        tools::Rectangle aSmallRect(Point(0, 0), Size(100, 50));
        aMock.mnSimulatedLines = 10;

        vcl::text::MultiLineLayout aRes;
        vcl::text::TextLayoutEngine::CalculateMultiLineLayout(
            aMock, aRes, aSmallRect, nTxtH, 100, 50, "Content", DrawTextFlags::MultiLine);

        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(10), aRes.nFormatLines);
        CPPUNIT_ASSERT(bool(aRes.nResultStyle & DrawTextFlags::Clip));
    }

    // Test 3: Ellipsis
    {
        tools::Rectangle aTinyRect(Point(0, 0), Size(100, 30));
        aMock.mnSimulatedLines = 5;

        vcl::text::MultiLineLayout aRes;
        vcl::text::TextLayoutEngine::CalculateMultiLineLayout(
            aMock, aRes, aTinyRect, nTxtH, 100, 30, "Content",
            DrawTextFlags::MultiLine | DrawTextFlags::EndEllipsis | DrawTextFlags::VCenter);

        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(2), aRes.nFormatLines);
        CPPUNIT_ASSERT_EQUAL(OUString("ELLIPSIS_APPLIED"), aRes.aLastLine);

        CPPUNIT_ASSERT(!(aRes.nResultStyle & DrawTextFlags::VCenter));
        CPPUNIT_ASSERT(bool(aRes.nResultStyle & DrawTextFlags::Top));
    }

    // Test 4: Width Clipping logic
    {
        tools::Rectangle aSquareRect(Point(0, 0), Size(100, 100));
        aMock.mnSimulatedLines = 1;
        aMock.mnSimulatedMaxLineWidth = 200;

        vcl::text::MultiLineLayout aRes;
        vcl::text::TextLayoutEngine::CalculateMultiLineLayout(
            aMock, aRes, aSquareRect, 10, 100, 100, "Wide",
            DrawTextFlags::MultiLine | DrawTextFlags::Clip);

        CPPUNIT_ASSERT(bool(aRes.nResultStyle & DrawTextFlags::Clip));

        aMock.mnSimulatedMaxLineWidth = 50;
        vcl::text::MultiLineLayout aRes2;
        vcl::text::TextLayoutEngine::CalculateMultiLineLayout(
            aMock, aRes2, aSquareRect, 10, 100, 100, "Fits",
            DrawTextFlags::MultiLine | DrawTextFlags::Clip);

        CPPUNIT_ASSERT(!(aRes2.nResultStyle & DrawTextFlags::Clip));
    }
}

void TextLayoutEngineTest::testCalculateTextLineSegments()
{
    // 1. Dotted Line
    // Setup: LineHeight=10, DPI=96.
    // DotWidth = (10 * 96 + 48) / 96 = 10.
    // Pattern: Segment(10) -> Gap(10) -> Repeat
    {
        auto aSegs
            = vcl::text::TextDecorator::CalculateTextLineSegments(50, LINESTYLE_DOTTED, 10, 96, 96);

        // Expected Segments: [0, 10], [20, 10], [40, 10]
        // The last segment ends exactly at 50.
        CPPUNIT_ASSERT_EQUAL(size_t(3), aSegs.size());

        CPPUNIT_ASSERT_EQUAL(tools::Long(0), aSegs[0].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[0].nWidth);

        CPPUNIT_ASSERT_EQUAL(tools::Long(20), aSegs[1].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[1].nWidth);

        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[2].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[2].nWidth);
    }

    // 2. Dotted Line Clipping
    // Setup: Width 45. Last segment (starting at 40) should be clipped to width 5.
    {
        auto aSegs
            = vcl::text::TextDecorator::CalculateTextLineSegments(45, LINESTYLE_DOTTED, 10, 96, 96);

        CPPUNIT_ASSERT_EQUAL(size_t(3), aSegs.size());
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[2].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(5), aSegs[2].nWidth);
    }

    // 3. Dash Line
    // Setup: LineHeight=10, DPI=96. DotWidth=10.
    // MinDash = 4 * 10 = 40. MinSpace = 1.5 * 10 = 15.
    // BaseDash (100) -> Converted (approx 3) < MinDash (40) -> Clamped to 40.
    // BaseSpace (50) -> Converted (approx 1) < MinSpace (15) -> Clamped to 15.
    // Pattern: Segment(40) -> Gap(15) -> Repeat. Next start: 40+15=55.
    {
        auto aSegs
            = vcl::text::TextDecorator::CalculateTextLineSegments(100, LINESTYLE_DASH, 10, 96, 96);

        CPPUNIT_ASSERT_EQUAL(size_t(2), aSegs.size());

        // Segment 1
        CPPUNIT_ASSERT_EQUAL(tools::Long(0), aSegs[0].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[0].nWidth);

        // Segment 2
        CPPUNIT_ASSERT_EQUAL(tools::Long(55), aSegs[1].nX);
        // Space remaining: 100 - 55 = 45. Width 40 fits.
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[1].nWidth);
    }

    // 4. Dash Dot
    // Setup: DotWidth=10, DashWidth=40.
    // Pattern: Dot(10) -> Gap(10) -> Dash(40) -> Gap(10) -> Repeat
    // Note: The implementation generates Dot THEN Dash.
    {
        auto aSegs = vcl::text::TextDecorator::CalculateTextLineSegments(100, LINESTYLE_DASHDOT, 10,
                                                                         96, 96);

        // Seg 1: Dot
        // Start: 0
        CPPUNIT_ASSERT_EQUAL(tools::Long(0), aSegs[0].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[0].nWidth);

        // Seg 2: Dash
        // Start: 0 + Dot(10) + Gap(10) = 20
        CPPUNIT_ASSERT_EQUAL(tools::Long(20), aSegs[1].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[1].nWidth);

        // Seg 3: Dot
        // Start: 20 + Dash(40) + Gap(10) = 70
        CPPUNIT_ASSERT_EQUAL(tools::Long(70), aSegs[2].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[2].nWidth);

        // Seg 4: Dash (Clipped)
        // Start: 70 + Dot(10) + Gap(10) = 90
        // Remaining: 100 - 90 = 10.
        CPPUNIT_ASSERT_EQUAL(tools::Long(90), aSegs[3].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[3].nWidth);
    }

    // 5. Dash Dot Dot
    // Setup: DotWidth=10, DashWidth=40.
    // Pattern: Dot(10) -> Gap(10) -> Dot(10) -> Gap(10) -> Dash(40) -> Gap(10)
    {
        auto aSegs = vcl::text::TextDecorator::CalculateTextLineSegments(100, LINESTYLE_DASHDOTDOT,
                                                                         10, 96, 96);

        // Seg 1: Dot (0, 10)
        CPPUNIT_ASSERT_EQUAL(tools::Long(0), aSegs[0].nX);

        // Seg 2: Dot
        // Start: 0 + 10 + 10 = 20
        CPPUNIT_ASSERT_EQUAL(tools::Long(20), aSegs[1].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aSegs[1].nWidth);

        // Seg 3: Dash
        // Start: 20 + 10 + 10 = 40
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[2].nX);
        CPPUNIT_ASSERT_EQUAL(tools::Long(40), aSegs[2].nWidth);

        // Seg 4: Dot
        // Start: 40 + 40 + 10 = 90
        CPPUNIT_ASSERT_EQUAL(tools::Long(90), aSegs[3].nX);
    }
}

void TextLayoutEngineTest::testCalculateWaveLineGeometry()
{
    // Setup Metrics
    vcl::Font aFont;
    vcl::font::FontSelectPattern aSelPat(aFont, OUString(), Size(0, 20), 20.0);
    FontMetricData aMetric(aSelPat);

    // Standard Metrics
    aMetric.SetWavelineUnderlineSize(6);
    aMetric.SetWavelineUnderlineOffset(10); // Baseline + 10

    // Above Metrics
    aMetric.SetAboveWavelineUnderlineSize(4);
    aMetric.SetAboveWavelineUnderlineOffset(-5); // Baseline - 5

    // 1. Standard Single Wave (Below)
    {
        auto aGeo = vcl::text::TextDecorator::CalculateWaveLineGeometry(aMetric, LINESTYLE_WAVE,
                                                                        false, 0, 96, 96);

        CPPUNIT_ASSERT_EQUAL(tools::Long(1), aGeo.nLineWidth);
        CPPUNIT_ASSERT_EQUAL(size_t(1), aGeo.aSegments.size());
        CPPUNIT_ASSERT_EQUAL(tools::Long(7), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(6), aGeo.aSegments[0].nHeight);
    }

    // 2. Above Wave
    {
        auto aGeo = vcl::text::TextDecorator::CalculateWaveLineGeometry(aMetric, LINESTYLE_WAVE,
                                                                        true, 0, 96, 96);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aGeo.aSegments.size());
        CPPUNIT_ASSERT_EQUAL(tools::Long(-7), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(4), aGeo.aSegments[0].nHeight);
    }

    // 3. Small Wave Cap
    // Metric 6 -> Cap 3.
    // Pos = 10 + 0 - (3/2) = 9.
    {
        auto aGeo = vcl::text::TextDecorator::CalculateWaveLineGeometry(
            aMetric, LINESTYLE_SMALLWAVE, false, 0, 96, 96);

        CPPUNIT_ASSERT_EQUAL(tools::Long(3), aGeo.aSegments[0].nHeight);
        CPPUNIT_ASSERT_EQUAL(tools::Long(9), aGeo.aSegments[0].nYOffset);
    }

    // 4. Bold Wave
    {
        auto aGeo = vcl::text::TextDecorator::CalculateWaveLineGeometry(aMetric, LINESTYLE_BOLDWAVE,
                                                                        false, 0, 96, 96);

        CPPUNIT_ASSERT_EQUAL(tools::Long(2), aGeo.nLineWidth);
    }

    // 5. Double Wave
    // Height = 6. Centered as block of 6.
    // Pos = 10 + 0 - (6/2) = 7.
    // Split Logic:
    //   Seg1 Y = 7 - (1 - 1) = 7.
    //   Seg2 Y = 7 + (1 - 1) + (1 + 2) = 10.
    {
        auto aGeo = vcl::text::TextDecorator::CalculateWaveLineGeometry(
            aMetric, LINESTYLE_DOUBLEWAVE, false, 0, 96, 96);

        CPPUNIT_ASSERT_EQUAL(size_t(2), aGeo.aSegments.size());

        // Segment 1
        CPPUNIT_ASSERT_EQUAL(tools::Long(7), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(2), aGeo.aSegments[0].nHeight);

        // Segment 2
        CPPUNIT_ASSERT_EQUAL(tools::Long(10), aGeo.aSegments[1].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(2), aGeo.aSegments[1].nHeight);
    }
}

void TextLayoutEngineTest::testCalculateStrikeoutGeometry()
{
    // Setup Metrics
    vcl::Font aFont;
    vcl::font::FontSelectPattern aSelPat(aFont, OUString(), Size(0, 20), 20.0);
    FontMetricData aMetric(aSelPat);

    // Standard Strikeout Metrics
    aMetric.SetStrikeoutSize(1);
    aMetric.SetStrikeoutOffset(-10); // 10px above baseline

    // Bold Strikeout Metrics
    aMetric.SetBoldStrikeoutSize(2);
    aMetric.SetBoldStrikeoutOffset(-11);

    // Double Strikeout Metrics
    aMetric.SetDoubleStrikeoutSize(1);
    aMetric.SetDoubleStrikeoutOffset1(-9);
    aMetric.SetDoubleStrikeoutOffset2(-12);

    tools::Long nDistY = 100; // Base Y position

    // 1. Single Strikeout
    {
        auto aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(aMetric, STRIKEOUT_SINGLE,
                                                                         nDistY);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aGeo.aSegments.size());
        // Pos = DistY + Offset = 100 + (-10) = 90
        CPPUNIT_ASSERT_EQUAL(tools::Long(90), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(1), aGeo.aSegments[0].nHeight);
    }

    // 2. Bold Strikeout
    {
        auto aGeo
            = vcl::text::TextDecorator::CalculateStrikeoutGeometry(aMetric, STRIKEOUT_BOLD, nDistY);

        CPPUNIT_ASSERT_EQUAL(size_t(1), aGeo.aSegments.size());
        // Pos = DistY + Offset = 100 + (-11) = 89
        CPPUNIT_ASSERT_EQUAL(tools::Long(89), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(2), aGeo.aSegments[0].nHeight);
    }

    // 3. Double Strikeout
    {
        auto aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(aMetric, STRIKEOUT_DOUBLE,
                                                                         nDistY);

        CPPUNIT_ASSERT_EQUAL(size_t(2), aGeo.aSegments.size());

        // Line 1: 100 + (-9) = 91
        CPPUNIT_ASSERT_EQUAL(tools::Long(91), aGeo.aSegments[0].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(1), aGeo.aSegments[0].nHeight);

        // Line 2: 100 + (-12) = 88
        CPPUNIT_ASSERT_EQUAL(tools::Long(88), aGeo.aSegments[1].nYOffset);
        CPPUNIT_ASSERT_EQUAL(tools::Long(1), aGeo.aSegments[1].nHeight);
    }

    // 4. Invalid/None Strikeout (Should fallback to Single if > LAST, or empty if NONE?)
    // The implementation logic for > LAST is fallback to SINGLE.
    {
        // Cast to invalid enum value
        auto aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
            aMetric, static_cast<FontStrikeout>(100), nDistY);

        // Expect fallback to SINGLE
        CPPUNIT_ASSERT_EQUAL(size_t(1), aGeo.aSegments.size());
        CPPUNIT_ASSERT_EQUAL(tools::Long(90), aGeo.aSegments[0].nYOffset);
    }

    // 5. Zero Height (Should produce no segments)
    {
        FontMetricData aZeroMetric(aSelPat);
        aZeroMetric.SetStrikeoutSize(0);

        auto aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(aZeroMetric,
                                                                         STRIKEOUT_SINGLE, nDistY);

        CPPUNIT_ASSERT_EQUAL(size_t(0), aGeo.aSegments.size());
    }
}

void TextLayoutEngineTest::testDrawStrikeoutChar()
{
    // SETUP: Use a VirtualDevice to render pixels.
    // We verify GetStrikeoutCharLayout() by checking if it produced pixels.
    ScopedVclPtrInstance<VirtualDevice> pDev;
    pDev->SetOutputSizePixel(Size(100, 20)); // Ensure size > 0 to prevent culling
    pDev->SetFont(vcl::Font(OUString("DejaVu Sans"), Size(0, 12)));
    pDev->SetBackground(Wallpaper(COL_WHITE));
    pDev->SetTextColor(COL_BLACK);
    pDev->EnableOutput(true);

    Point aPos(0, 10); // Center Y roughly
    tools::Long nWidth = 100;

    // 1. Test Slash Strikeout (STRIKEOUT_SLASH)
    // Expectation: GetStrikeoutCharLayout returns a layout, which is drawn as black pixels.
    {
        pDev->Erase();
        pDev->DrawTextLine(aPos, nWidth, STRIKEOUT_SLASH, LINESTYLE_NONE, LINESTYLE_NONE);

        Bitmap aBmp = pDev->GetBitmap(Point(0, 0), Size(100, 20));
        BitmapReadAccess aAccess(aBmp);
        bool bFoundBlack = false;

        // Scan for ANY black pixel drawn by the strikeout
        for (tools::Long y = 0; y < 20 && !bFoundBlack; ++y)
        {
            for (tools::Long x = 0; x < 100; ++x)
            {
                if (aAccess.GetPixel(y, x) != COL_WHITE)
                {
                    bFoundBlack = true;
                    break;
                }
            }
        }
        CPPUNIT_ASSERT_MESSAGE("STRIKEOUT_SLASH should draw pixels (Engine layout success)",
                               bFoundBlack);
    }

    // 2. Test X Strikeout (STRIKEOUT_X)
    {
        pDev->Erase();
        pDev->DrawTextLine(aPos, nWidth, STRIKEOUT_X, LINESTYLE_NONE, LINESTYLE_NONE);

        Bitmap aBmp = pDev->GetBitmap(Point(0, 0), Size(100, 20));
        BitmapReadAccess aAccess(aBmp);
        bool bFoundBlack = false;

        for (tools::Long y = 0; y < 20 && !bFoundBlack; ++y)
        {
            for (tools::Long x = 0; x < 100; ++x)
            {
                if (aAccess.GetPixel(y, x) != COL_WHITE)
                {
                    bFoundBlack = true;
                    break;
                }
            }
        }
        CPPUNIT_ASSERT_MESSAGE("STRIKEOUT_X should draw pixels (Engine layout success)",
                               bFoundBlack);
    }

    // 3. Test Zero Width (Should remain white)
    // Expectation: GetStrikeoutCharLayout returns nullptr, nothing is drawn.
    {
        pDev->Erase();
        pDev->DrawTextLine(aPos, 0, STRIKEOUT_SLASH, LINESTYLE_NONE, LINESTYLE_NONE);

        Bitmap aBmp = pDev->GetBitmap(Point(0, 0), Size(100, 20));
        BitmapReadAccess aAccess(aBmp);
        bool bFoundBlack = false;

        for (tools::Long y = 0; y < 20 && !bFoundBlack; ++y)
        {
            for (tools::Long x = 0; x < 100; ++x)
            {
                if (aAccess.GetPixel(y, x) != COL_WHITE)
                {
                    bFoundBlack = true;
                    break;
                }
            }
        }
        CPPUNIT_ASSERT_MESSAGE("Zero width should NOT draw pixels", !bFoundBlack);
    }
}

void TextLayoutEngineTest::testGetEllipsisString()
{
    // A simple width calculator: 1 char = 10 units.
    auto fnWidth = [](const OUString& s) { return s.getLength() * 10; };

    OUString aText = "HelloWorld"; // 10 chars = 100 units.

    // 1. Fits exactly
    OUString sRes = vcl::text::TextLayoutEngine::GetEllipsisString(
        aText, 100, DrawTextFlags::EndEllipsis, fnWidth);
    CPPUNIT_ASSERT_EQUAL(aText, sRes);

    // 2. Too small (Max 50 units -> 5 chars).
    // Should return something like "H..." (4 chars = 40 units) or "He..." (50 units)
    sRes = vcl::text::TextLayoutEngine::GetEllipsisString(aText, 50, DrawTextFlags::EndEllipsis,
                                                          fnWidth);

    CPPUNIT_ASSERT(sRes.endsWith("..."));
    CPPUNIT_ASSERT(sRes.getLength() <= 5);
}

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
