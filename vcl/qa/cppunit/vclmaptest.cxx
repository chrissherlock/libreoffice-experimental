/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <tools/fract.hxx>
#include <tools/mapunit.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/print.hxx>

namespace
{
class TestVirtualDevice : public VirtualDevice
{
public:
    // Expose protected methods using 'using'
    using OutputDevice::ImplLogicHeightToDevicePixel;
    using OutputDevice::LogicWidthToDevicePixel;
    using OutputDevice::SetOutOffXPixel;
    using OutputDevice::SetOutOffYPixel;

    // Explicit wrapper if 'using' doesn't satisfy specific compiler strictness
    // (Optional, but 'using' is usually sufficient for access)
};

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testIdentity)
{
    // Test that converting to the same MapMode changes nothing
    MapMode aMode(MapUnit::MapMM);
    Point aPt(1234, 5678);

    Point aResult = OutputDevice::LogicToLogic(aPt, aMode, aMode);

    CPPUNIT_ASSERT_EQUAL(tools::Long(1234), aResult.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(5678), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testSimpleUnitConversion)
{
    // Test MM to CM (10mm = 1cm)
    MapMode aSource(MapUnit::MapMM);
    MapMode aDest(MapUnit::MapCM);
    Point aPt(100, 200); // 100mm, 200mm

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(10), aResult.X()); // 10cm
    CPPUNIT_ASSERT_EQUAL(tools::Long(20), aResult.Y()); // 20cm
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testFractionScaling)
{
    // Converting 100 Source -> 50 Dest

    MapMode aSource(MapUnit::MapMM);
    aSource.SetScaleX(Fraction(1, 2));
    aSource.SetScaleY(Fraction(1, 2));

    MapMode aDest(MapUnit::MapMM); // Default 1:1

    Point aPt(100, 200);
    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(50), aResult.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(100), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testIntermediateOverflow)
{
// Fails on Windows 32-bit. See include/tools/long.hxx
#if defined _WIN64 || !defined _WIN32
    constexpr tools::Long nLarge = static_cast<tools::Long>(1LL << 30);
    constexpr tools::Long nExpected = static_cast<tools::Long>(25400000000);

    MapMode aSource(MapUnit::MapInch);
    aSource.SetScaleX(Fraction(nLarge, 1));

    MapMode aDest(MapUnit::Map100thMM);
    aDest.SetScaleX(Fraction(nLarge, 1));

    // n1 (Coord) = 10,000,000
    // n2 (Scale) = 2^30 (~10^9)
    // n3 (Unit)  = 2540
    // Product = 10^7 * 10^9 * 2540 ~= 2.5 * 10^19
    // Max Int64 ~= 9 * 10^18
    // THIS WILL OVERFLOW 64-bit integer logic if BigInt is missing.
    Point aPt(10000000, 0);

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    // Expected: 10,000,000 inches = 25,400,000,000 100thMM
    CPPUNIT_ASSERT_EQUAL(nExpected, aResult.X());
#endif
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRoundingBehavior)
{
    // Test that 0.5 rounds correctly (usually to nearest)
    // Source: 100thMM
    // Dest:   MM
    // 150 100thMM = 1.5 MM -> Should round to 2
    // 140 100thMM = 1.4 MM -> Should round to 1

    MapMode aSource(MapUnit::Map100thMM);
    MapMode aDest(MapUnit::MapMM);

    Point aPt1(150, 140);
    Point aResult1 = OutputDevice::LogicToLogic(aPt1, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(2), aResult1.X()); // 1.5 -> 2
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult1.Y()); // 1.4 -> 1

    // Test Negative Rounding
    Point aPt2(-150, -140);
    Point aResult2 = OutputDevice::LogicToLogic(aPt2, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(-2), aResult2.X()); // -1.5 -> -2
    CPPUNIT_ASSERT_EQUAL(tools::Long(-1), aResult2.Y()); // -1.4 -> -1
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testOverflowProtectionPositive)
{
// Fails on Windows 32-bit. See include/tools/long.hxx
#if defined _WIN64 || !defined _WIN32
    // Scenario: Convert Inch to MM (IsSimple() == true)
    // Factor: 127 / 5 (1 inch = 25.4 mm = 254/10 = 127/5)
    //
    // We need an input N such that:
    // 1. N * 127 > INT64_MAX (9.22e18)  --> Triggers overflow flag in o3tl::convert
    // 2. (N * 127) / 5 < INT64_MAX      --> Final result fits in tools::Long

    constexpr tools::Long nInput = static_cast<tools::Long>(100000000000000000LL); // 1e17
    constexpr tools::Long nExpected = static_cast<tools::Long>(2540000000000000000LL); // 2.54e18

    MapMode aSource(MapUnit::MapInch);
    MapMode aDest(MapUnit::MapMM);

    // Sanity check: Ensure our MapModes are indeed "Simple" so we hit fn3
    CPPUNIT_ASSERT(aSource.IsSimple());
    CPPUNIT_ASSERT(aDest.IsSimple());

    Point aPt(nInput, 0);
    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(nExpected, aResult.X());
#endif
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testOverflowProtectionNegative)
{
// Fails on Windows 32-bit. See include/tools/long.hxx
#if defined _WIN64 || !defined _WIN32
    // Same as above, but testing negative handling in BigInt path
    constexpr tools::Long nInput = static_cast<tools::Long>(-100000000000000000LL); // -1e17
    constexpr tools::Long nExpected = static_cast<tools::Long>(-2540000000000000000LL);

    MapMode aSource(MapUnit::MapInch);
    MapMode aDest(MapUnit::MapMM);

    Point aPt(nInput, 0);
    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(nExpected, aResult.X());
#endif
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRounding)
{
    // Conversion: MapMM to MapInch
    // Factor: 5 / 127

    MapMode aSource(MapUnit::MapMM);
    MapMode aDest(MapUnit::MapInch);

    // 13 * 5 / 127 = 65 / 127 ~= 0.5118 -> Rounds to 1
    Point aPt(13, 12);
    // 12 * 5 / 127 = 60 / 127 ~= 0.47 -> Rounds to 0

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(0), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testTwipsTo100thMM)
{
    // 1 Twip = 1/1440 inch
    // 1 inch = 2540 100thMM
    // Factor = 2540 / 1440 = 127 / 72 ~= 1.7638

    MapMode aSource(MapUnit::MapTwip);
    MapMode aDest(MapUnit::Map100thMM);

    Point aPt(7200, 1440); // 5 inches, 1 inch

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    // 5 inches = 12700 100thMM
    CPPUNIT_ASSERT_EQUAL(tools::Long(12700), aResult.X());
    // 1 inch = 2540 100thMM
    CPPUNIT_ASSERT_EQUAL(tools::Long(2540), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testPointsToInch)
{
    // 1 Point = 1/72 inch
    MapMode aSource(MapUnit::MapPoint);
    MapMode aDest(MapUnit::MapInch);

    Point aPt(72, 144); // 1 inch, 2 inches

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(2), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testZoomScaling)
{
    // Simulating a View Zoom
    // Source: 100thMM at 400% zoom (Scale 4:1)
    // Dest:   100thMM at 100% zoom (Scale 1:1)

    MapMode aSource(MapUnit::Map100thMM);
    aSource.SetScaleX(Fraction(4, 1));
    aSource.SetScaleY(Fraction(4, 1));

    MapMode aDest(MapUnit::Map100thMM);

    Point aPt(100, 100);
    // If I have 100 units at 400% zoom, that is physically 400 units.
    // Converting to 100% zoom, it should remain 400 units.

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(400), aResult.X());
    CPPUNIT_ASSERT_EQUAL(tools::Long(400), aResult.Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testComplexFractions)
{
    // Use prime number fractions to ensure rational arithmetic
    // is maintained without premature truncation.

    // Source Scale: 1/3
    // Dest Scale:   1/7
    // Conversion:   Val * (1/3) / (1/7) = Val * 7/3 ~= Val * 2.333

    MapMode aSource(MapUnit::MapMM);
    aSource.SetScaleX(Fraction(1, 3));

    MapMode aDest(MapUnit::MapMM);
    aDest.SetScaleX(Fraction(1, 7));

    Point aPt(300, 0);
    // 300 * 7 / 3 = 100 * 7 = 700

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    CPPUNIT_ASSERT_EQUAL(tools::Long(700), aResult.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCombinedScaleAndOrigin)
{
    // Scenario: Source is MM with 50% scale. Dest is 100thMM.
    MapMode aSource(MapUnit::MapMM);
    aSource.SetScaleX(Fraction(1, 2)); // 0.5 Scale
    aSource.SetScaleY(Fraction(1, 2));
    aSource.SetOrigin(Point(10, 0));

    MapMode aDest(MapUnit::Map100thMM);

    Point aPt(30, 0);

    // Standard GDI logic would imply: (Point - Origin) * Scale
    // i.e., (30 - 10) * 0.5 = 10mm -> 1000 100thMM.
    //
    // However, VCL historically treats SetOrigin as a Translation Offset (Additive).
    // Actual Calculation: (Point + Origin) * Scale
    // i.e., (30 + 10) * 0.5 = 20mm -> 2000 100thMM.

    Point aResult = OutputDevice::LogicToLogic(aPt, aSource, aDest);

    // We assert 2000 to enforce consistency with LogicToPixel and historical rendering.
    CPPUNIT_ASSERT_EQUAL(tools::Long(2000), aResult.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testBasicLogicToPixel)
{
    // Use VirtualDevice as it is lightweight and headless-compatible
    ScopedVclPtr<TestVirtualDevice> pDev(VclPtr<TestVirtualDevice>::Create());

    // 1. Default MapMode (MapPixel)
    // In MapPixel, Logic == Pixel
    Point aLogicPt(100, 100);
    Point aPixelPt = pDev->LogicToPixel(aLogicPt);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Default MapMode should map 1:1", aLogicPt, aPixelPt);

    // 2. LogicToPixel with specific MapMode override
    // 100th MM to Pixel.
    // Assuming 96 DPI for the test VirtualDevice (standard for headless)
    // 1 inch = 2540 100th MM.
    // 96 pixels = 2540 logic units.
    // 1000 logic units = (1000 * 96) / 2540 ~= 37 pixels.
    MapMode aMap100thMM(MapUnit::Map100thMM);
    Point aConverted = pDev->LogicToPixel(Point(1000, 1000), aMap100thMM);

    // We allow a small margin for integer arithmetic rounding, though 37 is exact-ish
    CPPUNIT_ASSERT(aConverted.X() >= 37 && aConverted.X() <= 38);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMapModePropagation)
{
    ScopedVclPtr<TestVirtualDevice> pDev(VclPtr<TestVirtualDevice>::Create());

    // Set MapMode on OutDev and ensure logic functions use it
    MapMode aMapMode(MapUnit::Map100thMM);
    pDev->SetMapMode(aMapMode);

    // Verify state getter
    CPPUNIT_ASSERT_EQUAL(MapUnit::Map100thMM, pDev->GetMapMode().GetMapUnit());

    // Test calculation through the residual wrapper
    // 1000 100thmm @ 96 DPI ~= 37.7 pixels
    long nHeightVal = 1000;
    long nPixelHeight = pDev->ImplLogicHeightToDevicePixel(nHeightVal);

    CPPUNIT_ASSERT(nPixelHeight >= 37 && nPixelHeight <= 38);

    // Change MapMode again to verify Reset/Update logic in map.cxx
    MapMode aTwips(MapUnit::MapTwip);
    pDev->SetMapMode(aTwips);

    // 1440 Twips = 1 Inch = 96 Pixels (usually)
    long nPixelTwips = pDev->ImplLogicHeightToDevicePixel(1440);

    // Check if the change propagated to the internal mapper
    CPPUNIT_ASSERT_EQUAL_MESSAGE("1440 Twips should be approx 96 pixels", long(96), nPixelTwips);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testSetRelativeMapMode)
{
    ScopedVclPtr<TestVirtualDevice> pDev(VclPtr<TestVirtualDevice>::Create());

    // Initial MapMode: 100th MM
    pDev->SetMapMode(MapMode(MapUnit::Map100thMM));

    // Target MapMode: Scale by 2 (Relative)
    MapMode aScaleMode(MapUnit::Map100thMM);
    aScaleMode.SetScaleX(Fraction(2, 1));
    aScaleMode.SetScaleY(Fraction(2, 1));

    // This calls the residual SetRelativeMapMode in map.cxx
    pDev->SetRelativeMapMode(aScaleMode);

    // 1000 logic units * 2 (scale) -> converted to pixels
    // 1000 units normally ~37 pixels. Scaled by 2 should be ~75 pixels.
    Point aPt(1000, 1000);
    Point aPix = pDev->LogicToPixel(aPt);

    CPPUNIT_ASSERT(aPix.X() > 70 && aPix.X() < 80);

    // Ensure the internal mapper updated its scale factor
    Fraction aResScale = pDev->GetMapMode().GetScaleX();
    CPPUNIT_ASSERT_EQUAL(long(2), long(aResScale));
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testViewTransformation)
{
    ScopedVclPtr<TestVirtualDevice> pDev(VclPtr<TestVirtualDevice>::Create());

    pDev->SetMapMode(MapMode(MapUnit::Map100thMM));
    pDev->SetOutOffXPixel(10);

    // Get transformation matrix via residual wrapper
    basegfx::B2DHomMatrix aMat = pDev->GetViewTransformation();

    // Transform a point manually using the matrix
    basegfx::B2DPoint aPt(1000.0, 1000.0); // 1000 100th mm
    aPt *= aMat;

    // Transform using the convenience function
    Point aLogicPt(1000, 1000);
    Point aPix = pDev->LogicToPixel(aLogicPt);

    // Compare Matrix result vs Helper result
    // Matrix result includes floating point precision, Helper rounds.
    // LogicToPixel = (Logic * Scale) + Offset
    // Offset is 10. 1000 100thMM is approx 37.8 pixels. Total ~47.8.

    CPPUNIT_ASSERT_DOUBLES_EQUAL(double(aPix.X()), aPt.getX(), 1.0);
}
} // end anonymous namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
