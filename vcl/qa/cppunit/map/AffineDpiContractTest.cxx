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
#include <basegfx/point/b2dpoint.hxx>

using namespace vcl::detail;

/**
 * Validates the pure floating-point transformation matrix.
 * Scope: Logic -> DPI-scaled Device Space.
 */
class AffineDpiContractTest : public CppUnit::TestFixture
{
protected:
    void verify(sal_Int32 nDPI, double fLogicVal)
    {
        CoordinateMapper m;
        m.SetDPIX(nDPI);
        m.SetDPIY(nDPI);

        // Feed the intent directly to the math engine (Stateless Mapper Refactor)
        m.CalcMapResolution(MapMode(MapUnit::MapPixel), nDPI, nDPI);

        basegfx::B2DPoint aInput(fLogicVal, fLogicVal);

        // Pass 'true' to explicitly enable mapping for the transformation retrieval
        basegfx::B2DPoint aResult
            = m.GetDeviceTransformation(vcl::MappingPolicy::ApplyMapMode) * aInput;

        // The Affine Contract: At MapPixel, 1 logic unit = 1 pixel.
        // The scale factor is exactly 1.0 regardless of the underlying DPI.
        double fExpected = fLogicVal;
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE("Affine DPI Scaling Failure", fExpected,
                                             aResult.getX(), 1e-7);
    }
};

CPPUNIT_TEST_FIXTURE(AffineDpiContractTest, testDpiScaling)
{
    verify(96, 100.0);
    verify(144, 100.0);
    verify(192, 1.0);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
