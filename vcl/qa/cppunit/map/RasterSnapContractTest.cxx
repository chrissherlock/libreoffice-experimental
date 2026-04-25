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
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <cmath>
#include <iostream>

using namespace vcl::detail;

class RasterSnapContractTest : public CppUnit::TestFixture
{
protected:
    void setupIdentity(CoordinateMapper& m) const
    {
        m.SetDPIX(96);
        m.SetDPIY(96);

        MapMode mm(MapUnit::MapPixel);
        mm.SetScaleX(1.0);
        mm.SetScaleY(1.0);

        m.ResetMapMode(mm);
        m.EnableMapMode(true);
    }

    tools::Long snap(double v) const
    {
        // Match typical VCL symmetric rounding
        return (v >= 0.0) ? static_cast<tools::Long>(std::floor(v + 0.5))
                          : static_cast<tools::Long>(std::ceil(v - 0.5));
    }
};

CPPUNIT_TEST_FIXTURE(RasterSnapContractTest, testIdentityFastPath)
{
    CoordinateMapper m;
    setupIdentity(m);

    auto mat = m.GetDeviceTransformation();

    double logic = 1.0;

    double affine = (mat * basegfx::B2DPoint(logic, 0)).getX();
    tools::Long device = m.LogicToDevicePixelX(logic);

    std::cout << "\n[Identity]\n";
    std::cout << "Affine: " << affine << "\n";
    std::cout << "Device: " << device << "\n";

    CPPUNIT_ASSERT_DOUBLES_EQUAL(96.0, affine, 1e-7);
    CPPUNIT_ASSERT_EQUAL(tools::Long(96), device);
}

CPPUNIT_TEST_FIXTURE(RasterSnapContractTest, testNoZeroCollapse)
{
    CoordinateMapper m;
    setupIdentity(m);

    // Use valid integer logic inputs
    CPPUNIT_ASSERT(m.LogicToDevicePixelX(1) != 0);
    CPPUNIT_ASSERT(m.LogicToDevicePixelX(2) != 0);
}

CPPUNIT_TEST_FIXTURE(RasterSnapContractTest, testAffineMatchesScalarStability)
{
    CoordinateMapper m;
    setupIdentity(m);

    auto mat = m.GetDeviceTransformation();

    for (tools::Long logic : { 1, 2, 5, 10 })
    {
        double affine = (mat * basegfx::B2DPoint(logic, 0)).getX();
        tools::Long device = m.LogicToDevicePixelX(logic);

        tools::Long expected = static_cast<tools::Long>(std::round(affine));

        CPPUNIT_ASSERT_EQUAL(expected, device);
    }
}

CPPUNIT_TEST_FIXTURE(RasterSnapContractTest, testNonIdentityScale)
{
    CoordinateMapper m;

    m.SetDPIX(96);
    m.SetDPIY(96);

    MapMode mm(MapUnit::MapPixel);
    mm.SetScaleX(2.0); // double scaling
    mm.SetScaleY(2.0);

    m.ResetMapMode(mm);
    m.EnableMapMode(true);

    auto mat = m.GetDeviceTransformation();

    double logic = 1.0;

    double affine = (mat * basegfx::B2DPoint(logic, 0)).getX();
    tools::Long device = m.LogicToDevicePixelX(logic);

    std::cout << "\n[Scaled]\n";
    std::cout << "Affine: " << affine << "\n";
    std::cout << "Device: " << device << "\n";

    CPPUNIT_ASSERT_DOUBLES_EQUAL(96.0, affine, 1e-7);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
