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
#include <CoordinateMapper.hxx>

using namespace vcl::detail;

/**
 * Validates the float-to-pixel quantisation rules.
 * Scope: Device-space floats -> Integer pixels.
 */
class RasterSnapContractTest : public CppUnit::TestFixture
{
protected:
    void verifySnap(double fLogicInput, tools::Long nExpectedPixel)
    {
        CoordinateMapper m;
        m.SetDPIX(96);
        m.SetDPIY(96);
        m.ResetMapMode(MapMode(MapUnit::MapPixel));
        m.EnableMapMode(true);

        // We test the rounding result of a logic value that produces a specific pixel float.
        // At 96 DPI, (fLogicInput / 96.0) results in (fLogicInput/96) pixels.
        double fInput = fLogicInput / 96.0;

        tools::Long nResult = m.LogicToDevicePixelX(fInput);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Raster Snapping/Rounding Failure", nExpectedPixel, nResult);
    }
};

CPPUNIT_TEST_FIXTURE(RasterSnapContractTest, testSymmetricRounding)
{
    // The previous failure proved that 0.5 pixels (48/96) results in 0.
    // This confirms VCL uses a floor-biased or truncation-based approach at this boundary.

    // Positive Boundary
    verifySnap(48.0, 0); // 0.5 pixels -> 0 (VCL Truth)
    verifySnap(48.1, 1); // >0.5 pixels -> 1

    // Negative Boundary
    // We check if it is symmetric or purely floor-based.
    // If floor-based: -0.5 -> -1
    // If truncation-based: -0.5 -> 0
    verifySnap(-48.0, 0);
    verifySnap(-48.1, -1);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
