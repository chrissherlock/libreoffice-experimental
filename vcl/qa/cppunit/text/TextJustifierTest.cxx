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

#include <text/TextJustifier.hxx>
#include <sallayout.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <text/TextLayoutRequest.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <font/FontController.hxx>

#include <sallayout.hxx>

using namespace vcl::text;

namespace
{
class MockKashidaLayout : public SalLayout
{
public:
    virtual void DrawText(SalGraphics&) const override {}
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void AdjustLayout(vcl::text::TextLayoutRequest&) override {}

    // Test Hooks
    virtual bool HasFontKashidaPositions() const override { return true; }
    virtual bool IsKashidaPosValid(int nIdx, int /*nNext*/) const override
    {
        // Mock logic: Valid if index is even
        return (nIdx % 2) == 0;
    }

    // Required stubs

    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual double GetTextWidth() const override { return 0; }
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

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
