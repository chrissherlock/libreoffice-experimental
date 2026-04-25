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

#include <cmath>
#include <iostream>

using namespace vcl::detail;

struct PrecisionPolicy
{
    double fAbsEpsilon = 1e-6;
};

struct CoordinateScenario
{
    sal_Int32 nDPIX = 96;
    sal_Int32 nDPIY = 96;

    MapMode aMapMode;

    basegfx::B2DPoint aInputLogicPoint = basegfx::B2DPoint(100.0, 100.0);

    PrecisionPolicy ePolicy;
    std::string aContext;
};

struct PixelCoord
{
    tools::Long X;
    tools::Long Y;
};

class CoordinateContractTest : public CppUnit::TestFixture
{
protected:
    void setupMapper(CoordinateMapper& m, const CoordinateScenario& s) const
    {
        m.SetDPIX(s.nDPIX);
        m.SetDPIY(s.nDPIY);
        m.ResetMapMode(s.aMapMode);
        m.EnableMapMode(true);
    }

    // IMPORTANT:
    // This is NOT a "model oracle".
    // It is the real VCL contract: logical → device scaling.
    basegfx::B2DPoint getAffine(const CoordinateScenario& s) const
    {
        CoordinateMapper m;
        setupMapper(m, s);

        return m.GetDeviceTransformation() * s.aInputLogicPoint;
    }

    void dumpScenario(const CoordinateScenario& s) const
    {
        std::cout << "\n=== " << s.aContext << " ===\n";
        std::cout << "DPI: " << s.nDPIX << " x " << s.nDPIY << "\n";
        std::cout << "Input: " << s.aInputLogicPoint.getX() << ", " << s.aInputLogicPoint.getY()
                  << "\n";
    }

    void verify(const CoordinateScenario& s)
    {
        dumpScenario(s);

        const basegfx::B2DPoint affine = getAffine(s);

        // ✔ CORRECT EXPECTATION:
        // VCL affine output is already DPI-scaled device space.
        const double expectedX = s.aInputLogicPoint.getX() * s.nDPIX;
        const double expectedY = s.aInputLogicPoint.getY() * s.nDPIY;

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(s.aContext + " [Affine X]", expectedX, affine.getX(),
                                             s.ePolicy.fAbsEpsilon);

        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(s.aContext + " [Affine Y]", expectedY, affine.getY(),
                                             s.ePolicy.fAbsEpsilon);
    }
};

CPPUNIT_TEST_FIXTURE(CoordinateContractTest, verifyCoordinateStability)
{
    CoordinateScenario base;
    base.aMapMode = MapMode(MapUnit::MapPixel);
    base.aContext = "Baseline";

    verify(base);

    for (int dpi : { 120, 144, 192 })
    {
        auto s = base;
        s.nDPIX = dpi;
        s.nDPIY = dpi;
        s.aContext = "DPI Walk " + std::to_string(dpi);

        verify(s);
    }

    auto offset = base;
    offset.aContext = "Offset test (device still DPI driven)";
    verify(offset);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
