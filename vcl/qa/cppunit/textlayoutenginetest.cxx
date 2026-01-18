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

#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>
#include <vcl/virdev.hxx>
#include <tools/fontenum.hxx>
#include <tools/gen.hxx>
#include <basegfx/point/b2dpoint.hxx>

// Forward declaration for the override
class LogicalFontInstance;

namespace
{
// Minimal Mock for SalLayout to test coordinate calculations
class MockSalLayout : public SalLayout
{
private:
    std::vector<GlyphItem> m_aGlyphs;

public:
    MockSalLayout() = default;
    virtual ~MockSalLayout() override = default;

    // -------------------------------------------------------------------
    // Implement Pure Virtuals from SalLayout
    // -------------------------------------------------------------------

    virtual void DrawText(SalGraphics&) const override {}

    virtual bool LayoutText(vcl::text::ImplLayoutArgs&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }

    virtual void AdjustLayout(vcl::text::ImplLayoutArgs&) override {}

    // Stubs for metrics
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }

    // FIX 1: Correct signature (3rd arg is sal_Int32, not vector)
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, sal_Int32,
                                      int) const override
    {
        return 0;
    }

    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}

    // Kashida stubs
    virtual bool HasFontKashidaPositions() const override { return true; }
    virtual bool IsKashidaPosValid(int, int) const override { return true; }

    // -------------------------------------------------------------------
    // Helper to add test glyphs
    // -------------------------------------------------------------------
    void addGlyph(int nX, int nWidth)
    {
        // Construct with standard parameters
        GlyphItem aItem(0, 0, 0, basegfx::B2DPoint(nX, 0), GlyphItemFlags::NONE, nWidth, 0, 0, 0);
        m_aGlyphs.push_back(aItem);
    }

    // -------------------------------------------------------------------
    // Override GetNextGlyph to return our mock data
    // FIX 2: Correct signature (4th arg is const LogicalFontInstance**)
    // -------------------------------------------------------------------
    virtual bool
    GetNextGlyph(const GlyphItem** ppGlyph, basegfx::B2DPoint& rPos, int& nStart,
                 const LogicalFontInstance** /*pFallbackFont*/ = nullptr) const override
    {
        if (nStart < 0 || static_cast<size_t>(nStart) >= m_aGlyphs.size())
            return false;

        *ppGlyph = &m_aGlyphs[nStart];
        rPos = (*ppGlyph)->linearPos(); // Access via helper method
        nStart++;
        return true;
    }
};

class TextLayoutEngineTest : public CppUnit::TestFixture
{
public:
    void testKashidaPositionLogic()
    {
        OUString aText = u"\u0628\u064E\u0627"_ustr; // Be + Fatha + Aleph

        ScopedVclPtrInstance<VirtualDevice> pVDev;
        vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 12));
        pVDev->SetFont(aFont);

        std::vector<bool> aKashidaMap;
        pVDev->GetWordKashidaPositions(aText, &aKashidaMap);

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(aText.getLength()), aKashidaMap.size());
    }

    void testEmphasisMarkPositions()
    {
        MockSalLayout aMockLayout;

        // Add 3 Glyphs:
        // 1. Pos 10, Width 20 -> Center X = 20
        // 2. Pos 40, Width 20 -> Center X = 50
        // 3. Pos 70, Width 30 -> Center X = 85
        aMockLayout.addGlyph(10, 20);
        aMockLayout.addGlyph(40, 20);
        aMockLayout.addGlyph(70, 30);

        long nAscent = 20;
        long nDescent = 5;

        // Test 1: Mark Above (Circle)
        auto aPointsAbove = vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(
            aMockLayout, nAscent, nDescent, FontEmphasisMark::Circle);

        CPPUNIT_ASSERT_EQUAL(size_t(3), aPointsAbove.size());

        CPPUNIT_ASSERT_EQUAL(static_cast<long>(20), aPointsAbove[0].X());
        CPPUNIT_ASSERT_EQUAL(static_cast<long>(50), aPointsAbove[1].X());
        CPPUNIT_ASSERT_EQUAL(static_cast<long>(85), aPointsAbove[2].X());
        CPPUNIT_ASSERT(aPointsAbove[0].Y() < 0);

        // Test 2: Mark Below (Dot)
        auto aPointsBelow = vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(
            aMockLayout, nAscent, nDescent, FontEmphasisMark::Dot | FontEmphasisMark::PosBelow);

        CPPUNIT_ASSERT_EQUAL(size_t(3), aPointsBelow.size());
        CPPUNIT_ASSERT(aPointsBelow[0].Y() > 0);
    }

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testKashidaPositionLogic);
    CPPUNIT_TEST(testEmphasisMarkPositions);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
