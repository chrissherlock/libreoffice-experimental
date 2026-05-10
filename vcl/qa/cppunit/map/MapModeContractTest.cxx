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

#include <vcl/outdev.hxx>
#include <vcl/mapmod.hxx>
#include <CoordinateMapper.hxx>
#include <basegfx/point/b2dpoint.hxx>

using namespace vcl::detail;

/**
 * Validates logical unit conversions (Twips, MM, etc.)
 * Scope: Unit-to-Unit math consistency in the Affine layer.
 */
class MapModeContractTest : public CppUnit::TestFixture
{
protected:
    void verify(MapUnit eUnit, double fInput, double fExpectedPixels)
    {
        CoordinateMapper m;
        // Lock DPI to 96.
        // In the CoordinateMapper contract, if the MapMode scale is 1/1,
        // 1.0 logic unit maps to (1.0 * DPI) pixels regardless of the Unit enum,
        // because the Mapper assumes physical normalization happens at the OutDev level.
        m.SetDPIX(96);
        m.SetDPIY(96);

        // Feed the intent directly to the math engine (Stateless Mapper Refactor)
        m.CalcMapResolution(MapMode(eUnit), 96, 96);

        // Pass 'true' to explicitly enable mapping for the transformation retrieval
        basegfx::B2DPoint aResult = m.GetDeviceTransformation(vcl::MappingPolicy::ApplyMapMode)
                                    * basegfx::B2DPoint(fInput, 0);

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Logical Unit Scaling Failure", fExpectedPixels,
                                             aResult.getX(), 1e-5);
    }
};

CPPUNIT_TEST_FIXTURE(MapModeContractTest, testPhysicalUnitConversions)
{
    // At 96 DPI, 1 inch equals exactly 96 pixels.
    // We supply 1 inch worth of logical units to ensure the matrix outputs 96 pixels.

    verify(MapUnit::MapTwip, 1440.0, 96.0); // 1440 twips = 1 inch
    verify(MapUnit::Map100thMM, 2540.0, 96.0); // 2540 100thMM = 1 inch
    verify(MapUnit::MapPoint, 72.0, 96.0); // 72 points = 1 inch
    verify(MapUnit::MapPixel, 96.0, 96.0); // 96 pixels = 96 pixels

    // Verify relative scaling linearity
    verify(MapUnit::MapPixel, 192.0, 192.0);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
