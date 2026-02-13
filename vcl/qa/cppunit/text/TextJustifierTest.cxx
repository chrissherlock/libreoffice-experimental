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

#include <vcl/text/LayoutResources.hxx>

#include <font/FontController.hxx>
#include <sallayout.hxx>
#include <text/TextJustifier.hxx>
#include <text/TextLayoutRequest.hxx>
#include <text/TextLayoutPositioning.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

using namespace vcl::text;

namespace
{
class MockKashidaLayout : public SalLayout
{
public:
    double mnWidth = 0.0;

public:
    bool m_bAdjustCalled = false;

public:
    virtual void DrawText(SalGraphics&) const override {}
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void AdjustLayout(vcl::text::TextLayoutRequest&) override { m_bAdjustCalled = true; }

    // Test Hooks
    virtual bool HasFontKashidaPositions() const override { return true; }
    virtual bool IsKashidaPosValid(int nIdx, int /*nNext*/) const override
    {
        // Mock logic: Valid if index is even
        return (nIdx % 2) == 0;
    }

    // Required stubs

    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual double GetTextWidth() const override { return mnWidth; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, sal_Int32,
                                      sal_Int32) const override
    {
        return 0;
    }
    virtual bool GetNextGlyph(const GlyphItem**, basegfx::B2DPoint&, int&,
                              const LogicalFontInstance**) const override
    {
        return false;
    }
};

class TextJustifierTest : public CppUnit::TestFixture
{
};

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testGetWordKashidaPositions)
{
    MockKashidaLayout aLayout;
    OUString aText = u"ABC"_ustr;
    std::vector<bool> aResult;

    // Call the static method
    TextJustifier::GetWordKashidaPositions(aLayout, aText, aResult);

    // Verification
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Result map size should match text length", size_t(3),
                                 aResult.size());

    // Check specific mock logic (Even=True, Odd=False)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Index 0 (Even) should be valid", true, bool(aResult[0]));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Index 1 (Odd) should be invalid", false, bool(aResult[1]));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Index 2 (Even) should be valid", true, bool(aResult[2]));
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testPrepareJustification)
{
    CoordinateMapper aMapper; // Default is pixel mode (Identity)
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    vcl::text::LayoutResources aRes{ nullptr, aMapper, nullptr, nullptr, nullptr,
                                     nullptr, false,   false,   aState,  aRealization };

    OUString aText = u"ABCD"_ustr;
    std::vector<double> aDX = { 10.0, 20.0, 30.0, 40.0 }; // Cumulative widths
    std::vector<sal_Bool> aKashida = { 0, 1, 0, 0 }; // Kashida at index 1 ('B')

    vcl::text::TextLayoutRequest aReq(aText, 0, 4, SalLayoutFlags::NONE,
                                      LanguageTag(LANGUAGE_ENGLISH), nullptr);
    double nEndCoord = 0.0;

    TextJustifier::PrepareJustification(aRes, aDX, aKashida, 0, 4, std::nullopt, std::nullopt, aReq,
                                        nEndCoord);

    // Assert End Coordinate
    // Since Mapper is Identity, last DX is 40.0. round(40.0) -> 40.0
    CPPUNIT_ASSERT_DOUBLES_EQUAL(40.0, nEndCoord, 0.001);

    // Assert Justification Data
    const JustificationData& rData = aReq.mstJustification;
    CPPUNIT_ASSERT_MESSAGE("Justification data should not be empty", !rData.empty());

    // Check DX array values (TotalAdvance)
    // Index 0: 10
    // Index 1: 20
    CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, rData.GetTotalAdvance(0), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, rData.GetTotalAdvance(1), 0.001);

    // Check Kashida
    // Index 1 should have kashida
    CPPUNIT_ASSERT_EQUAL(true, bool(rData.GetPositionHasKashida(1).value_or(false)));
    // Index 0 should not
    CPPUNIT_ASSERT_EQUAL(false, bool(rData.GetPositionHasKashida(0).value_or(false)));
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testJustifyLayout)
{
    MockKashidaLayout aLayout;
    OUString aText = u"Justify"_ustr;
    vcl::text::TextLayoutRequest aReq(aText, 0, 7, SalLayoutFlags::NONE,
                                      LanguageTag(LANGUAGE_ENGLISH), nullptr);

    // Act
    TextJustifier::JustifyLayout(aLayout, aReq);

    // Assert that the layout's AdjustLayout method was triggered
    CPPUNIT_ASSERT_MESSAGE("JustifyLayout should call SalLayout::AdjustLayout",
                           aLayout.m_bAdjustCalled);
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testApplyHorizontalOffset)
{
    MockKashidaLayout aLayout;
    OUString aText = u"RTL"_ustr;
    vcl::text::TextLayoutRequest aReq(aText, 0, 3, SalLayoutFlags::RightAlign,
                                      LanguageTag(LANGUAGE_ENGLISH), nullptr);

    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = true;
    aPos.bHasDXArray = false;
    aPos.nEndGlyphCoord = 0;

    // Case 1: Use Layout Width (Mocked at 50)
    aLayout.mnWidth = 50.0;

    TextJustifier::ApplyHorizontalOffset(aLayout, aReq, aPos);

    // Expected: 1 - 50 = -49
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-49.0, aLayout.DrawOffset().getX(), 0.001);

    // Case 2: Disabled RightAlign
    aPos.bRightAlign = false;
    aLayout.DrawOffset().setX(0); // Reset

    TextJustifier::ApplyHorizontalOffset(aLayout, aReq, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aLayout.DrawOffset().getX(), 0.001);
}

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

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testApplyHorizontalOffsetFromZero)
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"RTL"_ustr, 0, 3, SalLayoutFlags::RightAlign,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos{};
    aPos.bRightAlign = true;
    aPos.nEndGlyphCoord = 0;
    aArgs.mnLayoutWidth = 0;
    vcl::text::TextJustifier::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-99.0, aLayout.DrawOffset().getX(), 0.001);
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testApplyHorizontalOffset_EndGlyph)
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"RTL"_ustr, 0, 3, SalLayoutFlags::RightAlign,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = true;
    aPos.nEndGlyphCoord = 150.0;
    aPos.bHasDXArray = true;
    aArgs.mnLayoutWidth = 200;
    vcl::text::TextJustifier::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-149.0, aLayout.DrawOffset().getX(), 0.001);
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testApplyHorizontalOffset_Disabled)
{
    MockSalLayout aLayout;
    vcl::text::TextLayoutRequest aArgs(u"LTR"_ustr, 0, 3, SalLayoutFlags::NONE,
                                       LanguageTag(LANGUAGE_ENGLISH_US), nullptr);
    vcl::text::TextLayoutPositioning aPos;
    aPos.bRightAlign = false;
    vcl::text::TextJustifier::ApplyHorizontalOffset(aLayout, aArgs, aPos);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aLayout.DrawOffset().getX(), 0.001);
}

CPPUNIT_TEST_FIXTURE(TextJustifierTest, testSetAnchorPoint)
{
    MockKashidaLayout aLayout;

    vcl::text::TextLayoutPositioning aPos;
    aPos.aDrawBase = basegfx::B2DPoint(100.5, 200.5);

    // Act
    TextJustifier::SetAnchorPoint(aLayout, aPos);

    // Assert
    basegfx::B2DPoint aResult = aLayout.DrawBase();
    CPPUNIT_ASSERT_DOUBLES_EQUAL(100.5, aResult.getX(), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(200.5, aResult.getY(), 0.001);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
