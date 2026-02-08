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

#include <test/bootstrapfixture.hxx>
#include <test/outputdevice.hxx>

#include <vcl/text/CaretManager.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <vcl/virdev.hxx>
#include <vcl/font.hxx>

#include <sallayout.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <font/FontController.hxx>

class MockSalLayout : public SalLayout
{
    double mnWidth;
    std::vector<double> maCaretPos;

public:
    MockSalLayout(double nWidth, std::vector<double> aCaretPos)
        : mnWidth(nWidth)
        , maCaretPos(std::move(aCaretPos))
    {
    }

    virtual void GetCaretPositions(std::vector<double>& rCaretPositions,
                                   const OUString&) const override
    {
        rCaretPositions = maCaretPos;
    }

    virtual double GetTextWidth() const override { return mnWidth; }

    virtual void DrawText(SalGraphics&) const override {}
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override
    {
        return mnWidth;
    }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, sal_Int32,
                                      sal_Int32) const override
    {
        return 0;
    }
    virtual bool HasFontKashidaPositions() const override { return false; }
    virtual bool IsKashidaPosValid(int, int) const override { return false; }
    virtual bool GetNextGlyph(const GlyphItem**, basegfx::B2DPoint&, int&,
                              const LogicalFontInstance**) const override
    {
        return false;
    }
};

// --- Test Suite ---

class CaretManagerTest : public test::BootstrapFixture
{
public:
    CaretManagerTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(CaretManagerTest, testGetCaretPositionsRTL)
{
    // 1. Setup the Mock Layout
    // Scenario: Two characters. Char 1 at X=10, Char 2 at X=30.
    // Total text width is 100.
    std::vector<double> aRawPos = { 10.0, 30.0 };
    double nTotalWidth = 100.0;
    MockSalLayout aMockLayout(nTotalWidth, aRawPos);

    // 2. Setup Dependencies
    CoordinateMapper aMapper;
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;

    // 3. Construct LayoutResources with bRTLEnabled = true
    vcl::text::LayoutResources aRes{ nullptr, aMapper, nullptr,     nullptr, nullptr, nullptr,
                                     true, // bRTLEnabled = true
                                     false,   aState,  aRealization };

    // 4. Call the Component
    vcl::text::TextSpan aSpan("Test", 0, 2);
    std::vector<double> aResultPos;

    vcl::text::CaretManager::GetCaretPositions(aRes, aSpan, aResultPos, aMockLayout);

    // 5. Verify Results
    // Mirror Logic: NewX = Width - OldX - 1.0
    // Expected[0]: 100 - 10 - 1 = 89
    // Expected[1]: 100 - 30 - 1 = 69

    CPPUNIT_ASSERT_EQUAL(size_t(2), aResultPos.size());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(89.0, aResultPos[0], 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(69.0, aResultPos[1], 0.001);
}

CPPUNIT_TEST_FIXTURE(CaretManagerTest, testGetCaretPositionsLTR)
{
    // 1. Setup Mock Layout
    std::vector<double> aRawPos = { 10.0, 30.0 };
    MockSalLayout aMockLayout(100.0, aRawPos);

    // 2. Setup Dependencies
    CoordinateMapper aMapper;
    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;

    // 3. Construct LayoutResources with bRTLEnabled = false
    vcl::text::LayoutResources aRes{ nullptr, aMapper, nullptr,     nullptr, nullptr, nullptr,
                                     false, // bRTLEnabled = false
                                     false,   aState,  aRealization };

    // 4. Call the Component
    vcl::text::TextSpan aSpan("Test", 0, 2);
    std::vector<double> aResultPos;

    vcl::text::CaretManager::GetCaretPositions(aRes, aSpan, aResultPos, aMockLayout);

    // 5. Verify Results
    // Expect no change (other than potential int->double fidelity)
    CPPUNIT_ASSERT_EQUAL(size_t(2), aResultPos.size());
    CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, aResultPos[0], 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(30.0, aResultPos[1], 0.001);
}

CPPUNIT_PLUGIN_IMPLEMENT();
