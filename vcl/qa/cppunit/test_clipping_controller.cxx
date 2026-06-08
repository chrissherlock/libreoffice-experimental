
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

#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/utils/canvastools.hxx>

#include <vcl/ClippingController.hxx>
#include <vcl/CoordinateMapper.hxx>

namespace
{
class ClippingControllerTest : public CppUnit::TestFixture
{
    // You can add setUp() and tearDown() here if your controller
    // eventually requires complex initialization.
};

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testFastPathRectangular)
{
    vcl::ClippingController aClipper;

    // Create a 100x100 rectangle
    basegfx::B2DPolyPolygon aRect(
        basegfx::utils::createPolygonFromRect(basegfx::B2DRange(0, 0, 100, 100)));

    aClipper.ModifyClip(aRect, vcl::ClipOp::Set);

    CPPUNIT_ASSERT(aClipper.HasClipRegion());

    // Test culling
    basegfx::B2DRange aInside(10, 10, 50, 50);
    basegfx::B2DRange aOutside(150, 150, 200, 200);

    CPPUNIT_ASSERT(!aClipper.IsCulled(aInside)); // Should NOT be culled
    CPPUNIT_ASSERT(aClipper.IsCulled(aOutside)); // MUST be culled
}

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testComplexIntersection)
{
    vcl::ClippingController aClipper;

    basegfx::B2DPolyPolygon aRect1(
        basegfx::utils::createPolygonFromRect(basegfx::B2DRange(0, 0, 100, 100)));
    basegfx::B2DPolyPolygon aRect2(
        basegfx::utils::createPolygonFromRect(basegfx::B2DRange(50, 50, 150, 150)));

    aClipper.ModifyClip(aRect1, vcl::ClipOp::Set);
    aClipper.ModifyClip(aRect2, vcl::ClipOp::Intersect);

    // The intersection should be a 50x50 rect from (50,50) to (100,100)
    basegfx::B2DRange aExpected(50, 50, 100, 100);

    // Note: using the correct .getB2DRange() method
    basegfx::B2DRange aResult = aClipper.GetLogicClip().getB2DRange();

    CPPUNIT_ASSERT_EQUAL(aExpected.getMinX(), aResult.getMinX());
    CPPUNIT_ASSERT_EQUAL(aExpected.getMaxX(), aResult.getMaxX());
    CPPUNIT_ASSERT_EQUAL(aExpected.getMinY(), aResult.getMinY());
    CPPUNIT_ASSERT_EQUAL(aExpected.getMaxY(), aResult.getMaxY());
}

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testEpochSyncGuard)
{
    vcl::ClippingController aClipper;
    CoordinateMapper aMapper;

    basegfx::B2DPolyPolygon aRect(
        basegfx::utils::createPolygonFromRect(basegfx::B2DRange(0, 0, 100, 100)));
    aClipper.ModifyClip(aRect, vcl::ClipOp::Set);

    int syncCount = 0;
    auto syncFunc = [&](const basegfx::B2DPolyPolygon&) { syncCount++; };

    // Call sync three times sequentially.
    // Because the CoordinateMapper's semantic key hasn't changed,
    // the hardware callback should only fire the very first time.
    aClipper.Synchronize(aMapper, syncFunc);
    aClipper.Synchronize(aMapper, syncFunc);
    aClipper.Synchronize(aMapper, syncFunc);

    CPPUNIT_ASSERT_EQUAL(1, syncCount);
}

} // anonymous namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
