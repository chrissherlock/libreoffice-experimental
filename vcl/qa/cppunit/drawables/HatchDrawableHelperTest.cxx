/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_features.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <tools/gen.hxx>
#include <tools/line.hxx>
#include <tools/poly.hxx>

#include <vcl/hatch.hxx>
#include <vcl/virdev.hxx>

#include <drawables/HatchDrawableHelper.hxx>

namespace
{
class HatchDrawableHelperTest : public CppUnit::TestFixture
{
    void testGetHatchIncrement();
    void testGetPt1();
    void testGetPt2();
    void testEndPt1();
    void testGetHatchLinePoints_using_square();
    void testGetHatchLinePoints_using_polypolygon();

    CPPUNIT_TEST_SUITE(HatchDrawableHelperTest);
    CPPUNIT_TEST(testGetHatchIncrement);
    CPPUNIT_TEST(testGetPt1);
    CPPUNIT_TEST(testGetPt2);
    CPPUNIT_TEST(testEndPt1);
    CPPUNIT_TEST(testGetHatchLinePoints_using_square);
    CPPUNIT_TEST(testGetHatchLinePoints_using_polypolygon);
    CPPUNIT_TEST_SUITE_END();
};

void HatchDrawableHelperTest::testGetHatchIncrement()
{
    const long DISTANCE = 100;

    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetHatchIncrement: Distance: 100, Angle: 0 failure", Size(0, 100),
                                 vcl::HatchDrawableHelper::GetHatchIncrement(DISTANCE, 0));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetHatchIncrement: Distance: 100, Angle: 900 failure",
                                 Size(100, 0),
                                 vcl::HatchDrawableHelper::GetHatchIncrement(DISTANCE, 900));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetHatchIncrement: Distance: 100, Angle: 450 failure",
                                 Size(0, 141),
                                 vcl::HatchDrawableHelper::GetHatchIncrement(DISTANCE, 450));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetHatchIncrement: Distance: 100, Angle: -450 failure",
                                 Size(0, 114),
                                 vcl::HatchDrawableHelper::GetHatchIncrement(DISTANCE, -450));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetHatchIncrement: Distance: 100, Angle: 150 failure",
                                 Size(0, 104),
                                 vcl::HatchDrawableHelper::GetHatchIncrement(DISTANCE, 150));
}

void HatchDrawableHelperTest::testGetPt1()
{
    const long DISTANCE = 100;
    const tools::Rectangle aTestRect(1000, 1000, 3000, 3000);
    const Point aTestRefPt1(100, 100);
    const Point aTestRefPt2(2000, 2000);

    // 0 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 0, aRefPt(100, 100) failure", Point(1000, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 0, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 0, aRefPt(2000, 2000) failure", Point(1000, 900),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 0, aTestRefPt2));

    // 90 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(1000, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 900, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(900, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 900, aTestRefPt2));

    // 45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(1000, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 450, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(1000, 900),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 450, aTestRefPt2));

    // -45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(949, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, -450, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(934, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, -450, aTestRefPt2));

    // 15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(941, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 150, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(932, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 150, aTestRefPt2));

    // -15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(951, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, -150, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(910, 1000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, -150, aTestRefPt2));

    // 150 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(923, 3000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 1500, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt1: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(932, 3000),
        vcl::HatchDrawableHelper::GetPt1(aTestRect, DISTANCE, 1500, aTestRefPt2));
}

void HatchDrawableHelperTest::testGetPt2()
{
    const long DISTANCE = 100;
    const tools::Rectangle aTestRect(1000, 1000, 3000, 3000);
    const Point aTestRefPt1(100, 100);
    const Point aTestRefPt2(2000, 2000);

    // 0 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 0, aRefPt(100, 100) failure", Point(3000, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 0, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 0, aRefPt(2000, 2000) failure", Point(3000, 900),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 0, aTestRefPt2));

    // 90 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(1000, 3000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 900, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(900, 3000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 900, aTestRefPt2));

    // 45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(3000, -1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 450, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(3000, -1100),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 450, aTestRefPt2));

    // -45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-2759, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, -450, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-2723, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, -450, aTestRefPt2));

    // 15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-6505, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 150, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-6496, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 150, aTestRefPt2));

    // -15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-295, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, -150, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-283, 1000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, -150, aTestRefPt2));

    // 150 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-2490, 3000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 1500, aTestRefPt1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "GetPt2: aTestPt1(100, 100), Angle: 900, aRefPt(100, 100) failure", Point(-2487, 3000),
        vcl::HatchDrawableHelper::GetPt2(aTestRect, DISTANCE, 1500, aTestRefPt2));
}

void HatchDrawableHelperTest::testEndPt1()
{
    const tools::Rectangle aTestRect(1000, 1000, 3000, 3000);

    // 0 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: 0 failure", Point(1000, 3000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, 0));

    // 90 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: 900 failure", Point(3000, 1000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, 900));

    // 45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: 450 failure", Point(1000, 5000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, 450));

    // -45 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: -450 failure", Point(6668, 1000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, -450));

    // 15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: 15 failure", Point(79377, 1000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, 15));

    // -15 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: -15 failure", Point(3646, 1000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, -15));

    // 150 degree test
    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetEndPt1: Angle: 150 failure", Point(10464, 1000),
                                 vcl::HatchDrawableHelper::GetEndPt1(aTestRect, 150));
}

void HatchDrawableHelperTest::testGetHatchLinePoints_using_square()
{
    const tools::Rectangle aTestRect(Point(20, 20), Point(180, 180));
    const tools::Polygon aTestPolygon(aTestRect);
    const tools::PolyPolygon aTestPolyPolygon(aTestPolygon);
    const tools::Line aTestLine1(Point(0, 20), Point(200, 20));
    const tools::Line aTestLine2(Point(0, 40), Point(200, 40));

    const long EXPECTEDLINE1COUNT = 0;
    const long EXPECTEDLINE2COUNT = 2;

    vcl::PointArray aPtBuffer1
        = vcl::HatchDrawableHelper::GetHatchLinePoints(aTestLine1, aTestPolyPolygon);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 1 hatch line point count failed", EXPECTEDLINE1COUNT,
                                 aPtBuffer1.mnCountPoints);

    vcl::PointArray aPtBuffer2
        = vcl::HatchDrawableHelper::GetHatchLinePoints(aTestLine2, aTestPolyPolygon);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2 hatch line point count failed", EXPECTEDLINE2COUNT,
                                 aPtBuffer2.mnCountPoints);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 1 wrong", Point(20, 40), aPtBuffer2.mpPoints[0]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 2 wrong", Point(180, 40), aPtBuffer2.mpPoints[1]);
}

void HatchDrawableHelperTest::testGetHatchLinePoints_using_polypolygon()
{
    tools::Rectangle aTestRect(Point(20, 20), Point(180, 180));
    tools::Polygon aTestPolygon1(aTestRect);

    tools::Rectangle aTest45Rect(Point(20, 20), Point(180, 180));
    tools::Polygon aTestPolygon2(aTest45Rect);
    aTestPolygon2.Rotate(Point(100, 100), 450);

    tools::PolyPolygon aTestPolyPolygon;
    aTestPolyPolygon.Insert(aTestPolygon1);
    aTestPolyPolygon.Insert(aTestPolygon2);

    const tools::Line aTestLine1(Point(0, 20), Point(200, 20));
    const tools::Line aTestLine2(Point(0, 40), Point(200, 40));

    const long EXPECTEDLINE1COUNT = 2;
    const long EXPECTEDLINE2COUNT = 4;

    vcl::PointArray aPtBuffer1
        = vcl::HatchDrawableHelper::GetHatchLinePoints(aTestLine1, aTestPolyPolygon);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 1 hatch line point count failed", EXPECTEDLINE1COUNT,
                                 aPtBuffer1.mnCountPoints);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 1, point 1 wrong", Point(67, 20), aPtBuffer1.mpPoints[0]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 1, point 2 wrong", Point(133, 20), aPtBuffer1.mpPoints[1]);

    vcl::PointArray aPtBuffer2
        = vcl::HatchDrawableHelper::GetHatchLinePoints(aTestLine2, aTestPolyPolygon);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2 hatch line point count failed", EXPECTEDLINE2COUNT,
                                 aPtBuffer2.mnCountPoints);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 1 wrong", Point(20, 40), aPtBuffer2.mpPoints[0]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 1 wrong", Point(47, 40), aPtBuffer2.mpPoints[1]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 2 wrong", Point(153, 40), aPtBuffer2.mpPoints[2]);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Line 2, point 2 wrong", Point(180, 40), aPtBuffer2.mpPoints[3]);
}
} // namespace

CPPUNIT_TEST_SUITE_REGISTRATION(HatchDrawableHelperTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
