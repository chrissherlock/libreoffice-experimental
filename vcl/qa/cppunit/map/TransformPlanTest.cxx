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
#include <cppunit/plugin/TestPlugIn.h>

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/gen.hxx>

#include <vcl/TransformPlan.hxx>
#include <vcl/GeometryAdapter.hxx>
#include <vcl/TransformTypes.hxx>

#include <TransformCompiler.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testMonadicCompositionMath)
{
    // Plan A: Scale by 2x
    basegfx::B2DHomMatrix aMatA;
    aMatA.scale(2.0, 2.0);
    vcl::TransformPlan aPlanA = vcl::TransformCompiler::Compile(aMatA);

    // Plan B: Translate by 10 units
    basegfx::B2DHomMatrix aMatB;
    aMatB.translate(10.0, 10.0);
    vcl::TransformPlan aPlanB = vcl::TransformCompiler::Compile(aMatB);

    // Composite: Scale THEN Translate (Left-to-Right execution)
    vcl::TransformPlan aComposite = aPlanA.compose(aPlanB);

    Point aStartPt(5, 5);

    // Manual sequential apply
    Point aSequentialResult = aPlanB.apply(aPlanA.apply(aStartPt));

    // Fluent composite apply
    Point aFluentResult = aComposite.apply(aStartPt);

    // Both should equal (5*2)+10 = 20
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Sequential math failed", tools::Long(20), aSequentialResult.X());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Fluent composition math failed", tools::Long(20),
                                 aFluentResult.X());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Composition broke symmetry", aSequentialResult, aFluentResult);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testContractIntersection)
{
    // Identity matrix perfectly preserves Axis Alignment
    basegfx::B2DHomMatrix aMatA;
    vcl::TransformPlan aPlanA = vcl::TransformCompiler::Compile(aMatA);

    // Rotation matrix utterly destroys Axis Alignment
    basegfx::B2DHomMatrix aMatB;
    aMatB.rotate(0.785); // ~45 degrees
    vcl::TransformPlan aPlanB = vcl::TransformCompiler::Compile(aMatB);

    CPPUNIT_ASSERT(aPlanA.PreservesAxisAlignment());
    CPPUNIT_ASSERT(!aPlanB.PreservesAxisAlignment());

    // Compose them together
    vcl::TransformPlan aComposite = aPlanA.compose(aPlanB);

    // The composite plan must safely downgrade its contract to the lowest common denominator
    CPPUNIT_ASSERT_MESSAGE("Contract intersection failed to drop axis alignment!",
                           !aComposite.PreservesAxisAlignment());

    // It should also downgrade the execution mode to AffineFallback to prevent fast-path math errors
    CPPUNIT_ASSERT_EQUAL(TransformMode::AffineFallback, aComposite.meMode);
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testDeviceTranslationAccumulation)
{
    // Testing the discrete pixel offset accumulation bypassing the matrix
    vcl::TransformPlan aPlanA;
    aPlanA.meMode = TransformMode::Translation;
    aPlanA.mnDeviceTx = 15;
    aPlanA.mnDeviceTy = 30;

    vcl::TransformPlan aPlanB;
    aPlanB.meMode = TransformMode::Translation;
    aPlanB.mnDeviceTx = -5;
    aPlanB.mnDeviceTy = 10;

    vcl::TransformPlan aComposite = aPlanA.compose(aPlanB);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Device X accumulation failed", tools::Long(10),
                                 aComposite.mnDeviceTx);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Device Y accumulation failed", tools::Long(40),
                                 aComposite.mnDeviceTy);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
