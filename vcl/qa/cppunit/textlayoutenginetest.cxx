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
#include <text/TextLayoutPositioning.hxx>
#include <vcl/text/MultiLineEngine.hxx>
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
    void testGetTextHeightPixel();
    void testEmphasisMarkPositions();
    void testTextLineGeometry();
    void testCalculateMultiLineLayout();
    void testCalculateWaveLineGeometry();
    void testCalculateStrikeoutGeometry();
    void testGetStrikeoutCharLayout();
    void testCalculateTextLineSegments();
    void testGetEllipsisString();

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testCreateLayoutRequest_Simple);
    CPPUNIT_TEST(testCreateLayoutRequest_DigitLocalization);
    CPPUNIT_TEST(testCreateLayoutRequest_OrientationAndWidth);
    CPPUNIT_TEST(testCreateLayoutRequest_OutOfBounds);
    CPPUNIT_TEST(testGetTextHeightPixel);
    CPPUNIT_TEST(testEmphasisMarkPositions);
    CPPUNIT_TEST(testTextLineGeometry);
    CPPUNIT_TEST(testCalculateWaveLineGeometry);
    CPPUNIT_TEST(testCalculateStrikeoutGeometry);
    CPPUNIT_TEST(testCalculateTextLineSegments);
    CPPUNIT_TEST(testGetStrikeoutCharLayout);
    CPPUNIT_TEST(testGetEllipsisString);
    CPPUNIT_TEST_SUITE_END();
};

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

void TextLayoutEngineTest::testGetTextHeightPixel()
{
    vcl::font::FontRealization aRealization;
    aRealization.nEmphasisAscent = 5;
    aRealization.nEmphasisDescent = 2;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, vcl::text::TextLayoutEngine::GetTextHeightPixel(aRealization),
                                 0.001);
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
        vcl::text::TextDecorator::GetEmphasisMarkPositions(aLayout, aRealization, aMark, false,
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
        vcl::text::TextDecorator::GetEmphasisMarkPositions(aLayout, aRealization, aMark, true,
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

void TextLayoutEngineTest::testGetStrikeoutCharLayout()
{
    // SETUP: Use a VirtualDevice to render pixels.
    // NOTE: We test GetStrikeoutCharLayout indirectly via VirtualDevice because
    // constructing a LayoutResources struct requires VCL-internal classes (like CoordinateMapper)
    // that are not exported to the CppunitTest binary.
    // By drawing a strikeout line, we verify that the engine successfully generated the layout.
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
    OUString sRes = vcl::text::MultiLineEngine::GetEllipsisString(
        aText, 100, DrawTextFlags::EndEllipsis, fnWidth);
    CPPUNIT_ASSERT_EQUAL(aText, sRes);

    // 2. Too small (Max 50 units -> 5 chars).
    // Should return something like "H..." (4 chars = 40 units) or "He..." (50 units)
    sRes = vcl::text::MultiLineEngine::GetEllipsisString(aText, 50, DrawTextFlags::EndEllipsis,
                                                         fnWidth);

    CPPUNIT_ASSERT(sRes.endsWith("..."));
    CPPUNIT_ASSERT(sRes.getLength() <= 5);
}

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
