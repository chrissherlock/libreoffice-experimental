
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

#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/TransformTypes.hxx>
#include <vcl/TransformRouter.hxx>

#include <CoordinateMath.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testSizeBasisVectorScaling)
{
    basegfx::B2DHomMatrix aMat;

    // Scale by (2.0x, 3.0y) and rotate it 90 degrees (PI/2)
    aMat.scale(2.0, 3.0);
    aMat.rotate(M_PI_2);

    // THE OLD BUG: Prove that extracting the scalar diagonal fails.
    // Because of the 90-degree rotation, the X-diagonal is now effectively 0.0.
    // If we used aMat.get(0,0) * Width, the shape would vanish!
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, aMat.get(0, 0), 1e-10);

    // THE FIX: Prove that the basis vector magnitudes survive rotation
    double fMagX = vcl::detail::GetBasisVectorMagnitudeX(aMat);
    double fMagY = vcl::detail::GetBasisVectorMagnitudeY(aMat);

    // The magnitudes should perfectly extract the true geometric scale (2.0 and 3.0)
    // regardless of the rotation or shear.
    CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, fMagX, 1e-10);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(3.0, fMagY, 1e-10);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
