
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Evaluation of Font Refactoring Stability
 * * This test suite isolates the specific failure modes discovered previously:
 * 1. Missing Fonts (Headless fallback)
 * 2. Zero-Height Fonts (MapMode/DPI issues)
 * 3. Uninitialized State (InitFont order of operations)
 */

#include <test/bootstrapfixture.hxx>
#include <test/outputdevice.hxx>

#include <tools/mapunit.hxx>

#include <vcl/outdev.hxx>
#include <vcl/print.hxx>
#include <vcl/virdev.hxx>
#include <vcl/font.hxx>
#include <vcl/metric.hxx>

class FontRefactorTest : public test::BootstrapFixture
{
public:
    FontRefactorTest()
        : BootstrapFixture(true, false)
    {
    }

    // Test 1: The "Headless Fallback" Scenario
    // Headless environments often lack requested fonts. VCL must fallback to *something*
    // rather than failing and returning a 0 height.
    void testFallbackConsistency()
    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;

        // Request a font that definitely doesn't exist
        vcl::Font aFont(u"ThisFontDoesNotExist"_ustr, Size(0, 12));
        pVDev->SetFont(aFont);

        // Logic: Even if the font is missing, VCL should select a fallback.
        // The text height must be > 0. If it is 0, ImplNewFont failed to select a fallback.
        tools::Long nHeight = pVDev->GetTextHeight();

        SAL_INFO("vcl.test", "Fallback Height: " << nHeight);
        CPPUNIT_ASSERT_MESSAGE("Fallback font must render with positive height", nHeight > 0);
    }

    // Test 2: The "Initialization Cycle" Scenario
    // We discovered InitFont() failing because it read uninitialized data.
    // This test ensures GetFontMetric() works immediately after SetFont().
    void testImmediateMetricAccess()
    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        vcl::Font aFont(u"Liberation Sans"_ustr, Size(0, 12));
        pVDev->SetFont(aFont);

        // This call triggers ImplNewFont -> InitFont -> ImplInitFontMetrics
        FontMetric aMetric = pVDev->GetFontMetric();

        SAL_INFO("vcl.test", "Metric Ascent: " << aMetric.GetAscent());
        CPPUNIT_ASSERT_MESSAGE("Metric Ascent should be populated", aMetric.GetAscent() > 0);
    }

    // Test 3: The "Zero Pixel" Clamp
    // Small logical fonts in MapMode can round down to 0 pixels.
    // We must ensure they clamp to 1.
    void testSmallFontClamping()
    {
        ScopedVclPtrInstance<VirtualDevice> pVDev;
        pVDev->SetMapMode(MapMode(MapUnit::MapTwip));

        // 1 Twip is extremely small. Likely < 1 pixel.
        vcl::Font aFont(u"Liberation Sans"_ustr, Size(0, 20)); // 1 pt approx
        pVDev->SetFont(aFont);

        tools::Long nHeight = pVDev->GetTextHeight();
        SAL_INFO("vcl.test", "Small Font Height (Twips): " << nHeight);

        // It must not be 0.
        CPPUNIT_ASSERT_MESSAGE("Small fonts must clamp to at least 1 pixel (converted to logical)",
                               nHeight > 0);
    }

    CPPUNIT_TEST_SUITE(FontRefactorTest);
    CPPUNIT_TEST(testFallbackConsistency);
    CPPUNIT_TEST(testImmediateMetricAccess);
    CPPUNIT_TEST(testSmallFontClamping);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(FontRefactorTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
