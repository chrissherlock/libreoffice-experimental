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
#include <vcl/font.hxx>
#include <tools/fontenum.hxx>
#include <tools/gen.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <font/LogicalFontInstance.hxx> // Required for InitializeFontMetrics

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

    virtual void DrawText(SalGraphics&) const override {}
    virtual bool LayoutText(vcl::text::ImplLayoutArgs&, const SalLayoutGlyphsImpl*) override
    {
        return true;
    }
    virtual void AdjustLayout(vcl::text::ImplLayoutArgs&) override {}
    virtual sal_Int32 GetTextBreak(double, double, int) const override { return 0; }
    virtual double FillDXArray(std::vector<double>*, const OUString&) const override { return 0; }
    virtual double FillPartialDXArray(std::vector<double>*, const OUString&, sal_Int32,
                                      int) const override
    {
        return 0;
    }
    virtual void GetCaretPositions(std::vector<double>&, const OUString&) const override {}
    virtual bool HasFontKashidaPositions() const override { return true; }
    virtual bool IsKashidaPosValid(int, int) const override { return true; }

    void addGlyph(int nX, int nWidth)
    {
        GlyphItem aItem(0, 0, 0, basegfx::B2DPoint(nX, 0), GlyphItemFlags::NONE, nWidth, 0, 0, 0);
        m_aGlyphs.push_back(aItem);
    }

    virtual bool GetNextGlyph(const GlyphItem** ppGlyph, basegfx::B2DPoint& rPos, int& nStart,
                              const LogicalFontInstance** = nullptr) const override
    {
        if (nStart < 0 || static_cast<size_t>(nStart) >= m_aGlyphs.size())
            return false;

        *ppGlyph = &m_aGlyphs[nStart];
        rPos = (*ppGlyph)->linearPos();
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
        aMockLayout.addGlyph(10, 20);
        aMockLayout.addGlyph(40, 20);
        aMockLayout.addGlyph(70, 30);

        long nAscent = 20;
        long nDescent = 5;

        auto aPointsAbove = vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(
            aMockLayout, nAscent, nDescent, FontEmphasisMark::Circle);

        CPPUNIT_ASSERT_EQUAL(size_t(3), aPointsAbove.size());
        CPPUNIT_ASSERT_EQUAL(static_cast<long>(20), aPointsAbove[0].X());
        CPPUNIT_ASSERT(aPointsAbove[0].Y() < 0);

        auto aPointsBelow = vcl::text::TextLayoutEngine::GetEmphasisMarkPositions(
            aMockLayout, nAscent, nDescent, FontEmphasisMark::Dot | FontEmphasisMark::PosBelow);

        CPPUNIT_ASSERT_EQUAL(size_t(3), aPointsBelow.size());
        CPPUNIT_ASSERT(aPointsBelow[0].Y() > 0);
    }

    void testInitializeFontMetrics()
    {
        // 1. Setup a dummy LogicalFontInstance via VirtualDevice
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        // Use a generic font (English) to start
        vcl::Font aFont(u"DejaVu Sans"_ustr, Size(0, 12));
        aFont.SetLanguage(LANGUAGE_ENGLISH_US);
        pVDev->SetFont(aFont);

        // We need a mutable pointer to test the API.
        // In real code, OutputDevice owns it and passes it mutable.
        LogicalFontInstance* pFontInstance
            = const_cast<LogicalFontInstance*>(pVDev->GetFontInstance());

        // 2. Define Mocks
        int nCallCountWidth = 0;
        int nCallCountRect = 0;

        auto fnWidth = [&](const OUString& rStr) -> long {
            nCallCountWidth++;
            if (rStr == " ")
                return 100; // Space Width
            if (rStr == u"\x00b7")
                return 40; // Bullet Width
            return 0;
        };

        auto fnRect = [&](tools::Rectangle&, const OUString&) { nCallCountRect++; };

        // 3. Test: English Font (Should NOT trigger CJK rect calculation)
        vcl::text::TextLayoutEngine::InitializeFontMetrics(pFontInstance, aFont, 96, 1, fnWidth,
                                                           fnRect);

        // Check Bullet Offset Logic
        // We can't easily check private members of pFontInstance,
        // but we CAN verify the callbacks were used correctly to derive it.
        // Expected calls: " " (space) and bullet char
        CPPUNIT_ASSERT(nCallCountWidth >= 2);

        // Non-CJK font should NOT calculate bound rect for fullwidth fullstop
        CPPUNIT_ASSERT_EQUAL(0, nCallCountRect);

        // 4. Test: CJK Font (Should trigger CJK logic)
        aFont.SetLanguage(LANGUAGE_JAPANESE);
        // Update the instance/device to reflect language change?
        // Logic uses the PASSED font, so we just pass the modified aFont structure.

        vcl::text::TextLayoutEngine::InitializeFontMetrics(pFontInstance, aFont, 96, 1, fnWidth,
                                                           fnRect);

        // Should now have called GetBoundRect for the fullstop check
        CPPUNIT_ASSERT_EQUAL(1, nCallCountRect);
    }

    CPPUNIT_TEST_SUITE(TextLayoutEngineTest);
    CPPUNIT_TEST(testKashidaPositionLogic);
    CPPUNIT_TEST(testEmphasisMarkPositions);
    CPPUNIT_TEST(testInitializeFontMetrics);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(TextLayoutEngineTest);

} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
