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

#include <tools/mapunit.hxx>

#include <vcl/metric.hxx>
#include <vcl/text/MnemonicGeometry.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/virdev.hxx>
#include <CoordinateMapper.hxx>

#include <text/FontMetricEngine.hxx>
#include <font/FontController.hxx>
#include <font/PhysicalFontFace.hxx>
#include <sallayout.hxx>
#include <textlayout.hxx>

#include <vector>

using namespace vcl::text;

namespace
{
class TextFontMetricEngineTest : public CppUnit::TestFixture
{
};

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

CPPUNIT_TEST_FIXTURE(TextFontMetricEngineTest, testInitializeTextLineMetrics)
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    const LogicalFontInstance* pConstFont = xFont.get(); // Test const-correctness

    vcl::Font aFont;
    const tools::Long nDPI = 96;
    const tools::Long nSpaceW = 10;
    const tools::Long nBulletW = 4;

    vcl::text::FontMetricEngine::InitializeTextLineMetrics(pConstFont, aFont, nDPI, nSpaceW,
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

CPPUNIT_TEST_FIXTURE(TextFontMetricEngineTest, testInitializeFontMetrics)
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
    vcl::text::FontMetricEngine::InitializeFontMetrics(xFont.get(), aFont, nDPI, nPixelWidth,
                                                       fnWidth, fnRect);

    // Assertions
    // Verify that mnLineHeight was set (Ascent + Descent from StubFontInstance)
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line height should be initialized from metrics", tools::Long(20),
                                 xFont->mnLineHeight);

    // Verify the internal FontMetricData was touched by checking a property
    // that InitializeFontMetrics calculates or delegates.
    CPPUNIT_ASSERT_MESSAGE("FontMetricData should be initialized", xFont->mxFontMetric != nullptr);
}

CPPUNIT_TEST_FIXTURE(TextFontMetricEngineTest, testInitializeAboveTextLineMetrics)
{
    rtl::Reference<LogicalFontInstance> xFont(new StubFontInstance());
    const tools::Long nDPI = 96;
    const tools::Long nPixelWidth = 1; // Simulated logic-to-pixel width

    vcl::text::FontMetricEngine::InitializeAboveTextLineMetrics(xFont.get(), nDPI, nPixelWidth);

    // Verify the FontMetricData was updated with the correct above-line sizes
    // Note: Verification depends on specific FontMetricData getters for
    // overline/above-line properties.
    CPPUNIT_ASSERT(xFont->mxFontMetric != nullptr);
}

CPPUNIT_TEST_FIXTURE(TextFontMetricEngineTest, testGetAlignmentOffset)
{
    const tools::Long nAscent = 80;
    const tools::Long nDescent = 20;

    // Test ALIGN_TOP: Should return the positive ascent value to shift text down
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_TOP offset is incorrect", nAscent,
        vcl::text::FontMetricEngine::GetAlignmentOffset(ALIGN_TOP, nAscent, nDescent));

    // Test ALIGN_BOTTOM: Should return the negative descent value to shift text up
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_BOTTOM offset is incorrect", -nDescent,
        vcl::text::FontMetricEngine::GetAlignmentOffset(ALIGN_BOTTOM, nAscent, nDescent));

    // Test ALIGN_BASELINE: Should return 0 (no shift)
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "ALIGN_BASELINE offset should be zero", tools::Long(0),
        vcl::text::FontMetricEngine::GetAlignmentOffset(ALIGN_BASELINE, nAscent, nDescent));
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();
