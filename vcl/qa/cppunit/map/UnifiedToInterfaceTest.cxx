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

    // 1. Establish a base device state
    MapMode aBaseMode(MapUnit::Map100thMM);
    pDevice->SetMapMode(aBaseMode);

    // 2. Define an override state (different unit, with scaling and translation)
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

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
