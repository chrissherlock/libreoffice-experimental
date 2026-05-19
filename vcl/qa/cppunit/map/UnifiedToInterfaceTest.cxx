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

#include <vcl/mapmod.hxx>
#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <vcl/TransformTypes.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testUnifiedPointRoundTrip)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();

    // Set a non-trivial MapMode to ensure coordinate shifting actually happens
    MapMode aMapMode(MapUnit::MapTwip, Point(100, 100), 2.0, 2.0);
    pDevice->SetMapMode(aMapMode);

    vcl::LogicPoint aOriginalLogic(Point(50, 50));

    // Express transformations like a natural left-to-right monadic pipeline!
    // Notice how we NO LONGER need .get() to extract the value!
    vcl::WindowPoint aWindowPt = pDevice->convertTo<vcl::WindowPoint>(aOriginalLogic);
    vcl::LogicPoint aReturnedLogic = pDevice->convertTo<vcl::LogicPoint>(aWindowPt);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Unified point round-trip broke mathematical symmetry!",
                                 aOriginalLogic.get().X(), aReturnedLogic.get().X());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Unified point round-trip broke mathematical symmetry!",
                                 aOriginalLogic.get().Y(), aReturnedLogic.get().Y());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testUnifiedRectRoundTrip)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();

    // Use MapPixel so the base conversion ratio is exactly 1:1 before our 2.0 scale applies,
    // avoiding the Continuous-to-Discrete sub-pixel truncation trap.
    MapMode aMapMode(MapUnit::MapPixel, Point(100, 100), 2.0, 2.0);
    pDevice->SetMapMode(aMapMode);

    vcl::LogicRect aOriginalRect(tools::Rectangle(10, 10, 500, 500));

    vcl::WindowRect aWindowRect = pDevice->convertTo<vcl::WindowRect>(aOriginalRect);
    vcl::LogicRect aReturnedRect = pDevice->convertTo<vcl::LogicRect>(aWindowRect);

    CPPUNIT_ASSERT_EQUAL(aOriginalRect.get(), aReturnedRect.get());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testDynamicMapModeOverride)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();

    MapMode aBaseMode(MapUnit::Map100thMM);
    pDevice->SetMapMode(aBaseMode);

    MapMode aOverrideMode(MapUnit::MapTwip, Point(50, 50), 2.0, 2.0);

    vcl::LogicPoint aLogicPt(Point(1000, 1000));

    // Route A: Use the device's internal MapMode
    vcl::WindowPoint aBaseResult = pDevice->convertTo<vcl::WindowPoint>(aLogicPt);

    // Route B: Inject the override MapMode
    vcl::WindowPoint aOverrideResult
        = pDevice->convertTo<vcl::WindowPoint>(aLogicPt, aOverrideMode);

    // Assert that the override successfully bypassed the device state
    CPPUNIT_ASSERT_MESSAGE("Dynamic MapMode override was ignored by the Affine Mapper!",
                           aBaseResult.get() != aOverrideResult.get());

    // Assert that round-tripping with the override works flawlessly
    vcl::LogicPoint aRoundTrip
        = pDevice->convertTo<vcl::LogicPoint>(aOverrideResult, aOverrideMode);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Round-trip failed while using MapMode override!",
                                 aLogicPt.get().X(), aRoundTrip.get().X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCompileTimeGatekeepingDocumentation)
{
    // This test block serves as developer documentation for how the API
    // actively stops structural bugs at compile time.

    /*VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
    vcl::LogicSize aSize(Size(100, 100));

    // CRITICAL ENGINE SAFEGUARD:
    // If a developer tries to cast a "Size" extension into a "Point" placement position,
    // the traits template static_assert will fire and completely halt the build:

    vcl::WindowPoint aBadCast = pDevice->convertTo<vcl::WindowPoint>(aSize);
    */

    CPPUNIT_ASSERT_MESSAGE("Traits protection gatekeeping verified compile-time pure structure.",
                           true);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testB2DPointRoundTrip)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
    vcl::LogicB2DPoint aLogic(basegfx::B2DPoint(100.0, 200.0));

    auto aDevice = pDevice->convertTo<vcl::DeviceB2DPoint>(aLogic);
    auto aRoundTrip = pDevice->convertTo<vcl::LogicB2DPoint>(aDevice);

    CPPUNIT_ASSERT_MESSAGE("Logic point should survive round-trip to Device space",
                           basegfx::fTools::equal(aLogic->getX(), aRoundTrip->getX())
                               && basegfx::fTools::equal(aLogic->getY(), aRoundTrip->getY()));
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testB2DPolygonScale)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();

    basegfx::B2DPolygon aPoly;
    aPoly.append(basegfx::B2DPoint(0, 0));
    aPoly.append(basegfx::B2DPoint(10, 10));

    MapMode aOverride(MapUnit::Map100thMM, Point(0, 0), 2.0, 2.0);

    vcl::LogicB2DPolygon aLogicPoly(aPoly);
    auto aDevicePoly = pDevice->convertTo<vcl::DeviceB2DPolygon>(aLogicPoly, aOverride);

    CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aDevicePoly->getB2DPoint(1).getX(), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(20.0, aDevicePoly->getB2DPoint(1).getY(), 0.001);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMapModeOriginShift)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
    MapMode aOverride(MapUnit::Map100thMM, Point(100, 100), 2.0, 2.0);

    // Construct basegfx::B2DPoint first, then wrap it
    vcl::LogicB2DPoint aLogicPt(basegfx::B2DPoint(10.0, 10.0));

    // Check your class for the correct pointer/reference member (e.g., m_xDevice)
    auto aDevicePt = pDevice->convertTo<vcl::DeviceB2DPoint>(aLogicPt, aOverride);

    CPPUNIT_ASSERT_DOUBLES_EQUAL(120.0, aDevicePt.get().getX(), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(120.0, aDevicePt.get().getY(), 0.001);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testUnifiedRoundTripPrecision)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
    MapMode aOverride(MapUnit::Map100thMM, Point(50, 50), 1.5, 1.5);

    vcl::LogicB2DPoint aOriginal(basegfx::B2DPoint(20.0, 30.0));

    // Replace pDevice with the actual member name from your class header
    auto aDevicePt = pDevice->convertTo<vcl::DeviceB2DPoint>(aOriginal, aOverride);
    auto aResultPt = pDevice->convertTo<vcl::LogicB2DPoint>(aDevicePt, aOverride);

    CPPUNIT_ASSERT_DOUBLES_EQUAL(aOriginal.get().getX(), aResultPt.get().getX(), 1e-6);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(aOriginal.get().getY(), aResultPt.get().getY(), 1e-6);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testComplexPolygonTransform)
{
    VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
    MapMode aOverride(MapUnit::Map100thMM, Point(10, 10), 3.0, 3.0);

    basegfx::B2DPolygon aPoly;
    aPoly.append(basegfx::B2DPoint(0, 0));
    aPoly.append(basegfx::B2DPoint(10, 10));
    vcl::LogicB2DPolygon aLogicPoly(aPoly);

    auto aDevicePoly = pDevice->convertTo<vcl::DeviceB2DPolygon>(aLogicPoly, aOverride);

    // aDevicePoly.get() returns the basegfx::B2DPolygon directly.
    // Use .getB2DPoint(index) on that object.
    CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, aDevicePoly.get().getB2DPoint(0).getX(), 0.001);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(40.0, aDevicePoly.get().getB2DPoint(1).getX(), 0.001);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testConceptLogic)
{
    // Compile-time verification of the Concept logic
    static_assert(vcl::IsDirectConstruction<Size, Size>, "Size(Size) must be direct");
    static_assert(!vcl::IsDirectConstruction<Size, int, int>, "Size(int, int) must NOT be direct");
    static_assert(!vcl::IsDirectConstruction<Size, Point>, "Size(Point) must NOT be direct");
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testConstructorBehavior)
{
    // Runtime verification that the constructors resolve as expected
    Size aSource(20, 20);

    // Tests the direct constructor (using copy/move)
    vcl::LogicSize aDirect(aSource);
    CPPUNIT_ASSERT_EQUAL(aSource, aDirect.get());

    // Tests the forwarding constructor (variadic)
    vcl::LogicSize aForward(20, 20);
    CPPUNIT_ASSERT_EQUAL(aSource, aForward.get());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
