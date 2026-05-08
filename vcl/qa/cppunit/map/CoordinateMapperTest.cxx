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

#include <tools/gen.hxx>

#include <vcl/mapmod.hxx>

#include <CoordinateMapper.hxx>

namespace
{
CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testRoundingSymmetry)
{
    CoordinateMapper aMapper;
    // Verify lcl_RoundToLong uses symmetric rounding (away from zero).
    // Setup: 1 unit logic * 1.5 (UI Scale) = 1.5 units pixel
    aMapper.SetDPIX(100);
    aMapper.SetDPIY(100);
    aMapper.SetDPIScalePercentage(150);
    aMapper.SetMapMode(MapMode(MapUnit::MapPixel));

    // Positive case: 1.5 rounds away from zero to 2
    Point aPos = aMapper.LogicToDevicePixel(Point(1, 1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Symmetric positive rounding failed", tools::Long(2), aPos.X());

    // Negative case: -1.5 rounds away from zero to -2
    Point aNeg = aMapper.LogicToDevicePixel(Point(-1, -1));
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Symmetric negative rounding failed", tools::Long(-2), aNeg.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMatrixEngine)
{
    CoordinateMapper aMapper;

    // Setup HiDPI (200% scale) with 100thmm units
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);
    aMapper.SetDPIScalePercentage(200);
    aMapper.SetMapMode(MapMode(MapUnit::Map100thMM));

    // Scale calculation: (96 DPI / 2540 units per inch) * 2.0 UI scale = 0.07559055
    // 1000 units * 0.07559 = 75.59 -> Round to 76
    Point aPt = aMapper.LogicToDevicePixel(Point(1000, 1000));
    CPPUNIT_ASSERT_EQUAL(tools::Long(76), aPt.X());

    // The "Offset Sandwich" (The 13 vs 10 Regression Guard)
    // LogicOffset is applied BEFORE scale. WindowOffset is applied AFTER scale.
    aMapper.SetLogicOffset(Size(100, 100)); // 100 logic units
    aMapper.SetWindowOffset(Size(50, 50)); // 50 pixels

    // Math: round((1000 + 100) * 0.07559) + 50
    // round(1100 * 0.07559) + 50 = round(83.149) + 50 = 83 + 50 = 133
    aPt = aMapper.LogicToDevicePixel(Point(1000, 1000));
    CPPUNIT_ASSERT_EQUAL(tools::Long(133), aPt.X());

    // ====================================================================
    // Geometry Consistency (The B2DRange AABB Adapter)
    // ====================================================================
    tools::Rectangle aLogicRect(100, 100, 1100, 1100);
    tools::Rectangle aPixelRect = aMapper.LogicToDevicePixel(aLogicRect);

    // Left Edge: 100 -> +100 logic offset -> 200 * 0.07559 = 15.11 -> round(15) + 50 = 65
    CPPUNIT_ASSERT_EQUAL(tools::Long(65), aPixelRect.Left());
    CPPUNIT_ASSERT_EQUAL(tools::Long(65), aPixelRect.Top());

    // Right Edge (AABB Continuous Math):
    // 1. Inclusive logic bound 1100 means continuous math bound is 1101.
    // 2. 1101 + 100 logic offset = 1201.
    // 3. 1201 * 0.07559 = 90.78 -> round(91) + 50 = 141.
    // 4. Subtract 1 to return to inclusive integer bounds -> 140.
    CPPUNIT_ASSERT_EQUAL(tools::Long(140), aPixelRect.Right());
    CPPUNIT_ASSERT_EQUAL(tools::Long(140), aPixelRect.Bottom());

    // ====================================================================
    // Inverse Consistency (Mathematical Round-tripping)
    // ====================================================================
    // We test the matrices directly to bypass ANY integer wrapper APIs.
    basegfx::B2DPoint aInput(1000.0, 1000.0);

    // Forward journey via the actual LogicToDevice matrix
    basegfx::B2DPoint aDevice = aInput;
    aDevice *= aMapper.GetLogicToDeviceMatrix(true);

    // Backward journey via the actual DeviceToLogic matrix
    basegfx::B2DPoint aBackToLogic = aDevice;
    aBackToLogic *= aMapper.GetDeviceToLogicMatrix(true);

    // Standard epsilon check for double precision parity
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Matrix inversion drift detected", aInput.getX(),
                                         aBackToLogic.getX(), 1e-9);
    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Matrix inversion drift detected", aInput.getY(),
                                         aBackToLogic.getY(), 1e-9);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testBMapFalseSemantics)
{
    CoordinateMapper aMapper;
    aMapper.SetDPIX(96);
    aMapper.SetDPIY(96);
    aMapper.SetMapMode(MapMode(MapUnit::Map100thMM));

    // Set heavy logical offsets (should be ignored)
    aMapper.SetLogicOffset(Size(5000, 5000));

    // Set pixel view/device offsets (should be applied)
    aMapper.SetWindowOffset(Size(50, 50));
    aMapper.SetDeviceToWindowOffsetX(10);
    aMapper.SetDeviceToWindowOffsetY(10);

    // Request translation with bMap = false
    Point aPt(10, 10);
    Point aResult = aMapper.LogicToDevicePixel(aPt, false);

    // Expect: (10 + 0 logical scaling/offset) + 50 window + 10 device = 70
    CPPUNIT_ASSERT_EQUAL_MESSAGE("bMap=false must ignore logic but apply pixel offsets",
                                 tools::Long(70), aResult.X());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testCompiledTransformStability)
{
    CoordinateMapper aMapper;
    aMapper.SetMapMode(MapMode(MapUnit::MapPixel));
    aMapper.SetWindowOffset(Size(10, 10));

    CompiledTransform aTransform = aMapper.Compile(true);

    aMapper.SetWindowOffset(Size(999, 999));

    basegfx::B2DPoint aPt(0, 0);
    aPt *= aTransform.GetMatrix();

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Compiled transform isolation failed under mutation", 10.0,
                                         aPt.getX(), 1e-9);

    CompiledTransform aNewTransform = aMapper.Compile(true);
    basegfx::B2DPoint aPtNew(0, 0);
    aPtNew *= aNewTransform.GetMatrix();

    CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("New compilation failed to capture state mutation", 999.0,
                                         aPtNew.getX(), 1e-9);
}

} // namespace

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
