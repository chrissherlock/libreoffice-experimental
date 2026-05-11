
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

#include <tools/gen.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <GeometryAdapter.hxx>
#include <TransformCompiler.hxx>
#include <TransformTypes.hxx>

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testHairlineClampSize)
{
    // MUST ENABLE STRICT MODE to test the collapse prevention logic!
    vcl::GeometryAdapter::SetStrictSubPixelPrecision(true);

    // Create a matrix that drastically scales down.
    // e.g., 15 twips * 0.02 = 0.3 pixels (which would round to 0 without clamping)
    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02);

    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    // Standard text caret thickness (15 twips)
    Size aHairline(15, 240);
    Size aResult = vcl::GeometryAdapter::Apply(aPlan, aHairline);

    // Assert that the dimension did not collapse to 0
    CPPUNIT_ASSERT_MESSAGE("Hairline width collapsed to 0!", aResult.Width() != 0);

    // It should be strictly clamped to 1 pixel
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.Width());

    // The height (240 * 0.02 = 4.8) should round normally to 5
    CPPUNIT_ASSERT_EQUAL(tools::Long(5), aResult.Height());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testHairlineClampRectangle)
{
    // MUST ENABLE STRICT MODE to test the collapse prevention logic!
    vcl::GeometryAdapter::SetStrictSubPixelPrecision(true);

    // Extreme scale-down matrix
    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02);

    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    // Simulate a caret positioned at (100, 100) with a 15-unit width
    // Rectangle constructor is (Left, Top, Right, Bottom)
    // Note: VCL rectangles are inclusive, so width 15 is Right = Left + 14
    tools::Rectangle aCaretRect(100, 100, 114, 339);

    tools::Rectangle aResult = vcl::GeometryAdapter::Apply(aPlan, aCaretRect);

    // Without clamping, a 0.3px width flags as a "ghost line" and becomes empty
    CPPUNIT_ASSERT_MESSAGE("Rectangle collapsed to an Empty state!", !aResult.IsEmpty());

    // Ensure the resulting physical footprint is exactly 1 pixel wide
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.GetWidth());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testLegacyCollapse)
{
    // Ensure we are in Legacy Mode
    vcl::GeometryAdapter::SetStrictSubPixelPrecision(false);

    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02); // Will scale 15 units to 0.3 pixels
    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    tools::Rectangle aTinyRect(Point(0, 0), Size(15, 15));
    tools::Rectangle aResult = vcl::GeometryAdapter::Apply(aPlan, aTinyRect);

    // Legacy behavior: collapse to empty
    CPPUNIT_ASSERT_MESSAGE("Legacy mode failed to collapse tiny rectangle to empty",
                           aResult.IsEmpty());
}

CPPUNIT_TEST_FIXTURE(CppUnit::TestFixture, testStrictClamp)
{
    // Ensure we are in Strict Mode
    vcl::GeometryAdapter::SetStrictSubPixelPrecision(true);

    basegfx::B2DHomMatrix aMat;
    aMat.scale(0.02, 0.02);
    vcl::TransformPlan aPlan = vcl::TransformCompiler::Compile(aMat);

    tools::Rectangle aTinyRect(Point(0, 0), Size(15, 15));
    tools::Rectangle aResult = vcl::GeometryAdapter::Apply(aPlan, aTinyRect);

    // Strict behavior: clamp to 1px
    CPPUNIT_ASSERT_MESSAGE("Strict mode failed to clamp tiny rectangle to 1px", !aResult.IsEmpty());
    CPPUNIT_ASSERT_EQUAL(tools::Long(1), aResult.GetWidth());
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
