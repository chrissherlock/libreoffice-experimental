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

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
