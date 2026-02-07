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
#include <vcl/text/DefaultFallbackStrategy.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <vcl/font.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>

#include <GraphicsState.hxx>
#include <CoordinateMapper.hxx>
#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontFace.hxx>
#include <sallayout.hxx>
#include <text/TextLayoutRequest.hxx>

using namespace vcl::text;

class MockInputLayout : public SalLayout
{
public:
    MockInputLayout()
        : SalLayout()
    {
    }
    virtual ~MockInputLayout() override = default;

    // Pure virtual stubs required to instantiate the class
    virtual bool LayoutText(vcl::text::TextLayoutRequest&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void DrawText(SalGraphics&) const override {}
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, sal_Int32,
                                      sal_Int32) const override
    {
        return 0;
    }
    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual bool HasFontKashidaPositions() const override { return false; }
    virtual bool IsKashidaPosValid(int, int) const override { return false; }
    virtual bool GetNextGlyph(const GlyphItem**, basegfx::B2DPoint&, int&,
                              const LogicalFontInstance**) const override
    {
        return false;
    }
};

// Stub for LogicalFontInstance to satisfy LayoutResources
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
    virtual sal_IntPtr GetFontId() const override { return 0; }
    virtual hb_blob_t* GetHbTable(hb_tag_t) const override { return nullptr; }
};

class StubFontInstance : public LogicalFontInstance
{
public:
    StubFontInstance()
        : LogicalFontInstance(*new StubPhysicalFontFace(),
                              vcl::font::FontSelectPattern(vcl::Font(), "", Size(0, 12), 12.0))
    {
    }

    virtual bool GetGlyphOutline(sal_GlyphId, basegfx::B2DPolyPolygon&, bool) const override
    {
        return false;
    }
};

// ============================================================================
// Test Suite
// ============================================================================

class DefaultFallbackStrategyTest : public test::BootstrapFixture
{
public:
    DefaultFallbackStrategyTest()
        : BootstrapFixture(true, false)
    {
    }

    void testResolveFallbacks_NoAction();

    CPPUNIT_TEST_SUITE(DefaultFallbackStrategyTest);
    CPPUNIT_TEST(testResolveFallbacks_NoAction);
    CPPUNIT_TEST_SUITE_END();
};

void DefaultFallbackStrategyTest::testResolveFallbacks_NoAction()
{
    // 1. Setup minimal dependencies
    ScopedVclPtrInstance<VirtualDevice> pVDev; // Required for VCL subsystem init
    (void)pVDev;

    vcl::GraphicsState aState;
    vcl::font::FontRealization aRealization;
    CoordinateMapper aMapper;

    StubFontInstance* pStubFont = new StubFontInstance();
    rtl::Reference<LogicalFontInstance> xFont(pStubFont);

    // We pass nullptr for things we expect NOT to be used in this test case
    LayoutResources aRes{ xFont.get(), aMapper,
                          nullptr, // pFontCache
                          nullptr, // pFontCollection
                          nullptr, // pForcedFallback
                          nullptr, // fnGetGraphics - Should not be called!
                          false, // bRTLEnabled
                          false, // bSubpixelPositioning
                          aState,      aRealization };

    // 2. Setup Request - Mark it as "Complete" (No fallback needed)
    // By default, a TextLayoutRequest has an empty runs list.
    // We add a run that covers the whole string to indicate success.
    OUString aText = "OK";
    TextLayoutRequest aReq(aText, 0, 2, SalLayoutFlags::NONE, LanguageTag("en"), nullptr);
    aReq.maRuns.AddRun(0, 2, false); // "Resolved" run

    // 3. Create Input Layout
    std::unique_ptr<SalLayout> pBase = std::make_unique<MockInputLayout>();
    SalLayout* pOriginalPtr = pBase.get();

    // 4. Execute Strategy
    DefaultFallbackStrategy aStrategy;
    std::unique_ptr<SalLayout> pResult
        = aStrategy.ResolveFallbacks(aRes, std::move(pBase), aReq, nullptr);

    // 5. Verify
    // Since HasFallbackRun() should be false (we added a valid run),
    // the strategy should return the original layout immediately.
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should return original pointer", pOriginalPtr, pResult.get());
}

CPPUNIT_TEST_SUITE_REGISTRATION(DefaultFallbackStrategyTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
