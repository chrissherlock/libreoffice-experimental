/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <tools/gen.hxx>
#include <test/bootstrapfixture.hxx>
#include <test/outputdevice.hxx>

#include <CoordinateMapper.hxx>

class CoordinateMapperTest : public test::BootstrapFixture
{
public:
    CoordinateMapperTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(CoordinateMapperTest, testMirrorDevicePixelPoint)
{
    CoordinateMapper aMapper;

    // Setup standard OutputDevice-like metrics
    aMapper.SetOutputWidthPixel(100);
    aMapper.SetDeviceOriginX(10); // Sets mnOutOffX

    const tools::Long nFrameWidth = 200;

    // Base point to test (Y should NEVER change during X-mirroring)
    const Point aBasePt(20, 50);

    // Case 1: No Mirroring (!bRTL, !bAntiparallel)
    {
        Point aPt = aBasePt;
        aMapper.MirrorDevicePixelPoint(aPt, nFrameWidth, false, false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("X should not change", tools::Long(20), aPt.X());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Y should not change", tools::Long(50), aPt.Y());
    }

    // Case 2: Standard RTL Mirroring (bRTL, !bAntiparallel)
    {
        Point aPt = aBasePt;
        aMapper.MirrorDevicePixelPoint(aPt, nFrameWidth, true, false);
        // Expected X: nFrameWidth - 1 - x  =>  200 - 1 - 20 = 179
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Standard RTL failure", tools::Long(179), aPt.X());
        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPt.Y());
    }

    // Case 3: Antiparallel LTR Window (!bRTL, bAntiparallel)
    {
        Point aPt = aBasePt;
        aMapper.MirrorDevicePixelPoint(aPt, nFrameWidth, false, true);
        // devX = OutOffX = 10
        // Expected X: OutputWidth - (x - devX) + OutOffX - 1
        //           : 100 - (20 - 10) + 10 - 1 = 100 - 10 + 9 = 99
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Antiparallel LTR failure", tools::Long(99), aPt.X());
        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPt.Y());
    }

    // Case 4: Antiparallel RTL Window (bRTL, bAntiparallel)
    {
        Point aPt = aBasePt;
        aMapper.MirrorDevicePixelPoint(aPt, nFrameWidth, true, true);
        // devX = nFrameWidth - OutputWidth - OutOffX = 200 - 100 - 10 = 90
        // Expected X: devX + (x - OutOffX) = 90 + (20 - 10) = 100
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Antiparallel RTL failure", tools::Long(100), aPt.X());
        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPt.Y());
    }

    // Case 5: Edge Case - 0 Frame Width
    {
        Point aPt = aBasePt;
        // Should trigger early exit and not mutate the point
        aMapper.MirrorDevicePixelPoint(aPt, 0, true, false);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Zero framewidth should early exit", tools::Long(20), aPt.X());
        CPPUNIT_ASSERT_EQUAL(tools::Long(50), aPt.Y());
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
