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
#include <vcl/virdev.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/font.hxx>

#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontFace.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/FontMetricData.hxx>

#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>
#include <GraphicsState.hxx>
#include <text/TextLayoutRequest.hxx>

namespace
{
// Helper to access protected members of OutputDevice for testing
class TestDevice : public VirtualDevice
{
public:
    using VirtualDevice::LayoutText;
    TestDevice()
        : VirtualDevice()
    {
    }
};

// Stub for PhysicalFontFace to prevent segfaults and HarfBuzz assertions
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

    // Required to prevent HarfBuzz crash when Engine queries metrics
    virtual hb_blob_t* GetHbTable(hb_tag_t) const override { return nullptr; }
};

class StubFontInstance : public LogicalFontInstance
{
public:
    StubFontInstance()
        : LogicalFontInstance(*new StubPhysicalFontFace(),
                              vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0))
    {
        // Initialize metric with 10x10 dimensions
        mxFontMetric
            = new FontMetricData(vcl::font::FontSelectPattern(vcl::Font(), "", Size(), 0.0));
        mnLineHeight = 20;
        mxFontMetric->SetAscent(10);
        mxFontMetric->SetDescent(10);
        mnOrientation = 0_deg10;
    }

    virtual void ImplGetGlyphWidths(const sal_GlyphId*, bool, tools::Long*, int) const {}

    // Return a valid 10x20 bounding box for any glyph
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

// --------------------------------------------------------------------------
// MOCK LAYOUTS
// --------------------------------------------------------------------------

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

    // Default stubs
    virtual double FillDXArray(std::vector<double>* pDXArray, const OUString&) const override
    {
        if (pDXArray)
            std::fill(pDXArray->begin(), pDXArray->end(), 10.0);
        return 0.0;
    }

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

    // By default, return false to force manual calculation in some tests,
    // or true in others. Defaulting to false is safer for Engine tests.
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
            rPos = basegfx::B2DPoint(100.0, 200.0);
            nStart++;
            return true;
        }
        return false;
    }
};

// Simplified Mock for Rotation Test
// Simulates a horizontal run of 2 glyphs. Width ~110px.
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

    // Force Engine to calculate bounds manually

    virtual bool GetNextGlyph(const GlyphItem** pGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance** ppFont) const override
    {
        if (ppFont)
            *ppFont = mpFont;

        switch (nStart)
        {
            case 0:
                *pGlyph = &mGlyph1;
                rPos = basegfx::B2DPoint(100.0, 100.0);
                nStart++;
                return true;
            case 1:
                *pGlyph = &mGlyph2;
                rPos = basegfx::B2DPoint(200.0, 100.0);
                nStart++;
                return true;
            default:
                return false;
        }
    }
};

// --------------------------------------------------------------------------
// TEST SUITE
// --------------------------------------------------------------------------

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
    CPPUNIT_TEST_SUITE_END();
};

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

    CPPUNIT_ASSERT(aInput[5] == 0x0662); // '2' -> 0x0662
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

    aArgs.mnLayoutWidth = 250;
    vcl::text::TextLayoutEngine::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-249.0, aLayout.DrawOffset().getX(), 0.001);
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
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    aRealization.nEmphasisAscent = 5;
    aRealization.nEmphasisDescent = 2;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, vcl::text::TextLayoutEngine::GetTextHeightPixel(aRealization),
                                 0.001);
}

void TextLayoutEngineTest::testEmphasisMarkPositions()
{
    MockEmphasisLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(10.0, 100.0);
    vcl::font::FontRealization aRealization;
    aRealization.nEmphasisAscent = 50;
    aRealization.nEmphasisDescent = 20;

    std::vector<Point> aPoints;
    vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(aLayout, aRealization, true, aPoints);
    CPPUNIT_ASSERT_EQUAL(size_t(1), aPoints.size());
    CPPUNIT_ASSERT_EQUAL(tools::Long(120), aPoints[0].Y());

    aPoints.clear();
    vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(aLayout, aRealization, false, aPoints);
    CPPUNIT_ASSERT_EQUAL(size_t(1), aPoints.size());
    CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPoints[0].Y());
}

void TextLayoutEngineTest::testCalculateOutlineTransform()
{
    MockSalLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 200.0);
    vcl::font::FontRealization aRealization;
    aRealization.nXOffset = 5.0;
    aRealization.nYOffset = 5.0;

    {
        basegfx::B2DHomMatrix aMat
            = vcl::text::TextLayoutEngine::CalculateOutlineTransform(aLayout, aRealization, 0.0);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-95.0, aMat.get(0, 2), 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-195.0, aMat.get(1, 2), 0.001);
    }
    {
        double nXOffset = 10.0;
        basegfx::B2DHomMatrix aMat = vcl::text::TextLayoutEngine::CalculateOutlineTransform(
            aLayout, aRealization, nXOffset);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-105.0, aMat.get(0, 2), 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-195.0, aMat.get(1, 2), 0.001);
    }
    {
        aRealization.nXOffset = 0;
        aRealization.nYOffset = 0;
        aLayout.DrawBase() = basegfx::B2DPoint(0, 0);
        basegfx::B2DHomMatrix aMat
            = vcl::text::TextLayoutEngine::CalculateOutlineTransform(aLayout, aRealization, 0.0);
        CPPUNIT_ASSERT(aMat.isIdentity());
    }
}

void TextLayoutEngineTest::testGetTextInkBounds_Rotation()
{
    StubFontInstance* pStub = new StubFontInstance();
    rtl::Reference<LogicalFontInstance> xFont(pStub);

    vcl::font::FontRealization aRealization;
    aRealization.mxFont = xFont;
    aRealization.nEmphasisAscent = 0;
    aRealization.nEmphasisDescent = 0;

    RotatableMockLayout aLayout;
    aLayout.DrawBase() = basegfx::B2DPoint(100.0, 100.0);
    aLayout.mpFont = pStub;

    // Test Unrotated
    {
        tools::Rectangle aRect
            = vcl::text::TextLayoutEngine::GetTextInkBounds(aLayout, aRealization, false);

        CPPUNIT_ASSERT_EQUAL(tools::Long(20), aRect.GetHeight());
        CPPUNIT_ASSERT_EQUAL(tools::Long(100), aRect.GetWidth());
    }

    // 4. Test Rotated
    pStub->mnOrientation = 900_deg10; // 90 degrees
    pStub->mxFontMetric->SetOrientation(900_deg10);

    {
        tools::Rectangle aRect
            = vcl::text::TextLayoutEngine::GetTextInkBounds(aLayout, aRealization, true);

        // Coordinate rotation can cause Top > Bottom. Normalize first.
        aRect.Normalize();

        // At 90 degrees, the 110px width becomes 110px height.
        CPPUNIT_ASSERT_MESSAGE("Rotated Height should contain text width", aRect.GetHeight() >= 90);
        // And the 20px height becomes 20px width.
        CPPUNIT_ASSERT_MESSAGE("Rotated Width should contain line height", aRect.GetWidth() <= 30);
    }
}

} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */